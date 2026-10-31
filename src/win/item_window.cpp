#include "win/item_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "core/char_status.hpp"
#include "core/hotbar_panel.hpp"
#include "core/item_window.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/window_drag.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/pad_assign.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win::itemswin {
namespace {

namespace iw = uagfx::itemwin;

// uaRO.exe's: the windows' manager's «Речі» (UI window 8, its class's vtable, its SendMsg FUN_004b9df0 - message 6
// with 0x87 its close button - and its drop FUN_004ba530), a window's shown flag; the item card's window (0xC, its
// message 0x18 an ITEM_INFO), MakeWindow FUN_004d42d0
constexpr uint32_t kWindowMgr = 0x727ED8, kItemsAt = kWindowMgr + 0x8C, kItemsClass = 0x69B8A0;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kItemsSendMsg = 0x4B9DF0, kDropSlot = 0x2C;
constexpr uint32_t kItemsDrop = 0x4BA530, kMakeWindow = 0x4D42D0;
constexpr int kButtonMsg = 6, kCloseButton = 0x87, kItemCard = 0xC, kShowItem = 0x18;
// The game mode (its SendMsg FUN_005707d0 at vtable + 0x14) and what its window sends it (FUN_004b9a10, FUN_004b98b0):
// use, wear, a card into a slot, arrows, a drag begun
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kUseMsg = 0x1D, kWearMsg = 0x14, kCardMsg = 0x88, kAmmoMsg = 100, kDragMsg = 0x11;
// 3.0.11-beta.43: a drag let go - the game mode's message 0x12 (the cursor's x, y), as its frame sends it when the
// button comes up (FUN_00566c80): onto a window of the game's (FUN_004ccef0, the driver's parts through win/
// world_mouse), else the ground's
constexpr int kDropMsg = 0x12;
// The bag: the session's list (a node's next at +0), each node's ITEM_INFO at +8, 72 bytes - its kind +0, where it is
// worn +4, the bag's place +8, worn now +0xC, its count +0x10, its number's words (an std::string: c_str +0x30, its
// length +0x34), identified +0x3C, refine +0x40
constexpr uint32_t kBag = 0x771148 + 0xF1C, kInfo = 8;
constexpr size_t kInfoSize = 0x48, kMostNodes = 256;
constexpr ULONGLONG kReadMs = 200, kConfirmMs = 3000, kBlinkMs = 530;
constexpr ULONGLONG kTipMs = 400;  // 3.0.11-beta.51: a cell's tip after this long pointed at (the concept's)
constexpr ULONGLONG kToastMs = 2000;  // 3.0.11-beta.112: the words of a thing come into the bag
constexpr size_t kMostTyped = 24;

using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
using DropFn = void(__attribute__((thiscall)) *)(uint32_t self, int x, int y, uint32_t drag);

struct State {
    WindowShadow shadow;  // 3.0.11-beta.49
    WindowEdge edge;  // 3.0.11-beta.108: the top of two or more windows (win/window_stack)
    int tipOf = -1;       // 3.0.11-beta.51: the cell pointed at since tipSince
    ULONGLONG tipSince = 0;
    bool on = false, shown = false, hid = false, stateRead = false, skipChar = false;
    unsigned codePage = 0;
    int gainsTold = 0;  // 3.0.11-beta.107: the items come into the bag told the log
    // 3.0.11-beta.112 (question 3 «да»): the things come into the bag (their places) till pointed at - «нове»; the last
    // one's words («+500 Fire Arrow») over the bag, or the screen's lower middle with it closed, for 2 s
    std::set<int> fresh;
    std::string toast, toastPainted;
    ULONGLONG toastUntil = 0;
    float toastK = 0, toastScale = 1;
    int toastW = 0, toastH = 0;  // the target's
    panel::Picture toastPicture;
    uint32_t toastTexture = 0;
    int toastTexW = 0, toastTexH = 0;
    bool toastUpload = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    float fx = 0.08f, fy = 0.23f;  // its place: a share of the free room to its left and above it (the concept's)
    std::vector<iw::Item> items;
    std::vector<uint32_t> infos;  // each item's ITEM_INFO in the game's memory (its card's)
    ULONGLONG readAt = 0;
    iw::Show show;
    iw::Layout layout;
    std::vector<int> shownItems;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool moving = false;
    float grabX = 0, grabY = 0;
    int lastCell = -1, armed = 0;
    ULONGLONG lastClick = 0, armedAt = 0;
    int told = 0;
    // 3.0.11-beta.43 (the owner: «не перетаскивается»): a press on an item - a drag of its own once the cursor moved
    // (its icon under the cursor), the game's own drag begun and let go only when the button comes up: a press the
    // driver's part took is kept from the game, so a game's drag begun at the press never saw its button come up
    int press = -1;  // the pressed item (of `items`), -1 none
    float pressX = 0, pressY = 0;
    bool dragging = false;
    int pressesTold = 0;
    std::vector<std::pair<iw::Flash, ULONGLONG>> flashes;  // 3.0.11-beta.44: the uses answered, when
    int padFocus = 0;      // 3.0.11-beta.91: the controller's frame (of the shown cells)
    bool padNow = false;   // played with the controller: its frame, its buttons in the tip
    panel::Picture dragIcon;  // the dragged item's icon under the cursor
    const panel::Picture* dragOf = nullptr;
    uint32_t dragTexture = 0;
    int dragW = 0, dragH = 0;
    bool dragUpload = false;
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
    uint32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}

uint32_t gameWindow() {
    const uint32_t w = read32(kItemsAt);
    return w && read32(w) == kItemsClass ? w : 0;
}

// Its shown flag held at 0 while the new one shows it, back after
void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.hid = false;
        return;
    }
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0) && !s.hid) {
            LOG_INFO("items: the game's «Речі» window hidden for the new one (its shown flag 0)");
            s.hid = true;
        }
    } else if (s.hid) {
        write32(window + kShown, 1);
        s.hid = false;
        LOG_INFO("items: the game's «Речі» window back");
    }
}

bool modeMsg(int msg, int a, int b) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, 0);
    return true;
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kItemsState), text)) return;
    float x = 0, y = 0;
    int tab = 0;
    for (size_t at = 0; at < text.size();) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        if (std::sscanf(line.c_str(), "at %f %f", &x, &y) == 2 && x >= 0 && x <= 1 && y >= 0 && y <= 1) {
            s.fx = x;
            s.fy = y;
        } else if (std::sscanf(line.c_str(), "tab %d", &tab) == 1 && tab >= 0 && tab < iw::kTabs) {
            s.show.tab = iw::Tab(tab);
        }
    }
}

void keepState(const State& s) {
    char text[64];
    std::snprintf(text, sizeof text, "at %.4f %.4f\ntab %d\n", s.fx, s.fy, int(s.show.tab));
    const std::string path = layout::join(gameDir(), layout::kItemsState);
    if (!writeFileAtomic(path, std::string(text))) LOG_INFO("items: %s cannot be written", path.c_str());
}

// The bag as the session has it, the worn ones out (their window is the gear's)
void readBag(State& s) {
    s.items.clear();
    s.infos.clear();
    const auto& names = clienttables::itemNames();
    const auto& unknown = clienttables::unknownNames();
    const auto& files = clienttables::itemFiles();
    const uint32_t head = read32(kBag);
    if (!head) return;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kInfoSize] = {};
        if (!readGame(node + kInfo, raw, sizeof raw)) break;
        const uint32_t index = rd32(raw, 0x08), worn = rd32(raw, 0x0C), ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
        if (!index || worn || !ptr || !len || len > 16) continue;
        char digits[17] = {};
        if (!readGame(ptr, digits, len)) continue;
        iw::Item it;
        it.index = int(index);
        it.id = std::atoi(digits);
        it.type = int(rd32(raw, 0x00));
        it.location = int(rd32(raw, 0x04));
        it.count = int(rd32(raw, 0x10));
        it.identified = raw[0x3C] != 0;
        const int refine = int(rd32(raw, 0x40));
        it.refine = refine > 0 && refine <= 20 ? refine : 0;
        const std::string id = std::to_string(it.id);
        std::string words;  // an unidentified one's own words, else the item's
        if (!it.identified) {
            const auto u = unknown.find(id);
            if (u != unknown.end()) words = u->second;
        }
        if (words.empty()) {
            const auto w = names.find(id);
            if (w != names.end()) words = w->second;
        }
        it.name = words.empty() ? "#" + id : utf8Of(widened(hotbar::spaced(words), s.codePage));
        const auto f = files.find(id);
        if (f != files.end()) it.icon = clienttables::icon(f->second, iw::iconSide(s.scale), 4 * s.scale);
        s.items.push_back(it);
        s.infos.push_back(node + kInfo);
    }
    for (iw::Item& it : s.items) it.fresh = s.fresh.count(it.index) > 0;  // 3.0.11-beta.112
    for (auto f = s.fresh.begin(); f != s.fresh.end();) {  // gone from the bag (or worn): no longer new
        const int at = *f;
        f = std::any_of(s.items.begin(), s.items.end(), [at](const iw::Item& it) { return it.index == at; })
                ? std::next(f)
                : s.fresh.erase(f);
    }
}

// 3.0.11-beta.91: played with the controller - its buttons in the tip («A: використати · X: призначити · Y: картка»)
iw::Texts texts(bool pad = false);

iw::Texts texts(bool pad) {
    iw::Texts t;
    t.title = say(settings::Text::ItemsTitle);
    t.hotkey = "Alt+E";
    t.tabs[0] = say(settings::Text::ItemsTabItems);
    t.tabs[1] = say(settings::Text::ItemsTabGear);
    t.tabs[2] = say(settings::Text::ItemsTabEtc);
    t.fresh = say(settings::Text::ItemsFresh);  // 3.0.11-beta.112
    t.search = say(settings::Text::ItemsSearch);
    t.weight = say(settings::Text::CharWeight);
    t.use = say(settings::Text::ItemsUse);
    t.wear = say(settings::Text::ItemsWear);
    t.card = say(settings::Text::ItemsCard);
    t.info = say(settings::Text::ItemsInfo);
    if (pad) {
        const std::string a = uapad::buttonLabel(0), x = uapad::buttonLabel(2), y = uapad::buttonLabel(3);
        t.use = a + ": " + say(settings::Text::ItemsPadUse);
        t.wear = a + ": " + say(settings::Text::ItemsPadWear);
        t.card = a + ": " + say(settings::Text::ItemsPadCard);
        std::string more = say(settings::Text::ItemsPadMore);
        if (const size_t at = more.find("{1}"); at != std::string::npos) more.replace(at, 3, x);
        if (const size_t at = more.find("{2}"); at != std::string::npos) more.replace(at, 3, y);
        t.info = more;
    }
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

std::vector<Words> gdiWords(const std::vector<iw::Words>& in) {
    std::vector<Words> out;
    for (const iw::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == iw::Align::Right    ? DT_RIGHT
                                 : w.align == iw::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

int freshSum(const State& s) {  // 3.0.11-beta.112: which ones are new (for the picture's key)
    int sum = 0;
    for (const int f : s.fresh) sum += f;
    return sum;
}

void render(State& s) {
    const uicost::Paint cost("«Речі»", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;  // 3.0.11-beta.57: in the memory it had
    const iw::Texts t = texts(s.padNow);
    iw::paint(s.picture, s.layout, s.items, s.shownItems, s.show, t, measure);
    drawWords(s.picture, gdiWords(iw::words(s.layout, s.items, s.shownItems, s.show, t, measure)));
    s.upload = true;
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the world's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Items, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Items, nullptr, nullptr, false);
    if (s.dragging) windrag::clear();  // 3.0.11-beta.110
    s.shown = false;
    s.moving = false;
}

void place(State& s) {
    const iw::Texts t = texts();
    s.layout = iw::layout(0, 0, s.scale, t, measure);
    const int roomX = std::max(0, s.targetW - s.layout.w), roomY = std::max(0, s.targetH - s.layout.h);
    s.layout.x = int(std::lround(s.fx * float(roomX)));
    s.layout.y = int(std::lround(s.fy * float(roomY)));
}

// The pressed item let go after a drag: the game's drag of it begun (0x11, as its own window's press) and let go at
// once where the cursor is (0x12, as its frame on the button's coming up)
const char* dropAt(const State& s, const iw::Item& it) {
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy)) return "not dropped (no cursor)";
    if (!modeMsg(kDragMsg, it.index, it.count)) return "not dropped (the game mode not found)";
    modeMsg(kDropMsg, cx, cy);
    (void)s;
    return "let go through the game's own drop";
}

// What a double click on it does, as the game's own window: used, worn, a card into a slot, arrows worn; the items
// the game asks about first used on a second double click within 3 s
const char* use(State& s, const iw::Item& it) {
    switch (iw::useOf(it)) {
        case iw::Use::Use: return modeMsg(kUseMsg, it.index, 0) ? "used" : "not used (the game mode not found)";
        case iw::Use::Wear: return modeMsg(kWearMsg, it.index, it.location) ? "worn" : "not worn";
        case iw::Use::Card: return modeMsg(kCardMsg, it.index, 0) ? "a card to put in" : "not put";
        case iw::Use::Ammo: return modeMsg(kAmmoMsg, it.index, it.location) ? "the arrows worn" : "not worn";
        case iw::Use::Confirm: {
            const ULONGLONG now = GetTickCount64();
            if (s.armed == it.index && now - s.armedAt < kConfirmMs) {
                s.armed = 0;
                return modeMsg(kUseMsg, it.index, 0) ? "used (asked twice)" : "not used";
            }
            s.armed = it.index;
            s.armedAt = now;
            return "asked once - again to use";
        }
        case iw::Use::None: break;
    }
    return "nothing to do with it";
}

// A cell's item used as a double click uses it (3.0.11-beta.91: the controller's A too) - its cell answering
void useCell(State& s, int shownIndex, ULONGLONG now, const char* how) {
    if (shownIndex < 0 || shownIndex >= int(s.shownItems.size())) return;
    const iw::Item& it = s.items[size_t(s.shownItems[size_t(shownIndex)])];
    const iw::Use kind = iw::useOf(it);
    const char* did = use(s, it);
    const bool asked = kind != iw::Use::None && !(kind == iw::Use::Confirm && s.armed == it.index);
    if (asked) {  // 3.0.11-beta.44: its cell answers - a ring, its word rising
        iw::Flash f;
        const int cell = shownIndex - s.show.scroll * s.layout.columns;
        if (cell >= 0 && cell < int(s.layout.cells.size())) f.cell = s.layout.cells[size_t(cell)];
        f.icon = it.icon;
        f.label = kind == iw::Use::Wear || kind == iw::Use::Ammo ? say(settings::Text::ItemsWorn)
                  : kind == iw::Use::Card                        ? ""
                                                                 : "\xE2\x88\x92" "1";  // −1
        f.rgb = kind == iw::Use::Wear || kind == iw::Use::Ammo ? 0x5FD08A : 0xF3C653;
        if (f.cell.w > 0) s.flashes.emplace_back(f, now);
    }
    if (s.told++ < 20) LOG_INFO("items: %s on #%d (kind %d) - %s", how, it.id, it.type, did);
    s.readAt = 0;
}

bool card(const State& s, size_t at) {
    if (at >= s.infos.size()) return false;
    const uint32_t window = reinterpret_cast<MakeFn>(uintptr_t(kMakeWindow))(kWindowMgr, kItemCard);
    const uint32_t send = window ? read32(read32(window) + kSendMsgSlot) : 0;
    if (!send) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kShowItem, int(s.infos[at]), 0, 0);
    return true;
}

bool closeGameWindow() {
    const uint32_t window = gameWindow();
    if (!window || read32(read32(window) + kSendMsgSlot) != kItemsSendMsg) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(kItemsSendMsg))(window, 0, kButtonMsg, kCloseButton, 0, 0);
    return true;
}

char letterOf(WPARAM vk, bool shift) {
    if (vk >= 'A' && vk <= 'Z') return char(shift ? vk : vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9' && !shift) return char(vk);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return char('0' + (vk - VK_NUMPAD0));
    switch (vk) {
        case VK_SPACE: return ' ';
        case VK_OEM_MINUS: return shift ? '_' : '-';
        case VK_OEM_7: return '\'';
        case VK_OEM_PERIOD: return '.';
        default: return 0;
    }
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (k > 0 && targetWidth > 0 && targetHeight > 0) {  // 3.0.11-beta.112: a thing come's words, the bag closed too
        s.toastK = float(k);
        s.toastScale = uistyle::readingScale(k);
        s.toastW = targetWidth;
        s.toastH = targetHeight;
    }
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("items: [interface] items = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameWindow() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        s.show.typing = false;
        hide(s);
        return;
    }
    readState(s);
    const float scale = uistyle::readingScale(k);  // 3.0.11-beta.67: [interface] text_size's too
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    if (now - s.readAt >= kReadMs || s.items.empty()) {
        s.readAt = now;
        readBag(s);
        charstatus::Status st;
        if (readOwnStatus(st)) {
            s.show.weight = st.weight;
            s.show.weightMost = st.weightMost;
            s.show.zeny = st.zeny;
        }
    }
    place(s);
    s.show.flashes.clear();  // 3.0.11-beta.44: the uses still answering in their cells
    for (auto f = s.flashes.begin(); f != s.flashes.end();) {
        const float t = float(now - f->second) / iw::kFlashMs;
        if (t >= 1) {
            f = s.flashes.erase(f);
            continue;
        }
        f->first.t = t;
        s.show.flashes.push_back(f->first);
        ++f;
    }
    s.shownItems = iw::shownOf(s.items, s.show.tab, s.show.search);
    s.show.scroll = std::clamp(s.show.scroll, 0, iw::mostScroll(s.layout, s.shownItems.size()));
    s.show.caretOn = (now / kBlinkMs) % 2 == 0;
    s.show.hover = -1;
    s.show.hoverTab = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy) && !s.moving) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        const iw::Hit h = iw::hitTest(s.layout, s.shownItems.size(), s.show.scroll, lx, ly);
        if (h.part == iw::Part::Cell) s.show.hover = h.index;
        if (h.part == iw::Part::Tab) s.show.hoverTab = h.index;
        s.show.hoverClose = h.part == iw::Part::Close;
    }
    if (s.show.hover != s.tipOf) {  // 3.0.11-beta.51: the concept's - a tip after 0.4 s pointed at
        s.tipOf = s.show.hover;
        s.tipSince = now;
    }
    s.show.tip = s.show.hover >= 0 && now - s.tipSince >= kTipMs;
    if (s.show.hover >= 0 && size_t(s.show.hover) < s.shownItems.size()) {  // 3.0.11-beta.112: pointed at - seen
        iw::Item& it = s.items[size_t(s.shownItems[size_t(s.show.hover)])];
        if (it.fresh) {
            it.fresh = false;
            s.fresh.erase(it.index);
        }
    }
    s.padNow = uapad::runtime::look().shown;  // 3.0.11-beta.91: the controller's frame and its tip at once
    s.show.dropping = windrag::now().kind == windrag::Kind::Worn;  // 3.0.11-beta.110: a drop here takes it off
    if (s.padNow && !s.shownItems.empty()) {
        s.padFocus = std::clamp(s.padFocus, 0, int(s.shownItems.size()) - 1);
        s.show.hover = s.padFocus;
        s.show.tip = true;
    }
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(int(s.show.tab)) + ';' + std::to_string(s.show.scroll) + ';' +
                      std::to_string(s.show.hover) + (s.show.tip ? "t" : "") + ',' + std::to_string(s.show.hoverTab) +
                      (s.show.hoverClose ? "x" : "") + ';' + s.show.search + (s.show.typing ? "|t" : "") +
                      (s.show.typing && s.show.caretOn ? "c" : "") + (s.show.dropping ? "D" : "") + 'n' +
                      std::to_string(s.fresh.size()) + '/' + std::to_string(freshSum(s)) + ';' +
                      std::to_string(s.show.flashes.size()) + '@' +
                      std::to_string(s.show.flashes.empty() ? 0 : int(s.show.flashes.front().t * 40)) + ';' +
                      std::to_string(s.show.weight) + '/' +
                      std::to_string(s.show.weightMost) + ';' + std::to_string(s.show.zeny) + ';' +
                      (s.padNow ? "P;" : ";");
    for (const iw::Item& it : s.items)
        key += std::to_string(it.index) + ':' + std::to_string(it.id) + 'x' + std::to_string(it.count) + '+' +
               std::to_string(it.refine) + (it.identified ? "" : "?") + (it.icon ? "i" : "") + ',';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {  // its grounds
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    const bool known = readGameCursor(cx, cy);
    if (known) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;  // 3.0.11-beta.49: its shadow under it
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (windowstack::marked(winstack::Win::Items))  // 3.0.11-beta.108: its gold edge, the top of two or more
        s.edge.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                    s.scale, s.k);
    const panel::Picture* icon = s.dragging && s.press >= 0 && size_t(s.press) < s.items.size()
                                     ? s.items[size_t(s.press)].icon
                                     : nullptr;
    if (icon && icon->width > 0 && known) {  // the dragged item's icon under the cursor
        if (icon != s.dragOf) {
            s.dragOf = icon;
            s.dragIcon = *icon;
            s.dragUpload = true;
        }
        const int x = int(std::lround(float(cx) * s.k)) - icon->width / 2;
        const int y = int(std::lround(float(cy) * s.k)) - icon->height / 2;
        drawPart(record, renderer, s.dragIcon, s.dragTexture, s.dragW, s.dragH, s.dragUpload, x, y, s.k);
    }
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool drop(int x, int y, uint32_t drag) {
    State& s = state();
    if (!s.shown || s.k <= 0) return false;
    const float tx = float(x) * s.k - float(s.layout.x), ty = float(y) * s.k - float(s.layout.y);
    if (!s.layout.glass.contains(tx, ty)) return false;
    const uint32_t window = gameWindow();
    if (!window || read32(read32(window) + kDropSlot) != kItemsDrop) return false;
    reinterpret_cast<DropFn>(uintptr_t(kItemsDrop))(window, 0, 0, drag);
    if (s.told++ < 12) LOG_INFO("items: a drag let go over «Речі» - given to the game's own window's drop");
    s.readAt = 0;
    return true;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    bool fromGame = false;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, &fromGame)) return false;
    if (s.moving) {  // its title dragged: it follows the cursor, inside the screen
        if (m == WM_MOUSEMOVE || m == WM_LBUTTONUP) {
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
    }
    if (s.press >= 0 && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {  // an item pressed: dragged, or a click
        if (size_t(s.press) >= s.items.size()) {
            s.press = -1;
            if (s.dragging) windrag::clear();
            s.dragging = false;
            return false;
        }
        if (m == WM_MOUSEMOVE) {
            if (!s.dragging && std::hypot(tx - s.pressX, ty - s.pressY) > 10 * s.scale) {
                s.dragging = true;  // 3.0.11-beta.110: told to the windows that take it (lit in gold)
                const iw::Item& it = s.items[size_t(s.press)];
                windrag::set(windrag::Thing{windrag::Kind::Item, it.id, uint32_t(it.location), it.type});
            }
            return s.dragging;
        }
        const iw::Item it = s.items[size_t(s.press)];
        const bool dragged = s.dragging;
        if (dragged) windrag::clear();
        s.press = -1;
        s.dragging = false;
        s.key.clear();
        if (dragged) {
            const char* did = dropAt(s, it);
            s.lastCell = -1;
            s.readAt = 0;
            if (s.told++ < 20) LOG_INFO("items: #%d dragged and let go - %s", it.id, did);
        }
        result = 0;
        return true;
    }
    const iw::Hit h = iw::hitTest(s.layout, s.shownItems.size(), s.show.scroll, tx - float(s.layout.x),
                                  ty - float(s.layout.y));
    const bool press = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN;
    if (press && s.pressesTold < 12) {  // where the presses land: the game's cursor against the window
        ++s.pressesTold;
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int cx = -1, cy = -1;
        readGameCursor(cx, cy);
        LOG_INFO("items: a press (0x%X) at the window's (%ld, %ld), the game's cursor (%d, %d) - the target's (%.0f, "
                 "%.0f) by the %s; «Речі» at %d, %d %dx%d: part %d", m, p.x, p.y, cx, cy, tx, ty,
                 fromGame ? "game's cursor" : "message's point", s.layout.x, s.layout.y, s.layout.w, s.layout.h,
                 int(h.part));
    }
    if (h.part == iw::Part::None) {
        if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) s.show.typing = false;  // a click elsewhere: the search lets go
        return false;
    }
    if (m == WM_MOUSEMOVE) return false;  // the hover is the frame's; the game's cursor goes on
    result = 0;
    if (m == WM_MOUSEWHEEL) {
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1;
        s.show.scroll = std::clamp(s.show.scroll + by, 0, iw::mostScroll(s.layout, s.shownItems.size()));
        return true;
    }
    if (m == WM_RBUTTONDOWN && h.part == iw::Part::Cell && h.index < int(s.shownItems.size())) {
        const size_t at = size_t(s.shownItems[size_t(h.index)]);
        const bool shown = card(s, at);
        if (s.told++ < 20)
            LOG_INFO("items: #%d's card %s", s.items[at].id, shown ? "asked of the game's item window (0xC)"
                                                                    : "not shown - the game's item window not made");
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    if (h.part != iw::Part::Search) s.show.typing = false;
    switch (h.part) {
        case iw::Part::Close: {
            const bool asked = closeGameWindow();
            if (s.told++ < 20)
                LOG_INFO("items: its close - %s", asked ? "the game's own window asked to close (its 0x87)"
                                                        : "the game's window not found");
            break;
        }
        case iw::Part::Tab:
            s.show.tab = iw::Tab(h.index);
            s.show.scroll = 0;
            keepState(s);
            break;
        case iw::Part::Search:
            s.show.typing = true;
            break;
        case iw::Part::Cell: {
            if (h.index >= int(s.shownItems.size())) break;
            const int at = s.shownItems[size_t(h.index)];
            const ULONGLONG now = GetTickCount64();
            if ((h.index == s.lastCell && now - s.lastClick <= GetDoubleClickTime()) || m == WM_LBUTTONDBLCLK) {
                useCell(s, h.index, now, "a double click");
                s.lastCell = -1;
                s.press = -1;
            } else {  // a click, or a drag once it moves
                s.lastCell = h.index;
                s.lastClick = now;
                s.press = at;
                s.pressX = tx;
                s.pressY = ty;
                s.dragging = false;
            }
            break;
        }
        case iw::Part::Title:
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

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (!s.shown || !s.show.typing) return false;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        const bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0;
        if (alt) return false;  // Alt+E and the rest: the game's
        switch (wp) {
            case VK_ESCAPE:
            case VK_RETURN:
                s.show.typing = false;
                return true;
            case VK_BACK:
                if (!s.show.search.empty()) s.show.search.pop_back();
                s.show.scroll = 0;
                return true;
            case VK_DELETE:
                s.show.search.clear();
                return true;
            default:
                break;
        }
        const char c = ctrl ? 0 : letterOf(wp, GetKeyState(VK_SHIFT) < 0);
        if (c && s.show.search.size() < kMostTyped) {
            s.show.search += c;
            s.show.scroll = 0;
        }
        return true;  // every key the search's while it types (not the chat's, not the skills')
    }
    return m == WM_KEYUP || m == WM_CHAR || m == WM_DEADCHAR;
}

bool shown() { return state().shown; }

void drawToast(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (s.toast.empty() || now >= s.toastUntil || s.toastK <= 0) return;
    const float sc = s.toastScale;
    if (s.toast != s.toastPainted) {
        s.toastPainted = s.toast;
        panel::Picture& p = s.toastPicture;
        p.width = int(std::ceil(measure(s.toast, 13 * sc, true) + 24 * sc));
        p.height = int(std::ceil(26 * sc));
        p.px.assign(size_t(p.width) * size_t(p.height), 0u);
        const panel::Box all{0, 0, float(p.width), float(p.height)};
        panel::roundRect(p, all, all.h / 2, 0x10141C, 0.92f, 0xF3C653, 0.7f, 1.0f * sc);
        drawWords(p, {Words{all, utf8Wide(s.toast), 0xF3C653, 13 * sc, true, UINT(DT_CENTER), false}});
        s.toastUpload = true;
    }
    const int w = s.toastPicture.width, h = s.toastPicture.height;
    int x = s.toastW / 2 - w / 2, y = int(float(s.toastH) * 0.78f);  // the bag closed: the screen's lower middle
    if (s.shown) {  // over the bag's right end
        x = s.layout.x + s.layout.w - w;
        y = s.layout.y - h - int(6 * sc);
    }
    x = std::clamp(x, 0, std::max(0, s.toastW - w));
    y = std::clamp(y, 0, std::max(0, s.toastH - h));
    const float fade = std::clamp(float(s.toastUntil - now) / 400.0f, 0.0f, 1.0f);  // its last 0.4 s fading
    drawPicture(record, renderer, s.toastPicture, s.toastTexture, s.toastTexW, s.toastTexH, s.toastUpload, x, y,
                s.toastK, fade);
}

void packet(const packets::Event& e) {
    const auto* p = std::get_if<packets::Pickup>(&e);
    State& s = state();
    if (!p || !p->ok) return;
    const auto& names = clienttables::itemNames();
    const auto it = names.find(std::to_string(p->item));
    const std::string name =
        it != names.end() ? utf8Of(widened(hotbar::spaced(it->second), s.codePage)) : "#" + std::to_string(p->item);
    s.fresh.insert(int(p->index));  // 3.0.11-beta.112: «нове» till pointed at, its words for 2 s
    s.toast = "+" + std::to_string(p->amount) + " " + name;
    s.toastUntil = GetTickCount64() + kToastMs;
    s.key.clear();
    if (s.gainsTold >= 40) return;
    ++s.gainsTold;
    const iw::Tab tab = iw::tabOf(p->type);
    const settings::Text words = tab == iw::Tab::Items  ? settings::Text::ItemsTabItems
                                 : tab == iw::Tab::Gear ? settings::Text::ItemsTabGear
                                                        : settings::Text::ItemsTabEtc;
    LOG_INFO("items: %s #%d x%d came into the bag (its place %d, the server's kind %d) - «Речі» shows it under «%s»%s",
             name.c_str(), int(p->item), int(p->amount), int(p->index), int(p->type), say(words).c_str(),
             s.shown ? ", open now" : "");
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const int n = int(s.shownItems.size()), cols = s.layout.columns > 0 ? s.layout.columns : 8;
    const auto keepInSight = [&] {  // the frame's row on the grid's rows
        const int row = s.padFocus / cols;
        if (row < s.show.scroll) s.show.scroll = row;
        if (row >= s.show.scroll + s.layout.rows) s.show.scroll = row - s.layout.rows + 1;
    };
    switch (c) {
        case PadCommand::Up: s.padFocus = std::max(0, s.padFocus - cols); keepInSight(); break;
        case PadCommand::Down: s.padFocus = std::min(std::max(0, n - 1), s.padFocus + cols); keepInSight(); break;
        case PadCommand::Left: s.padFocus = std::max(0, s.padFocus - 1); keepInSight(); break;
        case PadCommand::Right: s.padFocus = std::min(std::max(0, n - 1), s.padFocus + 1); keepInSight(); break;
        case PadCommand::Use: useCell(s, s.padFocus, GetTickCount64(), "the controller's A"); break;
        case PadCommand::Card:
            if (s.padFocus >= 0 && s.padFocus < n) card(s, size_t(s.shownItems[size_t(s.padFocus)]));
            break;
        case PadCommand::Assign:
            if (s.padFocus >= 0 && s.padFocus < n) {
                const iw::Item& it = s.items[size_t(s.shownItems[size_t(s.padFocus)])];
                padassignwin::open(std::to_string(it.id), 0, iw::nameOf(it));
            }
            break;
        case PadCommand::TabBack:
        case PadCommand::TabOn:
            s.show.tab = iw::Tab((int(s.show.tab) + (c == PadCommand::TabOn ? 1 : iw::kTabs - 1)) % iw::kTabs);
            s.padFocus = 0;
            s.show.scroll = 0;
            break;
        case PadCommand::Close: closeGameWindow(); break;
    }
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

}  // namespace uagfx::win::itemswin
