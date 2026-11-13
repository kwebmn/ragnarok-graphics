#include "win/popup_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/popup_card.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::popupwin {
namespace {

namespace pc = uagfx::popupcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the popup (where the manager keeps it, its class's vtable; its shown flag, its place, its owner's id,
// its items - their first, past the last, 16 bytes each), the separator's words (a pointer to them); the manager's
// FindWindow and DeleteWindow; the game mode (its message 0x1A: the world's player menu's pick)
constexpr uint32_t kWindowMgr = 0x727ED8, kPopupAt = kWindowMgr + 0xC0, kPopupClass = 0x699C68, kShown = 0x38;
constexpr uint32_t kRect = 0x14, kOwner = 0x60, kItems = 0x70, kSeparator = 0x6B99B0;
constexpr uint32_t kFindWindow = 0x4D1CB0, kDeleteWindow = 0x4D1E60, kSendMsgSlot = 0x94;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kPopupWindow = 0x12, kPickMsg = 0x30, kWorldOwner = 0x89, kWorldPick = 0x1A, kMostItems = 40;
using FindFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
using DeleteFn = char(__attribute__((thiscall)) *)(uint32_t manager, int id);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);

// 3.0.18-beta.73 (the big six, F): the menu a live document - its rows one under another, RmlUi's clicks
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw popw" data-style-background-color="ground">
<div data-for="e, i : items"><div class="sep" data-if="e.sep"></div><div class="row ctl" data-if="!e.sep"
  data-event-click="pick_row(i)">{{e.text}}</div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";

struct ItemView {
    Rml::String text;
    bool sep = false;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.73
    std::vector<ItemView> views;
    Rml::String ground = "#10141cf0";
    uisheet::Sheet sheet;  // 3.0.18-beta.32: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, hid = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    uint32_t window = 0;
    std::vector<pc::Item> items;
    int hover = -1;
    pc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t gamePopup() {
    const uint32_t w = read32(kPopupAt);
    return w && read32(w) == kPopupClass ? w : 0;
}

std::string raw(uint32_t at) {
    char buf[96] = {};
    if (!at || !readGame(at, buf, sizeof buf - 1)) return "";
    buf[sizeof buf - 1] = 0;
    return buf;
}

void readItems(State& s, uint32_t window) {
    std::vector<std::string> words;
    const uint32_t from = read32(window + kItems), past = read32(window + kItems + 4);
    if (from && past > from && (past - from) / 16 <= uint32_t(2 * kMostItems))
        for (uint32_t at = from; at < past; at += 16) words.push_back(raw(read32(at + 4)));
    s.items = pc::items(words, raw(read32(kSeparator)));
    for (pc::Item& it : s.items) it.text = pc::translated(utf8Of(widened(it.text, s.codePage)), language());
}

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // 3.0.18-beta.73: the live menu's own box
    if (s.live.box(lx, ly, lw, lh)) x0 = float(lx), y0 = float(ly), b.w = float(lw), b.h = float(lh);
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(Blocker::Popup, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Popup, nullptr, nullptr, false);
    s.shown = false;
}

// The row picked: what the game's own click does with it, then the menu closed
void pick(State& s, int row) {
    const uint32_t w = gamePopup();
    const int number = pc::pickNumber(s.items, row);
    if (!w || number < 0) return;
    const int owner = int(read32(w + kOwner));
    const uint32_t ownerWindow = reinterpret_cast<FindFn>(uintptr_t(kFindWindow))(kWindowMgr, owner);
    if (s.told++ < 30) LOG_INFO("popups: «%s» picked (item %d, its owner 0x%X)", s.items[size_t(row)].text.c_str(),
                                number, unsigned(owner));
    if (ownerWindow) {
        reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(ownerWindow) + kSendMsgSlot)))(ownerWindow, 0, kPickMsg,
                                                                                         number, 0, 0);
    } else if (owner == kWorldOwner) {
        const uint32_t mode = read32(kModeNow);
        if (mode && read32(read32(mode) + kModeMsgSlot) == kModeMsg)
            reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kWorldPick, number, 0, 0);
    }
    if (gamePopup()) reinterpret_cast<DeleteFn>(uintptr_t(kDeleteWindow))(kWindowMgr, kPopupWindow);
    s.key.clear();
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("popups: [interface] popups = %d", on ? 1 : 0);
    }
    const uint32_t window = gamePopup();
    if (window) {  // the game's menu hidden while ours shows; back when ours is off
        if (on && read32(window + kShown) && write32(window + kShown, 0)) s.hid = true;
        else if (!on && s.hid && write32(window + kShown, 1)) s.hid = false;
    } else {
        s.hid = false;
    }
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    if (window != s.window) s.hover = -1;
    s.window = window;
    readItems(s, window);
    float widest = 0;
    for (const pc::Item& it : s.items)
        if (!it.separator) widest = std::max(widest, float(wordsWidth(utf8Wide(it.text), 13 * s.scale, true)));
    const int atX = int(std::lround(float(int32_t(read32(window + kRect))) * s.k));
    const int atY = int(std::lround(float(int32_t(read32(window + kRect + 4))) * s.k));
    s.layout = pc::layout(s.items, atX, atY, targetWidth, targetHeight, s.scale, widest);
    if (!s.shown) LOG_INFO("popups: the game's popup menu (%zu items) - the new card shows it", s.items.size());
    s.hover = -1;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        s.hover = pc::rowAt(s.layout, s.items, float(cx) * float(k) - float(s.layout.x),
                            float(cy) * float(k) - float(s.layout.y));
    std::string key = std::to_string(s.hover) + ';' + std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) +
                      ',' + std::to_string(s.layout.w) + '#' + std::to_string(uistyle::generation());
    for (const pc::Item& it : s.items) key += ';' + (it.separator ? std::string("-") : it.text);
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.32
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.73: the live menu's model and place
static void liveFrame(State& s) {
    s.live.make("popup", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<ItemView>()) {
            v.RegisterMember("text", &ItemView::text);
            v.RegisterMember("sep", &ItemView::sep);
        }
        m.RegisterArray<std::vector<ItemView>>();
        m.Bind("items", &s.views);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("pick_row", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) pick(state(), a[0].Get<int>());
        });
    });
    if (!s.live.made()) return;
    std::vector<ItemView> views;
    for (const pc::Item& it : s.items) views.push_back(ItemView{it.text, it.separator});
    const bool same = views.size() == s.views.size() &&
                      std::equal(views.begin(), views.end(), s.views.begin(), [](const ItemView& a, const ItemView& b) {
                          return a.text == b.text && a.sep == b.sep;
                      });
    if (!same) s.views = std::move(views), s.live.doc.dirty("items");
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    if (ground != s.ground) s.ground = ground, s.live.doc.dirty("ground");
    s.live.frame(s.shown, s.layout.x, s.layout.y, int(winstack::Win::Popup), ".row");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    liveFrame(s);  // the old edition (the sheet) gone: the live window alone
    if (s.sheetShown) {  // 3.0.18-beta.60: the pad walks its boxes (A clicks one)
        std::vector<panel::Box> targets;
        for (const auto& part : std::initializer_list<std::vector<panel::Box>>{s.layout.rows})
            targets.insert(targets.end(), part.begin(), part.end());
        s.sheet.nav(int(winstack::Win::Popup), std::move(targets));
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.live.shown) return;  // 3.0.18-beta.73
    s.live.draw(s.shadow, record, renderer, s.scale, s.k);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    if (s.live.shown) {  // 3.0.18-beta.73: RmlUi's; a press elsewhere closes it, the press going on
        if (!s.live.mouse(game, m, wp, lp)) {
            if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_MBUTTONDOWN) close();
            return false;
        }
        result = 0;
        return true;
    }
    return false;  // the old edition gone: the live window alone
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown || !gamePopup()) return false;
    reinterpret_cast<DeleteFn>(uintptr_t(kDeleteWindow))(kWindowMgr, kPopupWindow);
    s.key.clear();
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on) return;
    const uint32_t w = gamePopup();
    if (w && read32(w + kShown) && write32(w + kShown, 0)) s.hid = true;
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    if (s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.73
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::popupwin
