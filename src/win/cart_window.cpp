#include "win/ui_field.hpp"
#include "win/stash_watch.hpp"
#include "win/cart_window.hpp"

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

#include "core/cart_window.hpp"
#include "core/hotbar_panel.hpp"
#include "core/item_window.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/item_window.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/item_grid_ui.hpp"
#include "win/uaui.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win::cartwin {
namespace {

namespace iw = uagfx::itemwin;
namespace cw = uagfx::cartwindow;

// uaRO.exe's: the windows' manager's cart window (its class's vtable, its SendMsg FUN_004b7bd0 - message 6 with 0x87
// its close button: FUN_004d1e60(0x28) - and its drop FUN_004b7ad0: a bag's or the storage's drag - the count window,
// mode 8, or straight in for one), a window's shown flag; the item card's window (0xC, its message 0x18 an ITEM_INFO),
// MakeWindow FUN_004d42d0
constexpr uint32_t kWindowMgr = 0x727ED8, kCartAt = kWindowMgr + cw::kManagerOffset, kCartClass = 0x69B770;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kCartSendMsg = 0x4B7BD0, kDropSlot = 0x2C;
constexpr uint32_t kCartDrop = 0x4B7AD0, kMakeWindow = 0x4D42D0;
constexpr int kButtonMsg = 6, kCloseButton = 0x87, kItemCard = 0xC, kShowItem = 0x18;
// The game mode (its SendMsg FUN_005707d0 at vtable + 0x14): 0x59 a cart thing's drag begun (its place - the cart
// window's press FUN_004b7850), 0x12 let go at the cursor's x, y, 0x13 the drag ended; its drag at +0x32C (the kind)
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0, kDragInfo = 0x32C;
constexpr int kDropMsg = 0x12, kEndDragMsg = 0x13;
// The cart: the session's list (each node's ITEM_INFO at +8 as the bag's); the server's 0x0121 as the game keeps it:
// the things, their most, the weight and its most (in tenths already)
constexpr uint32_t kSession = 0x771148, kCart = kSession + cw::kSessionList, kCartCount = kSession + cw::kSessionCount;
constexpr uint32_t kInfo = 8, kCounts = 0x77192C;
constexpr size_t kInfoSize = 0x48, kMostNodes = 1000;
constexpr ULONGLONG kReadMs = 500, kBlinkMs = 530, kTipMs = 400;
constexpr size_t kMostTyped = 72;  // bytes (3.0.18-beta.72: UTF-8 - 24 letters of any language)
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
using DropFn = void(__attribute__((thiscall)) *)(uint32_t self, int x, int y, uint32_t drag);

struct State {
    bool searchAll = false;  // 3.0.18-beta.72: Ctrl+A in the search
    itemgridui::Ui grid;  // 3.0.18-beta.27: the window drawn by uaUI
    int gridGen = 0;
    bool gridShown = false;
    WindowShadow shadow;
    WindowEdge edge;  // the top of two or more windows (win/window_stack)
    int tipOf = -1;   // the cell pointed at since tipSince
    ULONGLONG tipSince = 0;
    bool on = false, shown = false, hid = false, stateRead = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    float fx = 0.72f, fy = 0.23f;  // its place: a share of the free room to its left and above it
    std::vector<iw::Item> items;
    std::vector<uint32_t> infos;  // each thing's ITEM_INFO in the game's memory (its card's)
    ULONGLONG readAt = 0;
    uint32_t countRead = 0;  // the session's count at the last read
    int count = 0, most = 0, weight = 0, weightMost = 0;  // the server's 0x0121
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
    int lastCell = -1;
    ULONGLONG lastClick = 0;
    int told = 0;
    int press = -1;  // the pressed thing (of `items`), -1 none: dragged once the cursor moves, else a click
    float pressX = 0, pressY = 0;
    bool dragging = false;
    int padFocus = 0;
    bool padNow = false;
    panel::Picture dragIcon;  // the dragged thing's icon under the cursor
    const panel::Picture* dragOf = nullptr;
    uint32_t dragTexture = 0;
    int dragW = 0, dragH = 0;
    bool dragUpload = false;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

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
    const uint32_t w = read32(kCartAt);
    return w && read32(w) == kCartClass ? w : 0;
}

// Its shown flag held at 0 while the new one shows it, back after
void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.hid = false;
        return;
    }
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0) && !s.hid) {
            LOG_INFO("cart: the game's cart window hidden for «Візок» (its shown flag 0)");
            s.hid = true;
        }
    } else if (s.hid) {
        write32(window + kShown, 1);
        s.hid = false;
        LOG_INFO("cart: the game's cart window back");
    }
}

uint32_t modeNow() {
    const uint32_t mode = read32(kModeNow);
    return mode && inGameWorld() && read32(read32(mode) + kModeMsgSlot) == kModeMsg ? mode : 0;
}

bool modeMsg(int msg, int a, int b) {
    const uint32_t mode = modeNow();
    if (!mode) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, 0);
    return true;
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kCartState), text)) return;
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
    const std::string path = layout::join(gameDir(), layout::kCartState);
    if (!writeFileAtomic(path, std::string(text))) LOG_INFO("cart: %s cannot be written", path.c_str());
}

// The cart as the session has it
void readCart(State& s) {
    s.items.clear();
    s.infos.clear();
    const auto& names = clienttables::itemNames();
    const auto& unknown = clienttables::unknownNames();
    const auto& files = clienttables::itemFiles();
    const uint32_t head = read32(kCart);
    s.countRead = read32(kCartCount);
    int32_t counts[4] = {};
    if (readGame(kCounts, counts, sizeof counts)) {
        s.count = counts[0];
        s.most = counts[1];
        s.weight = counts[2];
        s.weightMost = counts[3];
    }
    if (!head) return;
    const std::set<int>& stars = itemswin::starredIds();  // «Речі»'s «★»
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kInfoSize] = {};
        if (!readGame(node + kInfo, raw, sizeof raw)) break;
        const uint32_t index = rd32(raw, 0x08), ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
        if (!index || !ptr || !len || len > 16) continue;
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
        it.starred = stars.count(it.id) > 0;
        s.items.push_back(it);
        s.infos.push_back(node + kInfo);
    }
}

iw::Texts texts(const State& s, bool pad) {
    iw::Texts t;
    t.title = cw::title(say(settings::Text::CartTitle), s.count, s.most);
    t.hotkey = "Alt+W";
    t.tabs[0] = say(settings::Text::ItemsTabAll);
    t.tabs[1] = say(settings::Text::ItemsTabItems);
    t.tabs[2] = say(settings::Text::ItemsTabGear);
    t.tabs[3] = say(settings::Text::ItemsTabCards);
    t.tabs[4] = say(settings::Text::ItemsTabEtc);
    t.tabs[5] = "\xE2\x98\x85";  // ★ the starred (the same as «Речі»'s)
    t.search = say(settings::Text::ItemsSearch);
    t.weight = say(settings::Text::CharWeight);
    t.all = say(settings::Text::StorageTake);
    t.info = say(settings::Text::ItemsInfo);
    if (pad) {
        t.all = std::string(uapad::buttonLabel(0)) + ": " + say(settings::Text::StoragePadTake);
        std::string more = say(settings::Text::CartPadCard);
        if (const size_t at = more.find("{1}"); at != std::string::npos) more.replace(at, 3, uapad::buttonLabel(3));
        t.info = more;
    }
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

void render(State& s) {
    ++s.gridGen;
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the world's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Cart, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Cart, nullptr, nullptr, false);
    s.shown = false;
    s.moving = false;
    s.press = -1;
    s.dragging = false;
}

void place(State& s) {
    const iw::Texts t = texts(s, false);
    s.layout = iw::layout(0, 0, s.scale, t, measure);
    const int roomX = std::max(0, s.targetW - s.layout.w), roomY = std::max(0, s.targetH - s.layout.h);
    s.layout.x = int(std::lround(s.fx * float(roomX)));
    s.layout.y = int(std::lround(s.fy * float(roomY)));
}

// A cart thing into the bag at once, as a drag of it let go over «Речі»: the game's drag of it begun (0x59) and given
// to the game's «Речі» window's drop (made first when not open) - the game's count window for a stack
const char* take(State& s, const iw::Item& it) {
    const uint32_t mode = modeNow();
    if (!mode) return "not taken (the game mode not found)";
    if (!modeMsg(cw::kDragMsg, it.index, 0) || int(read32(mode + kDragInfo)) != cw::kDragKind)
        return "not taken (the game began no drag of it)";
    if (!itemswin::giveDrag(mode + kDragInfo)) {
        modeMsg(kEndDragMsg, 0, 0);
        return "not taken (the game's «Речі» window not made)";
    }
    s.readAt = 0;
    return it.count > 1 ? "given to the game's «Речі» window's drop (its count window)" : "taken into the bag";
}

// The pressed thing let go after a drag: the game's drag of it begun (0x59, as its own window's press) and let go at
// once where the cursor is (0x12): over «Речі» or «Склад» their drops take it
const char* dropAt(const iw::Item& it) {
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy)) return "not dropped (no cursor)";
    if (!modeMsg(cw::kDragMsg, it.index, 0)) return "not dropped (the game mode not found)";
    modeMsg(kDropMsg, cx, cy);
    return "let go through the game's own drop";
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
    if (!window || read32(read32(window) + kSendMsgSlot) != kCartSendMsg) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(kCartSendMsg))(window, 0, kButtonMsg, kCloseButton, 0, 0);
    return true;
}

void takeCell(State& s, int shownIndex, const char* how) {
    if (shownIndex < 0 || shownIndex >= int(s.shownItems.size())) return;
    const iw::Item it = s.items[size_t(s.shownItems[size_t(shownIndex)])];
    const char* did = take(s, it);
    if (s.told++ < 30) LOG_INFO("cart: %s on #%d x%d - %s", how, it.id, it.count, did);
}


}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("cart: [interface] cart = %d", on ? 1 : 0);
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
    const float scale = uistyle::itemScale(k);  // 3.0.15-beta.9: [interface] item_windows
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    if (!s.shown) s.readAt = 0;
    if (now - s.readAt >= kReadMs || read32(kCartCount) != s.countRead) {
        s.readAt = now;
        readCart(s);
        stashwatch::note(stashbook::Kind::Cart, s.items, s.codePage);  // 3.0.11-beta.286
        stashwatch::prices(s.items);  // 3.0.11-beta.287: the site's market in the tips
    }
    if (!s.shown)
        LOG_INFO("cart: the cart open - «Візок» shows it (%d of %d things, weight %d of %d)", s.count, s.most,
                 s.weight, s.weightMost);
    s.show.weight = s.weight;
    s.show.weightMost = s.weightMost;
    s.show.zeny = -1;
    place(s);
    s.shownItems = iw::shownOf(s.items, s.show.tab, s.show.search);
    s.show.also =  // 3.0.11-beta.286
        stashwatch::also(s.show.search, s.codePage, {stashbook::Kind::Cart, stashbook::Kind::Bag});
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
    if (s.show.hover != s.tipOf) {
        s.tipOf = s.show.hover;
        s.tipSince = now;
    }
    s.show.tip = s.show.hover >= 0 && now - s.tipSince >= kTipMs;
    s.padNow = uapad::runtime::look().shown;
    if (s.padNow && !s.shownItems.empty()) {
        s.padFocus = std::clamp(s.padFocus, 0, int(s.shownItems.size()) - 1);
        s.show.hover = s.padFocus;
        s.show.tip = true;
    }
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(int(s.show.tab)) + ';' + std::to_string(s.show.scroll) + ';' +
                      std::to_string(s.show.hover) + (s.show.tip ? "t" : "") + ',' + std::to_string(s.show.hoverTab) +
                      (s.show.hoverClose ? "x" : "") + ';' + s.show.search + (s.show.typing ? "|t" : "") +
                      (s.show.typing && s.show.caretOn ? "c" : "") + ';' + std::to_string(s.count) + '/' +
                      std::to_string(s.most) + ',' + std::to_string(s.weight) + '/' + std::to_string(s.weightMost) +
                      ';' + (s.padNow ? "P;" : ";");
    for (const iw::Item& it : s.items)
        key += std::to_string(it.index) + ':' + std::to_string(it.id) + 'x' + std::to_string(it.count) + '+' +
               std::to_string(it.refine) + (it.identified ? "" : "?") + (it.icon ? "i" : "") +
               (it.starred ? "*" : "") + ',';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
        drawn != s.key) {  // 3.0.18-beta.27: uaUI's or the picture
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.gridShown = s.grid.frame("cart", s.shown, s.layout.x, s.layout.y, s.gridGen, s.layout, s.items, s.shownItems,
                               s.show, texts(s, s.padNow), measure);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.grid.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.gridShown)) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    const bool known = readGameCursor(cx, cy);
    if (known) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                  12 * s.scale, s.scale, s.k);
    const panel::Picture* icon = s.dragging && s.press >= 0 && size_t(s.press) < s.items.size()
                                     ? s.items[size_t(s.press)].icon
                                     : nullptr;
    if (icon && icon->width > 0 && known) {  // the dragged thing's icon under the cursor
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
    if (!window || read32(read32(window) + kDropSlot) != kCartDrop) return false;
    reinterpret_cast<DropFn>(uintptr_t(kCartDrop))(window, 0, 0, drag);
    if (s.told++ < 30) LOG_INFO("cart: a drag let go over «Візок» - given to the game's cart window's drop");
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
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
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
    if (s.press >= 0 && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {  // a thing pressed: dragged, or a click
        if (size_t(s.press) >= s.items.size()) {
            s.press = -1;
            s.dragging = false;
            return false;
        }
        if (m == WM_MOUSEMOVE) {
            if (!s.dragging && std::hypot(tx - s.pressX, ty - s.pressY) > 10 * s.scale) s.dragging = true;
            return s.dragging;
        }
        const iw::Item it = s.items[size_t(s.press)];
        const bool dragged = s.dragging;
        s.press = -1;
        s.dragging = false;
        s.key.clear();
        if (dragged) {
            const char* did = dropAt(it);
            s.lastCell = -1;
            s.readAt = 0;
            if (s.told++ < 30) LOG_INFO("cart: #%d dragged and let go - %s", it.id, did);
        }
        result = 0;
        return true;
    }
    const iw::Hit h = iw::hitTest(s.layout, s.shownItems.size(), s.show.scroll, tx - float(s.layout.x),
                                  ty - float(s.layout.y));
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
        const bool shownCard = card(s, at);
        if (s.told++ < 30)
            LOG_INFO("cart: #%d's card %s", s.items[at].id,
                     shownCard ? "asked of the game's item window (0xC)"
                               : "not shown - the game's item window not made");
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    if (h.part != iw::Part::Search) s.show.typing = false;
    switch (h.part) {
        case iw::Part::Close: {
            const bool asked = closeGameWindow();
            if (s.told++ < 30)
                LOG_INFO("cart: its close - %s", asked ? "the game's cart window asked to close (its 0x87)"
                                                       : "the game's cart window not found");
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
        case iw::Part::Star:  // the same «★» as «Речі»'s
            if (h.index < int(s.shownItems.size())) {
                itemswin::toggleStar(s.items[size_t(s.shownItems[size_t(h.index)])].id);
                s.readAt = 0;
            }
            break;
        case iw::Part::Cell: {
            if (h.index >= int(s.shownItems.size())) break;
            const ULONGLONG now = GetTickCount64();
            if ((h.index == s.lastCell && now - s.lastClick <= GetDoubleClickTime()) || m == WM_LBUTTONDBLCLK) {
                takeCell(s, h.index, "a double click");
                s.lastCell = -1;
                s.press = -1;
            } else {  // a click, or a drag once it moves
                s.lastCell = h.index;
                s.lastClick = now;
                s.press = s.shownItems[size_t(h.index)];
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
    // 3.0.18-beta.72 (the big six, E): the new interface's field - any language, the clipboard, every key its own
    textfield::Field field{s.show.search, s.searchAll};
    uifield::Result r = uifield::Result::None;
    const bool took = uifield::key(field, m, wp, kMostTyped, r);
    s.show.search = field.text;
    s.searchAll = field.all;
    if (r == uifield::Result::Done) s.show.typing = false;
    if (r == uifield::Result::Changed) s.show.scroll = 0;
    return took;
}

bool shown() { return state().shown; }

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    if (const uint32_t window = gameWindow()) hold(s, window, true);
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
        case PadCommand::Take: takeCell(s, s.padFocus, "the controller's A"); break;
        case PadCommand::Card:
            if (s.padFocus >= 0 && s.padFocus < n) card(s, size_t(s.shownItems[size_t(s.padFocus)]));
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

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

void moveTo(int x, int y, bool keep) {
    State& s = state();
    if (!s.shown) return;
    const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
    s.fx = std::clamp(float(x) / float(roomX), 0.0f, 1.0f);
    s.fy = std::clamp(float(y) / float(roomY), 0.0f, 1.0f);
    s.layout.x = x;
    s.layout.y = y;
    if (keep) keepState(s);
}

bool moving() { return state().shown && state().moving; }

}  // namespace uagfx::win::cartwin
