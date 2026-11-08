#include "win/popup_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "core/popup_card.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

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
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);

struct State {
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

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
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

std::vector<Words> gdiWords(const std::vector<pc::Words>& in) {
    std::vector<Words> out;
    for (const pc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == pc::Align::Right    ? DT_RIGHT
                                 : w.align == pc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("a popup menu", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    pc::paint(s.picture, s.layout, s.items, s.hover);
    drawWords(s.picture, gdiWords(pc::words(s.layout, s.items, s.hover)));
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

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
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
    if (readGameCursor(cx, cy))
        s.hover = pc::rowAt(s.layout, s.items, float(cx) * float(k) - float(s.layout.x),
                            float(cy) * float(k) - float(s.layout.y));
    std::string key = std::to_string(s.hover) + ';' + std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) +
                      ',' + std::to_string(s.layout.w) + '#' + std::to_string(uistyle::generation());
    for (const pc::Item& it : s.items) key += ';' + (it.separator ? std::string("-") : it.text);
    if (key != s.key) {
        s.key = key;
        render(s);
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
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 10 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) {  // a press elsewhere closes it, as the game's own; the press goes on
        if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_MBUTTONDOWN) close();
        return false;
    }
    result = 0;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) pick(s, pc::rowAt(s.layout, s.items, lx, ly));
    return true;
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
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::popupwin
