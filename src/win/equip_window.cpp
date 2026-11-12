#include "win/carry_icon.hpp"
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
#include "core/pad_glyphs.hpp"
#include "core/hotbar_panel.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/window_drag.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/client_tables.hpp"
#include "win/game_memory.hpp"
#include "win/info_card.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "core/equip_sets.hpp"
#include "win/head_picture.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "uikit/equip_live.hpp"
#include "core/mount_icon.hpp"
#include "win/hero_picture.hpp"
#include "win/pack_pictures.hpp"
#include "win/live_part.hpp"
#include "uikit/hits.hpp"
#include "win/ui_sheet.hpp"
#include "win/window_stack.hpp"
#include "win/words.hpp"

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
// 3.0.11-beta.110: a drag let go - the game mode's 0x12 (the cursor's x, y), as its frame sends it when the button
// comes up (as «Речі»'s and «Навички»'s)
constexpr int kDropMsg = 0x12;
// The bag (win/item_window's): the session's list, a node's ITEM_INFO at +8, 72 bytes
constexpr uint32_t kBag = 0x771148 + 0xF1C, kInfo = 8;
constexpr size_t kInfoSize = 0x48, kMostNodes = 256;
// 3.0.11-beta.118 (R-2YRWU2 «1 thing(s) worn» - the arrows alone - and R-8YW96B «0 worn» on a dressed Paladin; the
// owner's «шмоток нету»): the worn gear is not in the bag - the session keeps it apart, an ITEM_INFO for
// each of the ten places by its bit (+0xFC0, 72 bytes apart: FUN_00628e50 at the equipment's list 0x00a4 and at the
// wear's answer 0x00aa; a two-hander or a full helm in each of its places); the arrows worn stay in the bag - their
// place at +0xEEC (0x013c, at the login too). 3.0.11-beta.120 (R-4T7N4D: Sleep Arrow shown while plain arrows were
// worn): the place alone - the login list's worn 0x8000 stays on the first arrows after others are worn
constexpr uint32_t kWorn = 0x771148 + 0xFC0, kAmmoNow = 0x771148 + 0xEEC, kAmmoBit = 0x8000;
constexpr int kWornPlaces = 10;
// The character: its job, sex, hair style and colour (the character block's)
constexpr uint32_t kJob = 0x771F8C, kSex = 0x772DEC, kHead = 0x7718F4, kHairColour = 0x771910;
constexpr ULONGLONG kReadMs = 200;

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
    // 3.0.18-beta.96 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): live - laid out by RmlUi
    LivePart<uikit::equip::Model> live;
    // 3.0.18-beta.154 (the later clients' Equip Switch): the character's sets, their file's character, the steps
    // to put one on (one each 150 ms), the set under the cursor
    equipsets::Set sets[equipsets::kSets];
    std::string setsOf;
    std::vector<equipsets::Step> setQueue;
    ULONGLONG setStepAt = 0;
    int setHot = -1;
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    WindowEdge edge;  // 3.0.11-beta.108: the top of two or more windows (win/window_stack)
    // 3.0.11-beta.110 (the concept «Концепция окон и инвентаря», stage 2): the game's drag of
    // a worn thing begun at the press never saw its button come up (the press and the release are the window's) - a
    // drag of its own now, its icon under the cursor, the game's drag begun and let go only when the button comes up
    // (as «Речі»)
    windrag::Press press;
    panel::Picture dragIcon;
    const panel::Picture* dragOf = nullptr;
    uint32_t dragTexture = 0;
    int dragW = 0, dragH = 0;
    bool dragUpload = false;
    bool on = false, shown = false, hid = false, stateRead = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    float fx = 0.5f, fy = 0.22f;
    unsigned codePage = 0;
    std::vector<Thing> bag;
    std::vector<Thing> worn;  // 3.0.11-beta.118: the worn gear (the session's own places, not the bag)
    uint32_t slotInfo[ew::kSlots] = {};  // each slot's worn thing's ITEM_INFO (its card)
    ew::Data data;
    std::vector<int> pickBag;  // the picks' places in `bag`
    ULONGLONG readAt = 0;
    ew::Show show;
    ew::Layout layout;
    std::string key, portraitKey;
    // 3.0.18-beta.111 (the owner: «из спрайтов, настоящего»): the character in the middle from
    // the game's sprites, the way it faces (◀ ▶), made again when its looks, its way or the size change
    int heroDir = 0;
    heropicture::Looks heroLooks;
    std::string heroKey;
    bool mountHot = false;  // 3.0.18-beta.114: the cursor over the cart / falcon / Peco button
    int turnHot = -1;       // 3.0.18-beta.115: over ◀ (0) or ▶ (1)
    bool mountAsk = false;  // 3.0.18-beta.117: the sign pressed - Yes / No asked
    int askHot = -1;        // over Yes (0) or No (1)
    panel::Picture hero;
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

// An ITEM_INFO at `info` as a thing; false: no thing there
bool thingAt(uint32_t info, Thing& t) {
    uint8_t raw[kInfoSize] = {};
    if (!readGame(info, raw, sizeof raw)) return false;
    const uint32_t index = rd32(raw, 0x08), ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
    if (!index || !ptr || !len || len > 16) return false;
    char digits[17] = {};
    if (!readGame(ptr, digits, len)) return false;
    t = Thing{};
    t.info = info;
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
    return true;
}

void readBag(State& s) {
    s.bag.clear();
    const uint32_t head = read32(kBag);
    if (!head) return;
    const uint32_t ammo = read32(kAmmoNow);
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        Thing t;
        if (!thingAt(node + kInfo, t)) continue;
        t.worn &= ~kAmmoBit;  // 3.0.11-beta.120: the arrows worn by their place alone (the login's flag stays stale)
        if (ammo && uint32_t(t.index) == ammo) t.worn |= kAmmoBit;  // 3.0.11-beta.118: the arrows worn
        s.bag.push_back(t);
    }
}

// 3.0.11-beta.118: the worn gear, from the session's ten places (a thing in two or three of them once)
void readWorn(State& s) {
    s.worn.clear();
    for (int place = 0; place < kWornPlaces; ++place) {
        Thing t;
        if (!thingAt(kWorn + uint32_t(place) * uint32_t(kInfoSize), t) || !t.worn) continue;
        const bool seen = std::any_of(s.worn.begin(), s.worn.end(), [&](const Thing& w) { return w.index == t.index; });
        if (!seen) s.worn.push_back(t);
    }
}

bool isAmmo(int type) { return type == 10 || type == 16 || type == 17; }

void readData(State& s) {
    readBag(s);
    readWorn(s);
    ew::Data& d = s.data;
    for (ew::Worn& w : d.slots) w = ew::Worn{};
    for (uint32_t& i : s.slotInfo) i = 0;
    const auto refs = reference::index();
    std::vector<Thing> worn = s.worn;  // the gear, then the arrows worn (in the bag)
    for (const Thing& t : s.bag)
        if (t.worn & kAmmoBit) worn.push_back(t);
    for (const Thing& t : worn) {
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
            if (t.identified) w.name = cardnames::named(w.name, w.cards, clienttables::cardWords());  // .112
            w.element = t.identified ? infocardwin::elementOf(t.id, w.cards) : 0;  // 3.0.18-beta.146
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
    // 3.0.18-beta.111: the character from its sprites (its body by the job it shows, its head, headgear, falcon, cart)
    const heropicture::Looks looks = heropicture::own();
    const int hw = int(std::lround(150 * s.scale)), hh = int(std::lround(210 * s.scale));  // .112: the smaller window
    std::string hk;
    for (const int v : {looks.job, looks.sex, looks.head, looks.colour, looks.cloth, looks.weapon, looks.shield,
                        looks.low, looks.top, looks.middle,
                        int(looks.options), s.heroDir, hw, hh, packpictures::iconsStamp()})
        hk += std::to_string(v) + ':';
    if (hk != s.heroKey) {
        s.heroKey = hk;
        std::string from;
        if (!heropicture::make(looks, s.heroDir, hw, hh, s.hero, &from)) s.hero = panel::Picture{};
        if (s.told++ < 40) LOG_INFO("equip: the character from its sprites - %s", from.c_str());
    }
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

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Equip, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Equip, nullptr, nullptr, false);
    if (s.press.dragging) windrag::clear();  // 3.0.11-beta.110
    s.press = windrag::Press{};
    s.shown = false;
    s.moving = false;
    s.show.picking = false;
}

bool modeMsg(int msg, int a, int b, int c);

// 3.0.11-beta.110: a worn thing dragged and let go - the game's drag of it begun (0x16, as its own window's press) and
// let go at once where the game's cursor is (0x12): onto «Речі» (taken off), a window of the game's or the ground
const char* dropAt(State& s, int slot) {
    if (slot < 0 || slot >= ew::kSlots || s.data.slots[slot].index <= 0) return "not dropped (nothing worn there)";
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy)) return "not dropped (no cursor)";
    if (!modeMsg(kDragWornMsg, int(s.data.slots[slot].location), 0, 0)) return "not dropped (the game mode not found)";
    modeMsg(kDropMsg, cx, cy, 0);
    return "let go through the game's own drop";
}

// 3.0.11-beta.110: the slots a thing of the bag being dragged fits (lit in gold)
uint32_t dropFits() {
    const windrag::Thing& t = windrag::now();
    if (t.kind != windrag::Kind::Item || !t.location) return 0;
    uint32_t bits = 0;
    for (int i = 0; i < ew::kSlots; ++i)
        if (ew::fits(t.location, i)) bits |= 1u << i;
    return bits;
}

bool modeMsg(int msg, int a, int b, int c) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, c);
    return true;
}

// 3.0.18-beta.114 (the owner: «нету кнопки для анманта пеко итд»): the game's own window's
// one button (0xB6 in its SendMsg FUN_00470f90; its words by FUN_0046ed20) - by the options (0x77191C): a cart
// (FUN_0063b1c0: 0x08 0x80 0x100 0x200 0x400) first, then the falcon (0x10), then the Peco (0x20); a press sends the
// game mode's 0x58, 0x91 or 0x92 (each its 0x012a to the server). The own shop open (0x728028): the game's line 0x20d
// in the chat instead
constexpr uint32_t kOptions = 0x77191C, kShopOpen = 0x728028, kPush = 0x4DB310, kMsgString = 0x504FB0;
constexpr int kCartOffMsg = 0x58, kFalconOffMsg = 0x91, kPecoOffMsg = 0x92, kCloseShopLine = 0x20D;
using PushFn = int(__attribute__((thiscall)) *)(uint32_t manager, int msg, const char* text, int colour, int more);
using MsgStringFn = const char* (*)(int id);

enum class Mount { None, Cart, Falcon, Peco };

Mount mountNow() {
    const uint32_t o = read32(kOptions);
    if (o & (0x08 | 0x80 | 0x100 | 0x200 | 0x400)) return Mount::Cart;
    if (o & 0x10) return Mount::Falcon;
    if (o & 0x20) return Mount::Peco;
    return Mount::None;
}

std::string mountWords(Mount m) {
    return m == Mount::Cart     ? say(settings::Text::EquipCartOff)
           : m == Mount::Falcon ? say(settings::Text::EquipFalconOff)
           : m == Mount::Peco   ? say(settings::Text::EquipPecoOff)
                                : std::string();
}

const char* mountOff(State& s) {
    const Mount m = mountNow();
    if (m == Mount::None) return "nothing to take off";
    if (read32(kShopOpen) != 0) {  // as the game: its chat shown (3), then its line in red
        const PushFn push = reinterpret_cast<PushFn>(uintptr_t(kPush));
        push(kWindowMgr, 3, nullptr, 0, 0);
        const char* line = reinterpret_cast<MsgStringFn>(uintptr_t(kMsgString))(kCloseShopLine);
        if (line) push(kWindowMgr, 1, line, 0xFF, 0);
        return "the own shop open - the game's «close the shop» line";
    }
    const int msg = m == Mount::Cart ? kCartOffMsg : m == Mount::Falcon ? kFalconOffMsg : kPecoOffMsg;
    if (!modeMsg(msg, 0, 0, 0)) return "not sent (the game mode not found)";
    s.key.clear();
    return m == Mount::Cart     ? "the cart off (0x58)"
           : m == Mount::Falcon ? "the falcon let go (0x91)"
                                : "off the Peco (0x92)";
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

// 3.0.18-beta.154: the sets' file - uaro-graphics\equip-sets\<the character's name as hex>.txt, «#1» before each
std::string setsFile(const std::string& who) {
    std::string hex;
    char b[3];
    for (unsigned char c : who) std::snprintf(b, sizeof b, "%02x", c), hex += b;
    return gameDir() + "\\uaro-graphics\\equip-sets\\" + hex + ".txt";
}

void readSets(State& s) {
    const std::string who = readOwnName();
    if (who.empty() || who == s.setsOf) return;
    s.setsOf = who;
    for (equipsets::Set& x : s.sets) x = equipsets::Set{};
    std::string text;
    if (!readTextFile(setsFile(who), text)) return;
    int at = -1;
    std::string part[equipsets::kSets];
    size_t from = 0;
    while (from <= text.size()) {
        size_t end = text.find('\n', from);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(from, end - from);
        from = end + 1;
        if (line.size() >= 2 && line[0] == '#') at = std::atoi(line.c_str() + 1) - 1;
        else if (at >= 0 && at < equipsets::kSets) part[at] += line + "\n";
    }
    for (int i = 0; i < equipsets::kSets; ++i) s.sets[i] = equipsets::Set::parse(part[i]);
}

void keepSet(State& s, int i) {
    s.sets[i] = equipsets::capture(s.data.slots);
    std::string text;
    for (int n = 0; n < equipsets::kSets; ++n) text += "#" + std::to_string(n + 1) + "\n" + s.sets[n].text();
    CreateDirectoryA((gameDir() + "\\uaro-graphics\\equip-sets").c_str(), nullptr);
    writeFileAtomic(setsFile(s.setsOf), text);
    LOG_INFO("equip: the set %d kept - %zu thing(s)", i + 1, s.sets[i].things.size());
    s.key.clear();
}

void putSet(State& s, int i) {
    if (s.sets[i].things.empty()) return;
    std::vector<equipsets::Owned> bag;
    for (const Thing& t : s.bag) {
        equipsets::Owned o;
        o.index = t.index, o.id = t.id, o.refine = t.refine, o.worn = t.worn != 0;
        for (int c = 0; c < 4; ++c) o.cards[c] = t.cards[c];
        bag.push_back(o);
    }
    s.setQueue = equipsets::plan(s.sets[i], bag, s.data.slots);
    s.setStepAt = 0;
    LOG_INFO("equip: the set %d put on - %zu thing(s) to wear", i + 1, s.setQueue.size());
}

void setSteps(State& s) {  // one thing a step, as the game's own window wears them
    if (s.setQueue.empty() || GetTickCount64() < s.setStepAt) return;
    const equipsets::Step st = s.setQueue.front();
    s.setQueue.erase(s.setQueue.begin());
    bool ammo = false;
    for (const Thing& t : s.bag)
        if (t.index == st.index) ammo = isAmmo(t.type);
    modeMsg(ammo ? kAmmoMsg : kWearMsg, st.index, int(st.at), 0);
    s.setStepAt = GetTickCount64() + 150;
    s.readAt = 0;
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

// The controller's steps round the slots - 3.0.18-beta.110: the original window's columns (core equipwin padStep)
int stepFrom(int slot, PadCommand c) {
    const int dx = c == PadCommand::Left ? -1 : c == PadCommand::Right ? 1 : 0;
    const int dy = c == PadCommand::Up ? -1 : c == PadCommand::Down ? 1 : 0;
    return ew::padStep(slot, dx, dy);
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
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
    // 3.0.18-beta.151: times its own size (its corner pulled)
    const float scale = uistyle::readingScale(k) * windowstack::sizeOf(winstack::Win::Equip);
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
    const uapad::runtime::Look pad = uapad::runtime::navLook();  // .210: keys too
    s.show.pad = pad.shown;
    s.show.sony = pad.sony;
    s.show.dropFits = dropFits();  // 3.0.11-beta.110
    if (s.show.pad && (s.show.focus < 0 || s.show.focus >= ew::kSlots)) s.show.focus = ew::Weapon;
    if (s.show.picking)
        s.show.pickFocus = std::clamp(s.show.pickFocus, 0, std::max(0, int(s.data.picks.size()) - 1));
    const ew::Texts t = texts();
    s.layout = ew::layout(0, 0, scale, s.data, s.show, t, measure);
    if (s.live.shown()) uikit::equip::refit(s.live.doc.doc(), s.layout);  // 3.0.18-beta.96: its drawn boxes
    s.layout.x = int(std::lround(s.fx * float(std::max(0, targetWidth - s.layout.w))));
    s.layout.y = int(std::lround(s.fy * float(std::max(0, targetHeight - s.layout.h))));
    s.show.hoverSlot = s.show.hoverPick = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy) && !s.moving) {
        const ew::Hit h = ew::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == ew::Part::Slot) s.show.hoverSlot = h.index;
        if (h.part == ew::Part::Pick) s.show.hoverPick = h.index;
        s.show.hoverClose = h.part == ew::Part::Close;
    }
    s.mountHot = false;
    s.turnHot = -1;
    if (s.live.shown() && !s.moving && readHoverCursor(cx, cy)) {
        const float px = float(cx) * float(k) - float(s.layout.x), py = float(cy) * float(k) - float(s.layout.y);
        for (const uikit::Placed& p : uikit::placesOf(s.live.doc.doc(), "mountoff"))
            s.mountHot = s.mountHot || p.box.contains(px, py);
        s.askHot = -1;
        for (int i = 0; s.mountAsk && i < 2; ++i)  // 3.0.18-beta.117
            for (const uikit::Placed& p : uikit::placesOf(s.live.doc.doc(), i ? "mountno" : "mountyes"))
                if (p.box.contains(px, py)) s.askHot = i;
        for (int i = 0; i < 2; ++i)  // 3.0.18-beta.115 (R-RJBU55 «не активні при наведении»)
            for (const uikit::Placed& p : uikit::placesOf(s.live.doc.doc(), i ? "turnright" : "turnleft"))
                if (p.box.contains(px, py)) s.turnHot = i;
    }
    if (s.mountAsk && (mountNow() == Mount::None || !s.shown)) s.mountAsk = false;  // nothing left to ask about
    std::string key = std::string(s.mountHot ? "m" : "") + (s.mountAsk ? "a" + std::to_string(s.askHot) : "") + "t" +
                      std::to_string(s.turnHot) +
                      std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(s.show.hoverSlot) + ',' + std::to_string(s.show.hoverPick) + ',' +
                      std::to_string(s.show.focus) + ',' + std::to_string(s.show.pickFocus) +
                      (s.show.picking ? "p" + std::to_string(s.show.pickSlot) : "") + (s.show.hoverClose ? "x" : "") +
                      (s.show.pad ? (s.show.sony ? "S" : "X") : "") + 'F' + std::to_string(s.show.dropFits) + ';' +
                      s.data.who + ';' +
                      std::to_string(s.data.baseLevel) + '/' + std::to_string(s.data.jobLevel) + ';' +
                      (s.data.portrait ? "f" : "") + s.heroKey;  // 3.0.18-beta.111
    for (const ew::Worn& w : s.data.slots)
        key += std::to_string(w.id) + '+' + std::to_string(w.refine) + 'x' + std::to_string(w.count) + ':' +
               std::to_string(w.cards[0]) + '.' + std::to_string(w.cards[1]) + '.' + std::to_string(w.cards[2]) + '.' +
               std::to_string(w.cards[3]) + (w.icon ? "i" : "") + ',';
    for (const auto& [name, value] : s.data.stats) key += value + ',';
    for (const ew::Pick& p : s.data.picks) key += std::to_string(p.index) + (p.icon ? "i" : "") + ',';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
        drawn != s.key) {  // 3.0.18-beta.29: uaUI's or the picture
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
    readSets(s);   // 3.0.18-beta.154: the character's sets
    setSteps(s);   // a set being put on, a thing a step
    block(s, game);
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    namespace el = uikit::equip;
    s.sheetShown = s.live.frame("equip", s.shown, s.layout.x, s.layout.y, s.sheetGen, el::rml(), el::bind,
                                [&](el::Model& m) {
                                    m.ground = s.live.ground();
                                    m.turns = s.hero.width > 0;  // 3.0.18-beta.111: its sprites' character
                                    m.mountOff = mountWords(mountNow());  // 3.0.18-beta.114
                                    m.mountHot = s.mountHot;
                                    if (!m.mountOff.empty()) {  // .116: its sign, made for the window's size
                                        const int side = std::max(8, int(std::lround(16 * s.scale)));
                                        m.mountIconSide = float(side);
                                        m.mountIcon = s.live.made("mount" + std::to_string(side), side, side,
                                                                  [&](panel::Picture& p) {
                                                                      mounticon::paint(p.px, p.width, p.height,
                                                                                       0xE6CB8A);
                                                                  });
                                    } else {
                                        m.mountIcon.clear();
                                    }
                                    m.turnHot = s.turnHot;
                                    m.mountAsk = s.mountAsk && !m.mountOff.empty();  // 3.0.18-beta.117
                                    m.askHot = s.askHot;
                                    m.askYes = say(settings::Text::MsgYes);
                                    m.sets.clear();  // 3.0.18-beta.154: the sets «1 2 3»
                                    for (int i = 0; i < equipsets::kSets; ++i)
                                        m.sets.push_back(el::Model::SetV{std::to_string(i + 1),
                                                                         !s.sets[i].things.empty(), i == s.setHot});
                                    m.setsHint = say(settings::Text::EquipSetsHint);
                                    m.askNo = say(settings::Text::MsgNo);
                                    m.hero = m.turns ? s.live.icon(&s.hero) : "";
                                    {  // .113: fitted into the middle's box, its proportions kept
                                        const float bw = 150 * s.scale, bh = 210 * s.scale;
                                        const float k = s.hero.width > 0
                                                            ? std::min({1.0f, bw / float(s.hero.width),
                                                                        bh / float(s.hero.height)})
                                                            : 1.0f;
                                        m.heroW = std::round(float(s.hero.width) * k);
                                        m.heroH = std::round(float(s.hero.height) * k);
                                    }
                                    el::fill(m, s.data, s.show, texts(), s.layout.s,
                                             [&](const panel::Picture* p) { return s.live.icon(p); },
                                             [&](int button, float h, float& w) {
                                                 w = std::ceil(padglyph::widthOf(padglyph::Button(button), h));
                                                 const int iw = std::max(1, int(w)), ih = std::max(1, int(h));
                                                 const std::string key = "pad" + std::to_string(button) +
                                                                         (s.show.sony ? "s" : "x") +
                                                                         std::to_string(iw) + 'x' + std::to_string(ih);
                                                 return s.live.made(key, iw, ih, [&](panel::Picture& p) {
                                                     paintPadBadge(p, button, s.show.sony, h);  // .136
                                                 });
                                             });
                                });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.live.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;
    const bool known = readGameCursor(cx, cy);
    if (known) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                  12 * s.scale, s.scale, s.k);
    const int dragged = s.press.dragging ? s.press.index : -1;  // 3.0.11-beta.110: its icon under the cursor
    const panel::Picture* icon = dragged >= 0 && dragged < ew::kSlots ? s.data.slots[dragged].icon : nullptr;
    if (icon && icon->width > 0 && known) {
        if (icon != s.dragOf) {
            s.dragOf = icon;
            s.dragIcon = *icon;
            s.dragUpload = true;
        }
        const int x = int(std::lround(float(cx) * s.k)) - icon->width / 2;
        const int y = int(std::lround(float(cy) * s.k)) - icon->height / 2;
        // 3.0.18-beta.91: drawn over the windows (uaUI's layer of them comes after the stack)
        carryicon::put(s.dragIcon, s.dragTexture, s.dragW, s.dragH, s.dragUpload, x, y, s.k);
    }
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
    const bool button = mouseButton(m);
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
    if (s.press.index >= 0 && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {  // a worn thing pressed: dragged, or a click
        if (m == WM_MOUSEMOVE) {
            const bool was = s.press.dragging, now = windrag::moved(s.press, tx, ty, s.scale);
            if (now && !was && s.press.index < ew::kSlots) {  // told to «Речі» (its grid lit in gold)
                const ew::Worn& w = s.data.slots[s.press.index];
                windrag::set(windrag::Thing{windrag::Kind::Worn, w.id, uint32_t(w.location), 0});
            }
            return now;
        }
        if (s.press.dragging) windrag::clear();
        const int dragged = windrag::released(s.press);
        if (dragged >= 0 && dragged < ew::kSlots) {
            const int id = s.data.slots[dragged].id;
            const char* did = dropAt(s, dragged);
            s.readAt = 0;
            if (s.told++ < 20) LOG_INFO("equip: #%d dragged and let go - %s", id, did);
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
    if (h.part == ew::Part::None && s.mountAsk && (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN)) {
        s.mountAsk = false;  // 3.0.18-beta.117: a press past the window forgets the question
        s.key.clear();
    }
    if (h.part == ew::Part::None || m == WM_MOUSEMOVE) return false;
    result = 0;
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && s.live.shown()) {  // 3.0.18-beta.111: ◀ ▶ turn it
        // 3.0.18-beta.112 (the owner: «не крутится персонаж стрелками»): by their laid-out
        // boxes - the window is passive (pointer-events none), RmlUi's own point lookup never finds its elements
        const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
        // 3.0.18-beta.117 (the owner: «при анмаунте надо сделать подтверждение да
        // нет, потому что нажав случайно может быть проблема»): the sign
        // asks, Yes does it, No or a press anywhere else forgets the question
        if (s.mountAsk) {
            const auto on = [&](const char* part) {
                for (const uikit::Placed& p : uikit::placesOf(s.live.doc.doc(), part))
                    if (p.box.contains(lx, ly)) return true;
                return false;
            };
            const bool yes = on("mountyes"), no = on("mountno");
            s.mountAsk = false;
            s.key.clear();
            if (yes) {
                const char* did = mountOff(s);
                if (s.told++ < 40) LOG_INFO("equip: the cart, the falcon, the Peco - yes: %s", did);
                return true;
            }
            if (no) {
                if (s.told++ < 40) LOG_INFO("equip: the cart, the falcon, the Peco - no");
                return true;
            }
            if (on("mountoff")) return true;  // the sign again: the question closed
        }
        for (const uikit::Placed& p : uikit::placesOf(s.live.doc.doc(), "set"))  // 3.0.18-beta.154: a set put on
            if (p.box.contains(lx, ly) && p.index >= 0 && p.index < equipsets::kSets) {
                putSet(s, p.index);
                return true;
            }
        for (const uikit::Placed& p : uikit::placesOf(s.live.doc.doc(), "mountoff"))  // 3.0.18-beta.114
            if (p.box.contains(lx, ly)) {
                s.mountAsk = mountNow() != Mount::None;
                s.key.clear();
                if (s.told++ < 40) LOG_INFO("equip: the sign for the cart, the falcon, the Peco - asked");
                return true;
            }
        for (const char* part : {"turnleft", "turnright"})
            for (const uikit::Placed& p : uikit::placesOf(s.live.doc.doc(), part))
                if (p.box.contains(lx, ly)) {
                    s.heroDir = (s.heroDir + (part[4] == 'l' ? 1 : 7)) % 8;
                    s.key.clear();
                    if (s.told++ < 40) LOG_INFO("equip: the character turned to %d", s.heroDir);
                    return true;
                }
    }
    if (m == WM_RBUTTONDOWN && s.live.shown()) {  // 3.0.18-beta.154: a set's right click - what is worn kept in it
        const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
        for (const uikit::Placed& p : uikit::placesOf(s.live.doc.doc(), "set"))
            if (p.box.contains(lx, ly) && p.index >= 0 && p.index < equipsets::kSets && !s.setsOf.empty()) {
                keepSet(s, p.index);
                return true;
            }
    }
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
            } else {  // 3.0.11-beta.110: a drag of its own once it moves (the game's begun and let go on release)
                windrag::pressed(s.press, h.index, tx, ty);
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


bool box(int& x, int& y, int& w, int& h) {  // 3.0.11-beta.109
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

void moveTo(int x, int y, bool keep) {  // 3.0.11-beta.109: question 4 - beside the window standing in its own place
    State& s = state();
    if (!s.shown) return;
    const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
    s.fx = std::clamp(float(x) / float(roomX), 0.0f, 1.0f);
    s.fy = std::clamp(float(y) / float(roomY), 0.0f, 1.0f);
    s.layout.x = x;
    s.layout.y = y;
    if (keep) keepState(s);
}

bool moving() { return state().shown && state().moving; }  // 3.0.11-beta.111

std::vector<uagfx::padcarry::Place> slotPlaces() {  // 3.0.11-beta.121
    const State& s = state();
    std::vector<uagfx::padcarry::Place> out;
    if (!s.shown) return out;
    for (int i = 0; i < ew::kSlots; ++i) {
        const panel::Box& b = s.layout.slots[i];
        if (b.w <= 0) continue;
        uagfx::padcarry::Place p;
        p.to = uagfx::padcarry::To::Slot;
        p.index = i;
        p.x = float(s.layout.x) + b.x;
        p.y = float(s.layout.y) + b.y;
        p.w = b.w;
        p.h = b.h;
        out.push_back(p);
    }
    return out;
}

bool wearThing(int bagIndex, int slot) {  // 3.0.11-beta.121
    State& s = state();
    if (slot < 0 || slot >= ew::kSlots) return false;
    readBag(s);
    for (const Thing& t : s.bag) {
        if (t.index != bagIndex) continue;
        const bool ammo = isAmmo(t.type);
        const uint32_t at = (t.location & ew::kBits[slot]) ? ew::kBits[slot] : t.location;
        const bool sent = modeMsg(ammo ? kAmmoMsg : kWearMsg, t.index, int(at), 0);
        s.readAt = 0;
        if (s.told++ < 20)
            LOG_INFO("equip: the controller's carry puts on #%d (the bag's %d) in the slot %d at 0x%X %s", t.id,
                     t.index, slot, at, sent ? "(as the game's own window)" : "- not sent");
        return sent;
    }
    return false;
}

}  // namespace uagfx::win::equipwin
