#include "win/equip_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/char_status.hpp"
#include "core/equip_window.hpp"
#include "core/hotbar_panel.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/head_picture.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::equipwin {
namespace {

namespace ew = uagfx::equipwin;

// uaRO.exe's: the windows' manager's equipment window (UI window 10, its class's vtable, its SendMsg FUN_00470f90 -
// message 6 with 0x87 its close button - and its drop FUN_0046f550), a window's shown flag; the item card's window
// (0xC, its message 0x18 an ITEM_INFO), MakeWindow; the game mode's SendMsg: 0x14 wear (the bag's place, where),
// 100 the ammo, 0x15 take off (the slot's number), 0x16 drag a worn thing (where it is worn)
constexpr uint32_t kWindowMgr = 0x727ED8, kEquipAt = kWindowMgr + 0x94, kEquipClass = 0x698668;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kEquipSendMsg = 0x470F90, kDropSlot = 0x2C;
constexpr uint32_t kEquipDrop = 0x46F550, kMakeWindow = 0x4D42D0;
constexpr int kButtonMsg = 6, kCloseButton = 0x87, kItemCard = 0xC, kShowItem = 0x18;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kWearMsg = 0x14, kAmmoMsg = 100, kTakeOffMsg = 0x15, kDragWornMsg = 0x16;
// The bag (win/item_window's): the session's list, a node's ITEM_INFO at +8, 72 bytes
constexpr uint32_t kBag = 0x771148 + 0xF1C, kInfo = 8;
constexpr size_t kInfoSize = 0x48, kMostNodes = 256;
// The character: its job, sex, hair style and colour (the character block's)
constexpr uint32_t kJob = 0x771F8C, kSex = 0x772DEC, kHead = 0x7718F4, kHairColour = 0x771910;
constexpr ULONGLONG kReadMs = 200;

using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
using DropFn = void(__attribute__((thiscall)) *)(uint32_t self, int x, int y, uint32_t drag);

struct Thing {  // a thing of the bag as the game keeps it
    uint32_t info = 0;
    int index = 0, id = 0, type = 0, count = 0, refine = 0;
    uint32_t location = 0, worn = 0;
    int cards[4] = {};
    bool identified = true;
    std::string name;
};

struct State {
    WindowShadow shadow;
    bool on = false, shown = false, hid = false, stateRead = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    float fx = 0.5f, fy = 0.22f;
    unsigned codePage = 0;
    std::vector<Thing> bag;
    uint32_t slotInfo[ew::kSlots] = {};  // each slot's worn thing's ITEM_INFO (its card)
    ew::Data data;
    std::vector<int> pickBag;  // the picks' places in `bag`
    ULONGLONG readAt = 0;
    ew::Show show;
    ew::Layout layout;
    std::string key, portraitKey;
    panel::Picture picture, portrait;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool moving = false;
    float grabX = 0, grabY = 0;
    int told = 0, pressesTold = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t rd32(const uint8_t* p, size_t at) {
    return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16 | uint32_t(p[at + 3]) << 24;
}

uint32_t gameWindow() {
    const uint32_t w = read32(kEquipAt);
    return w && read32(w) == kEquipClass ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.hid = false;
        return;
    }
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0) && !s.hid) {
            LOG_INFO("equip: the game's equipment window hidden for «Спорядження» (its shown flag 0)");
            s.hid = true;
        }
    } else if (s.hid) {
        write32(window + kShown, 1);
        s.hid = false;
        LOG_INFO("equip: the game's equipment window back");
    }
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    float x = 0, y = 0;
    if (readTextFile(layout::join(gameDir(), layout::kEquipState), text) &&
        std::sscanf(text.c_str(), "at %f %f", &x, &y) == 2 && x >= 0 && x <= 1 && y >= 0 && y <= 1) {
        s.fx = x;
        s.fy = y;
    }
}

void keepState(const State& s) {
    char text[48];
    std::snprintf(text, sizeof text, "at %.4f %.4f\n", s.fx, s.fy);
    const std::string path = layout::join(gameDir(), layout::kEquipState);
    if (!writeFileAtomic(path, std::string(text))) LOG_INFO("equip: %s cannot be written", path.c_str());
}

std::string nameOf(const State& s, int id, bool identified) {
    const std::string key = std::to_string(id);
    std::string words;
    if (!identified) {
        const auto& unknown = clienttables::unknownNames();
        const auto u = unknown.find(key);
        if (u != unknown.end()) words = u->second;
    }
    if (words.empty()) {
        const auto& names = clienttables::itemNames();
        const auto w = names.find(key);
        if (w != names.end()) words = w->second;
    }
    return words.empty() ? "#" + key : utf8Of(widened(hotbar::spaced(words), s.codePage));
}

const panel::Picture* iconOf(const State& s, int id) {
    const auto& files = clienttables::itemFiles();
    const auto f = files.find(std::to_string(id));
    return f == files.end() ? nullptr : clienttables::icon(f->second, ew::iconSide(s.scale), 4 * s.scale);
}

void readBag(State& s) {
    s.bag.clear();
    const uint32_t head = read32(kBag);
    if (!head) return;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kInfoSize] = {};
        if (!readGame(node + kInfo, raw, sizeof raw)) break;
        const uint32_t index = rd32(raw, 0x08), ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
        if (!index || !ptr || !len || len > 16) continue;
        char digits[17] = {};
        if (!readGame(ptr, digits, len)) continue;
        Thing t;
        t.info = node + kInfo;
        t.index = int(index);
        t.id = std::atoi(digits);
        t.type = int(rd32(raw, 0x00));
        t.location = rd32(raw, 0x04);
        t.worn = rd32(raw, 0x0C);
        t.count = int(rd32(raw, 0x10));
        for (int c = 0; c < 4; ++c) t.cards[c] = int(rd32(raw, 0x1C + size_t(c) * 4));
        t.identified = raw[0x3C] != 0;
        const int refine = int(rd32(raw, 0x40));
        t.refine = refine > 0 && refine <= 20 ? refine : 0;
        s.bag.push_back(t);
    }
}

bool isAmmo(int type) { return type == 10 || type == 16 || type == 17; }

void readData(State& s) {
    readBag(s);
    ew::Data& d = s.data;
    for (ew::Worn& w : d.slots) w = ew::Worn{};
    for (uint32_t& i : s.slotInfo) i = 0;
    const auto refs = reference::index();
    for (const Thing& t : s.bag) {
        if (!t.worn) continue;
        const std::vector<int> slots = ew::slotsOf(t.worn);
        for (size_t n = 0; n < slots.size(); ++n) {
            ew::Worn& w = d.slots[slots[n]];
            w.index = t.index;
            w.id = t.id;
            w.refine = t.refine;
            w.count = t.count;
            w.location = t.worn;
            for (int c = 0; c < 4; ++c) w.cards[c] = t.cards[c];
            w.identified = t.identified;
            w.spill = n > 0;
            w.name = nameOf(s, t.id, t.identified);
            w.icon = iconOf(s, t.id);
            if (refs) {
                const auto it = refs->items.find(t.id);
                if (it != refs->items.end()) w.slots = std::clamp(it->second.slots, 0, 4);
            }
            s.slotInfo[slots[n]] = t.info;
        }
    }
    const std::string own = readOwnName();
    d.who = own.empty() ? "" : utf8Of(widened(own, s.codePage));
    const char* job = charblock::jobName(int(read32(kJob)));
    d.job = job ? job : "";
    charstatus::Status st;
    d.stats.clear();
    if (readOwnStatus(st)) {
        d.baseLevel = st.baseLevel;
        d.jobLevel = st.jobLevel;
        const auto plus = [](int a, int b) { return std::to_string(a) + " + " + std::to_string(b); };
        const std::string matk = std::to_string(st.matkMin) + "~" + std::to_string(st.matkMax);
        d.stats = {{"ATK", plus(st.atk, st.atk2)}, {"MATK", matk},
                   {"DEF", plus(st.def, st.def2)}, {"MDEF", plus(st.mdef, st.mdef2)},
                   {"HIT", std::to_string(st.hit)}, {"FLEE", plus(st.flee, st.flee2)}};
    }
    const int side = int(std::lround(96 * s.scale));  // the portrait, made again when the head or the size changes
    const int sex = int(read32(kSex)) != 0 ? 1 : 0, head = int(read32(kHead)), colour = int(read32(kHairColour));
    const std::string key = std::to_string(sex) + ':' + std::to_string(head) + ':' + std::to_string(colour) + ':' +
                            std::to_string(side);
    if (key != s.portraitKey) {
        s.portraitKey = key;
        if (!headpicture::make(sex, head, colour, side, s.portrait)) s.portrait = panel::Picture{};
    }
    d.portrait = s.portrait.width > 0 ? &s.portrait : nullptr;
}

void readPicks(State& s, int slot) {  // what of the bag fits the slot: identified, not worn
    s.data.picks.clear();
    s.pickBag.clear();
    for (size_t i = 0; i < s.bag.size(); ++i) {
        const Thing& t = s.bag[i];
        if (t.worn || !t.identified || !ew::fits(t.location, slot)) continue;
        if ((slot == ew::Ammo) != isAmmo(t.type)) continue;
        ew::Pick p;
        p.index = t.index;
        p.id = t.id;
        p.refine = t.refine;
        p.location = t.location;
        p.name = nameOf(s, t.id, t.identified);
        p.icon = iconOf(s, t.id);
        s.data.picks.push_back(p);
        s.pickBag.push_back(int(i));
    }
}

ew::Texts texts() {
    ew::Texts t;
    t.title = say(settings::Text::EquipTitle);
    t.hotkey = "Alt+Q";
    const settings::Text labels[ew::kSlots] = {
        settings::Text::EquipHeadMid, settings::Text::EquipHeadLow, settings::Text::EquipArmor,
        settings::Text::EquipGarment, settings::Text::EquipAccessory, settings::Text::EquipHeadTop,
        settings::Text::EquipWeapon,  settings::Text::EquipShield,  settings::Text::EquipShoes,
        settings::Text::EquipAccessory, settings::Text::EquipAmmo};
    for (int i = 0; i < ew::kSlots; ++i) t.labels[i] = say(labels[i]);
    t.empty = "\xE2\x80\x94";
    t.cards = say(settings::Text::EquipCards);
    t.drag = say(settings::Text::EquipDrag);
    t.level = say(settings::Text::EquipLevel);
    t.pickTitle = say(settings::Text::EquipPickTitle);
    t.pickNone = say(settings::Text::EquipPickNone);
    t.off = say(settings::Text::EquipPadOff);
    t.put = say(settings::Text::EquipPadPut);
    t.change = say(settings::Text::EquipPadChange);
    t.card = say(settings::Text::EquipPadCard);
    t.close = say(settings::Text::SkillsPadClose);
    t.back = say(settings::Text::EquipPadBack);
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

std::vector<Words> gdiWords(const std::vector<ew::Words>& in) {
    std::vector<Words> out;
    for (const ew::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ew::Align::Right    ? DT_RIGHT
                                 : w.align == ew::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("«Спорядження»", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    ew::paint(s.picture, s.layout, s.data, s.show);
    drawWords(s.picture, gdiWords(ew::words(s.layout, s.data, s.show, texts(), measure)));
    s.upload = true;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Equip, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Equip, nullptr, nullptr, false);
    s.shown = false;
    s.moving = false;
    s.show.picking = false;
}

bool modeMsg(int msg, int a, int b, int c) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, c);
    return true;
}

void closeGameWindow() {
    const uint32_t window = gameWindow();
    if (window && read32(read32(window) + kSendMsgSlot) == kEquipSendMsg)
        reinterpret_cast<SendMsgFn>(uintptr_t(kEquipSendMsg))(window, 0, kButtonMsg, kCloseButton, 0, 0);
}

void takeOff(State& s, int slot, const char* by) {
    if (slot < 0 || slot >= ew::kSlots) return;
    const ew::Worn& w = s.data.slots[slot];
    if (w.index <= 0) return;
    const int number = ew::bitIndex(ew::kBits[slot]);
    const bool sent = number >= 0 && modeMsg(kTakeOffMsg, number, 0, 0);
    s.readAt = 0;
    if (s.told++ < 20)
        LOG_INFO("equip: %s takes off #%d from the slot %d %s", by, w.id, number,
                 sent ? "(the game mode's 0x15, as its own window)" : "- not sent");
}

void wear(State& s, int pick, const char* by) {
    if (pick < 0 || pick >= int(s.pickBag.size())) return;
    const Thing& t = s.bag[size_t(s.pickBag[size_t(pick)])];
    const bool ammo = isAmmo(t.type);
    const bool sent = modeMsg(ammo ? kAmmoMsg : kWearMsg, t.index, int(t.location), 0);
    s.readAt = 0;
    s.show.picking = false;
    if (s.told++ < 20)
        LOG_INFO("equip: %s puts on #%d (the bag's %d, at 0x%X) %s", by, t.id, t.index, t.location,
                 sent ? (ammo ? "(the game mode's 100)" : "(the game mode's 0x14)") : "- not sent");
}

bool card(const State& s, int slot) {
    if (slot < 0 || slot >= ew::kSlots || !s.slotInfo[slot]) return false;
    const uint32_t window = reinterpret_cast<MakeFn>(uintptr_t(kMakeWindow))(kWindowMgr, kItemCard);
    const uint32_t send = window ? read32(read32(window) + kSendMsgSlot) : 0;
    if (!send) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kShowItem, int(s.slotInfo[slot]), 0, 0);
    return true;
}

void openPicks(State& s, int slot) {
    readPicks(s, slot);
    s.show.picking = true;
    s.show.pickSlot = slot;
    s.show.pickFocus = 0;
    if (s.told++ < 20) LOG_INFO("equip: what fits the slot %d - %zu thing(s) of the bag", slot, s.data.picks.size());
}

// The controller's steps round the slots: up and down a column, across at the same row, the ammo under the last row
int stepFrom(int slot, PadCommand c) {
    if (slot == ew::Ammo) {
        if (c == PadCommand::Up) return ew::AccL;
        if (c == PadCommand::Left) return ew::AccL;
        if (c == PadCommand::Right) return ew::AccR;
        return slot;
    }
    const bool l = slot <= ew::AccL;
    const int row = l ? slot : slot - 5;
    switch (c) {
        case PadCommand::Up: return row > 0 ? slot - 1 : slot;
        case PadCommand::Down: return row < 4 ? slot + 1 : ew::Ammo;
        case PadCommand::Left: return l ? slot : slot - 5;
        case PadCommand::Right: return l ? slot + 5 : slot;
        default: return slot;
    }
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("equip: [interface] equip = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameWindow() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    readState(s);
    const float scale = uistyle::readingScale(k);
    const bool fresh = scale != s.scale || codePage != s.codePage;
    s.k = float(k);
    s.scale = scale;
    s.codePage = codePage;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    if (fresh || now - s.readAt >= kReadMs) {
        s.readAt = now;
        readData(s);
        if (s.show.picking) readPicks(s, s.show.pickSlot);
    }
    const uapad::runtime::Look pad = uapad::runtime::look();
    s.show.pad = pad.shown;
    s.show.sony = pad.sony;
    if (s.show.pad && (s.show.focus < 0 || s.show.focus >= ew::kSlots)) s.show.focus = ew::Weapon;
    if (s.show.picking)
        s.show.pickFocus = std::clamp(s.show.pickFocus, 0, std::max(0, int(s.data.picks.size()) - 1));
    const ew::Texts t = texts();
    s.layout = ew::layout(0, 0, scale, s.data, s.show, t, measure);
    s.layout.x = int(std::lround(s.fx * float(std::max(0, targetWidth - s.layout.w))));
    s.layout.y = int(std::lround(s.fy * float(std::max(0, targetHeight - s.layout.h))));
    s.show.hoverSlot = s.show.hoverPick = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy) && !s.moving) {
        const ew::Hit h = ew::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == ew::Part::Slot) s.show.hoverSlot = h.index;
        if (h.part == ew::Part::Pick) s.show.hoverPick = h.index;
        s.show.hoverClose = h.part == ew::Part::Close;
    }
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(s.show.hoverSlot) + ',' + std::to_string(s.show.hoverPick) + ',' +
                      std::to_string(s.show.focus) + ',' + std::to_string(s.show.pickFocus) +
                      (s.show.picking ? "p" + std::to_string(s.show.pickSlot) : "") + (s.show.hoverClose ? "x" : "") +
                      (s.show.pad ? (s.show.sony ? "S" : "X") : "") + ';' + s.data.who + ';' +
                      std::to_string(s.data.baseLevel) + '/' + std::to_string(s.data.jobLevel) + ';' +
                      (s.data.portrait ? "f" : "");
    for (const ew::Worn& w : s.data.slots)
        key += std::to_string(w.id) + '+' + std::to_string(w.refine) + 'x' + std::to_string(w.count) + ':' +
               std::to_string(w.cards[0]) + '.' + std::to_string(w.cards[1]) + '.' + std::to_string(w.cards[2]) + '.' +
               std::to_string(w.cards[3]) + (w.icon ? "i" : "") + ',';
    for (const auto& [name, value] : s.data.stats) key += value + ',';
    for (const ew::Pick& p : s.data.picks) key += std::to_string(p.index) + (p.icon ? "i" : "") + ',';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {
        s.key = drawn;
        render(s);
    }
    if (!s.shown && s.told < 20) {
        ++s.told;
        int worn = 0;
        for (const ew::Worn& w : s.data.slots) worn += w.index > 0 && !w.spill;
        LOG_INFO("equip: «Спорядження» open - %d thing(s) worn, %zu in the bag", worn, s.bag.size());
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool shown() { return state().shown; }

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    if (s.show.focus < 0 || s.show.focus >= ew::kSlots) s.show.focus = ew::Weapon;
    const int slot = s.show.focus;
    const bool worn = s.data.slots[slot].index > 0;
    if (s.show.picking) {
        const int count = int(s.data.picks.size());
        switch (c) {
            case PadCommand::Up: s.show.pickFocus = count ? (s.show.pickFocus + count - 1) % count : 0; break;
            case PadCommand::Down: s.show.pickFocus = count ? (s.show.pickFocus + 1) % count : 0; break;
            case PadCommand::Put: wear(s, s.show.pickFocus, "the controller's A"); break;
            case PadCommand::Back: s.show.picking = false; break;
            case PadCommand::Close: closeGameWindow(); break;
            default: break;
        }
        s.key.clear();
        return;
    }
    switch (c) {
        case PadCommand::Up:
        case PadCommand::Down:
        case PadCommand::Left:
        case PadCommand::Right: s.show.focus = stepFrom(slot, c); break;
        case PadCommand::Put:
            if (worn)
                takeOff(s, slot, "the controller's A");
            else
                openPicks(s, slot);
            break;
        case PadCommand::Change: openPicks(s, slot); break;
        case PadCommand::Card:
            if (worn && card(s, slot) && s.told++ < 20) LOG_INFO("equip: the controller's Y - the game's item card");
            break;
        case PadCommand::Back:
        case PadCommand::Close: closeGameWindow(); break;
    }
    s.key.clear();
}

bool drop(int x, int y, uint32_t drag) {
    State& s = state();
    if (!s.shown || s.k <= 0) return false;
    const float tx = float(x) * s.k - float(s.layout.x), ty = float(y) * s.k - float(s.layout.y);
    if (!s.layout.glass.contains(tx, ty)) return false;
    const uint32_t window = gameWindow();
    if (!window || read32(read32(window) + kDropSlot) != kEquipDrop) return false;
    reinterpret_cast<DropFn>(uintptr_t(kEquipDrop))(window, 0, 0, drag);
    if (s.told++ < 20) LOG_INFO("equip: a drag let go over «Спорядження» - given to the game's own window's drop");
    s.readAt = 0;
    return true;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    (void)wp;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    bool fromGame = false;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, &fromGame)) return false;
    if (s.moving && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {
        const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
        s.fx = std::clamp((tx - s.grabX) / float(roomX), 0.0f, 1.0f);
        s.fy = std::clamp((ty - s.grabY) / float(roomY), 0.0f, 1.0f);
        if (m == WM_LBUTTONUP) {
            s.moving = false;
            keepState(s);
        }
        result = 0;
        return true;
    }
    const ew::Hit h = ew::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN) && s.pressesTold < 8) {
        ++s.pressesTold;
        LOG_INFO("equip: a press (0x%X) at the window's (%d, %d) - the target's (%.0f, %.0f) by the %s; «Спорядження» "
                 "at %d, %d %dx%d: part %d, index %d", m, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), tx, ty,
                 fromGame ? "game's cursor" : "message's point", s.layout.x, s.layout.y, s.layout.w, s.layout.h,
                 int(h.part), h.index);
    }
    if (h.part == ew::Part::None || m == WM_MOUSEMOVE) return false;
    result = 0;
    if (m == WM_RBUTTONDOWN && h.part == ew::Part::Slot) {  // its card, as a right click in the game's window
        if (s.data.slots[h.index].index > 0) card(s, h.index);
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    switch (h.part) {
        case ew::Part::Close:
            if (s.told++ < 20) LOG_INFO("equip: its close - the game's own window asked to close (its 0x87)");
            closeGameWindow();
            break;
        case ew::Part::Pick: wear(s, h.index, "a click on the list"); break;
        case ew::Part::Slot: {
            s.show.focus = h.index;
            const ew::Worn& w = s.data.slots[h.index];
            if (w.index <= 0) break;
            if (m == WM_LBUTTONDBLCLK) {
                takeOff(s, h.index, "a double click");
            } else {  // dragged as the game's own window drags a worn thing
                const bool sent = modeMsg(kDragWornMsg, int(w.location), 0, 0);
                if (s.told++ < 20) LOG_INFO("equip: #%d dragged %s", w.id, sent ? "(the game mode's 0x16)" : "- not");
            }
            break;
        }
        case ew::Part::Title:
            s.moving = true;
            s.grabX = tx - float(s.layout.x);
            s.grabY = ty - float(s.layout.y);
            break;
        default:
            break;
    }
    s.key.clear();
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    if (const uint32_t window = gameWindow()) hold(s, window, true);
}

}  // namespace uagfx::win::equipwin
