#include "win/macro_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/macro_card.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::macrocardwin {
namespace {

namespace mc = uagfx::macrocard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the macro window (where the manager keeps it, its class's vtable, its SendMsg), a window's place; an
// edit control's words (thiscall); the manager's control with the keys (+0x6C) and its SetFocus
constexpr uint32_t kWindowMgr = 0x727ED8, kMacroAt = kWindowMgr + 0x1A8, kMacroClass = 0x69C288;
constexpr uint32_t kMacroSend = 0x4534C0, kRect = 0x14, kSendMsgSlot = 0x94, kGetText = 0x443300;
constexpr uint32_t kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510;
constexpr int kButtonMsg = 6, kAway = -4000;
constexpr ULONGLONG kBlinkMs = 530;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false, moved = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, oldX = 0;
    std::vector<std::string> texts = std::vector<std::string>(mc::kFields);
    mc::Show show;
    mc::Layout layout;
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

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t gameMacros() {
    const uint32_t w = read32(kMacroAt);
    return w && read32(w) == kMacroClass && read32(read32(w) + kSendMsgSlot) == kMacroSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.moved = false;
        return;
    }
    const int32_t x = int32_t(read32(window + kRect));
    if (on && x != kAway) {
        s.oldX = x;
        write32(window + kRect, uint32_t(kAway));
        if (!s.moved)
            LOG_INFO("macros: the game's macro window off the screen for the new card (its fields keep the keys)");
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        s.moved = false;
    }
}

void readFields(State& s, uint32_t window) {
    const uint32_t focus = read32(kFocus);
    s.show.focus = -1;
    for (int i = 0; i < mc::kFields; ++i) {
        const uint32_t edit = read32(window + mc::kFirstField + 4 * uint32_t(i));
        std::string& out = s.texts[size_t(i)];
        out.clear();
        if (!edit) continue;
        if (edit == focus) s.show.focus = i;
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
        char buf[128] = {};
        if (t && readGame(uint32_t(reinterpret_cast<uintptr_t>(t)), buf, sizeof buf - 1)) {
            buf[sizeof buf - 1] = 0;
            out = utf8Of(widened(buf, s.codePage));
        }
    }
    s.show.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
}

mc::Texts texts() {
    mc::Texts t;
    t.title = say(settings::Text::MacroTitle);
    t.hint = say(settings::Text::MacroHint);
    t.save = say(settings::Text::MacroSave);
    return t;
}

std::vector<Words> gdiWords(const std::vector<mc::Words>& in) {
    std::vector<Words> out;
    for (const mc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == mc::Align::Right    ? DT_RIGHT
                                 : w.align == mc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the chat's macros", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    mc::paint(s.picture, s.layout, s.show);
    drawWords(s.picture, gdiWords(mc::words(s.layout, s.texts, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::Macros, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Macros, nullptr, nullptr, false);
    s.shown = false;
}

void send(State& s, int msg, int id, const char* what) {
    const uint32_t window = gameMacros();
    if (!window) return;
    if (s.told++ < 30) LOG_INFO("macros: %s", what);
    reinterpret_cast<SendMsgFn>(uintptr_t(kMacroSend))(window, 0, msg, id, 0, 0);
    s.key.clear();
}

void focusField(State& s, int i) {
    const uint32_t window = gameMacros();
    const uint32_t edit = window && i >= 0 && i < mc::kFields ? read32(window + mc::kFirstField + 4 * uint32_t(i)) : 0;
    if (edit) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("macros: [interface] macros = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameMacros() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) LOG_INFO("macros: the game's macro window - the new card shows it");
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readFields(s, window);
    s.layout = mc::layout(targetWidth, targetHeight, s.scale);
    s.show.hover = -1;
    s.show.hoverSave = s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const mc::Hit h = mc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == mc::Part::Field) s.show.hover = h.index;
        s.show.hoverSave = h.part == mc::Part::Save;
        s.show.hoverClose = h.part == mc::Part::Close;
    }
    std::string key = std::to_string(s.show.focus) + (s.show.caret ? "|" : "") + ',' + std::to_string(s.show.hover) +
                      (s.show.hoverSave ? "s" : "") + (s.show.hoverClose ? "x" : "") + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (const std::string& t : s.texts) key += ';' + t;
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
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
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
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const mc::Hit h = mc::hitTest(s.layout, lx, ly);
    switch (h.part) {
        case mc::Part::Field: focusField(s, h.index); break;
        case mc::Part::Save: send(s, mc::kSaveMsg, 0, "«Зберегти» - the window's Enter (it writes the macros)"); break;
        case mc::Part::Close: send(s, kButtonMsg, mc::kCloseButton, "closed (the window's 0x87)"); break;
        case mc::Part::None: break;
    }
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    send(s, kButtonMsg, mc::kCloseButton, "Esc - closed (the window's 0x87)");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameMacros(), true);
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

}  // namespace uagfx::win::macrocardwin
