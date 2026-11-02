#include "win/whisper_setup.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/settings/strings.hpp"
#include "core/switch_card.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::whispersetupwin {
namespace {

namespace sw = uagfx::switchcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the whisper settings window (where the manager keeps it, its class's vtable, its SendMsg), a window's
// shown flag; its three checkboxes and the switches the game keeps for them (FUN_004dbe00 reads them as a whisper
// comes: a window of its own for a stranger's, for a friend's, a sound)
constexpr uint32_t kWindowMgr = 0x727ED8, kSetupAt = kWindowMgr + 0x18C, kSetupClass = 0x69BC4C, kSetupSend = 0x4C4010;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kBoxes[3] = {0x64, 0x68, 0x6C}, kFlags[3] = {0x771950, 0x771958, 0x771954};
constexpr int kButtonMsg = 6, kCheckMsg = 0x93, kCloseButton = 0x87;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<bool> values = std::vector<bool>(3, false);
    sw::Show show;
    sw::Layout layout;
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

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t gameSetup() {
    const uint32_t w = read32(kSetupAt);
    return w && read32(w) == kSetupClass && read32(read32(w) + kSendMsgSlot) == kSetupSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("whisper setup: the game's whisper settings hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

sw::Texts texts() {
    sw::Texts t;
    t.title = say(settings::Text::WhisperTitle);
    t.hint = say(settings::Text::WhisperHint);
    t.labels = {say(settings::Text::WhisperStrangers), say(settings::Text::WhisperFriends),
                say(settings::Text::WhisperSound)};
    return t;
}

std::vector<Words> gdiWords(const std::vector<sw::Words>& in) {
    std::vector<Words> out;
    for (const sw::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == sw::Align::Right    ? DT_RIGHT
                                 : w.align == sw::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the whisper's settings", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    sw::paint(s.picture, s.layout, s.values, s.show);
    drawWords(s.picture, gdiWords(sw::words(s.layout, s.values, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::WhisperSetup, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::WhisperSetup, nullptr, nullptr, false);
    s.shown = false;
}

void flip(State& s, int i) {
    const uint32_t window = gameSetup();
    const uint32_t box = window && i >= 0 && i < 3 ? read32(window + kBoxes[i]) : 0;
    if (!box) return;
    const bool now = !s.values[size_t(i)];
    reinterpret_cast<SendMsgFn>(uintptr_t(kSetupSend))(window, box, kButtonMsg, kCheckMsg, now ? 1 : 0, 0);
    if (s.told++ < 30) LOG_INFO("whisper setup: switch %d %s (its checkbox's 0x93)", i, now ? "on" : "off");
    s.key.clear();
}

void closeIt(State& s) {
    const uint32_t window = gameSetup();
    if (!window) return;
    reinterpret_cast<SendMsgFn>(uintptr_t(kSetupSend))(window, 0, kButtonMsg, kCloseButton, 0, 0);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    (void)codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("whisper setup: [interface] whisper_setup = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameSetup() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) LOG_INFO("whisper setup: the game's whisper settings - the new card shows them");
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    for (int i = 0; i < 3; ++i) s.values[size_t(i)] = read32(kFlags[i]) != 0;
    s.layout = sw::layout(targetWidth, targetHeight, s.scale, 3);
    s.show.hover = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const sw::Hit h = sw::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == sw::Part::Toggle) s.show.hover = h.index;
        s.show.hoverClose = h.part == sw::Part::Close;
    }
    std::string key = std::to_string(s.show.hover) + (s.show.hoverClose ? "x" : "") + ',' +
                      std::to_string(s.show.focus) + ';' + std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
    for (bool b : s.values) key += b ? '1' : '0';
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
    const sw::Hit h = sw::hitTest(s.layout, lx, ly);
    if (h.part == sw::Part::Toggle) flip(s, h.index);
    else if (h.part == sw::Part::Close) closeIt(s);
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    closeIt(s);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameSetup(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    int& f = s.show.focus;
    if (f < 0) f = 0;
    switch (c) {
        case PadCommand::Up: f = f <= 0 ? 2 : f - 1; break;
        case PadCommand::Down: f = f >= 2 ? 0 : f + 1; break;
        case PadCommand::Press: flip(s, f); break;
        case PadCommand::Close: closeIt(s); break;
    }
    s.key.clear();
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

}  // namespace uagfx::win::whispersetupwin
