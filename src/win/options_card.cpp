#include "win/options_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/options_card.hpp"
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

namespace uagfx::win::optionscardwin {
namespace {

namespace oc = uagfx::optionscard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the options window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown
// flag; the switches the game keeps (in our toggles' order: the music off - inverted -, the sounds, the snaps, opaque)
// and the window's volumes (the music's +0x74, the sounds' +0x70); its refresh message (0x3c: the controls from them)
constexpr uint32_t kWindowMgr = 0x727ED8, kOptionsAt = kWindowMgr + 0x98, kOptionsClass = 0x698830;
constexpr uint32_t kOptionsSend = 0x473DD0, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kFlags[6] = {0x772D28, 0x6B8320, 0x77196C, 0x771968, 0x771960, 0x77195C};
constexpr uint32_t kVolumes[2] = {0x74, 0x70};
constexpr int kButtonMsg = 6, kRefresh = 0x3C, kPadStep = 8;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    oc::Values values;
    oc::Show show;
    oc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int dragging = -1;  // the slider held
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

uint32_t gameOptions() {
    const uint32_t w = read32(kOptionsAt);
    return w && read32(w) == kOptionsClass && read32(read32(w) + kSendMsgSlot) == kOptionsSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("options: the game's options window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

oc::Values valuesOf(uint32_t window) {
    oc::Values v;
    for (int i = 0; i < 6; ++i) v.on[i] = read32(kFlags[i]) != 0;
    v.on[0] = !v.on[0];  // the game keeps the music's «off»
    for (int i = 0; i < 2; ++i)
        v.volume[i] = std::clamp(int(int32_t(read32(window + kVolumes[i]))), 0, oc::kVolumeMost);
    return v;
}

oc::Texts texts() {
    oc::Texts t;
    t.title = say(settings::Text::OptionsTitle);
    const settings::Text names[6] = {settings::Text::OptionsMusic,      settings::Text::OptionsSounds,
                                     settings::Text::OptionsAttackSnap, settings::Text::OptionsSkillSnap,
                                     settings::Text::OptionsItemSnap,   settings::Text::OptionsOpaque};
    for (int i = 0; i < 6; ++i) t.toggles[i] = say(names[i]);
    t.snapHint = say(settings::Text::OptionsSnapHint);
    return t;
}

std::vector<Words> gdiWords(const std::vector<oc::Words>& in) {
    std::vector<Words> out;
    for (const oc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == oc::Align::Right    ? DT_RIGHT
                                 : w.align == oc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the game's options", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    oc::paint(s.picture, s.layout, s.values, s.show);
    drawWords(s.picture, gdiWords(oc::words(s.layout, s.values, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::Options, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Options, nullptr, nullptr, false);
    s.shown = false;
    s.dragging = -1;
}

void toggle(State& s, int i) {
    const uint32_t window = gameOptions();
    const uint32_t box = window && i >= 0 && i < 6 ? read32(window + oc::kToggleAt[i]) : 0;
    if (!box) return;
    const bool now = !s.values.on[i];
    const auto send = reinterpret_cast<SendMsgFn>(uintptr_t(kOptionsSend));
    send(window, box, kButtonMsg, oc::kCheckMsg, now ? 1 : 0, 0);
    send(window, 0, kRefresh, 0, 0, 0);  // its own checkboxes from the game's switches again
    if (s.told++ < 40) LOG_INFO("options: switch %d %s (its checkbox's 0x93)", i, now ? "on" : "off");
    s.values = valuesOf(window);
    s.key.clear();
}

void setVolume(State& s, int i, int volume) {
    const uint32_t window = gameOptions();
    const uint32_t slider = window && i >= 0 && i < 2 ? read32(window + oc::kSliderAt[i]) : 0;
    if (!slider) return;
    const int target = std::clamp(volume, 0, oc::kVolumeMost), by = target - s.values.volume[i];
    if (!by) return;
    reinterpret_cast<SendMsgFn>(uintptr_t(kOptionsSend))(window, slider, oc::kStepMsg, by, 0, 0);
    if (s.told++ < 40) LOG_INFO("options: volume %d to %d (its slider's message 8 by %d)", i, target, by);
    s.values = valuesOf(window);
    s.key.clear();
}

void closeIt(State& s) {
    const uint32_t window = gameOptions();
    if (!window) return;
    if (s.told++ < 40) LOG_INFO("options: closed (the window's 0x87)");
    reinterpret_cast<SendMsgFn>(uintptr_t(kOptionsSend))(window, 0, kButtonMsg, oc::kCloseButton, 0, 0);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    (void)codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("options: [interface] options = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameOptions() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = oc::Show{};
        LOG_INFO("options: the game's options window - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.values = valuesOf(window);
    s.layout = oc::layout(targetWidth, targetHeight, s.scale);
    s.show.hoverToggle = s.show.hoverSlider = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const oc::Hit h = oc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == oc::Part::Toggle) s.show.hoverToggle = h.index;
        if (h.part == oc::Part::Slider) s.show.hoverSlider = h.index;
        s.show.hoverClose = h.part == oc::Part::Close;
    }
    std::string key = std::to_string(s.values.volume[0]) + ',' + std::to_string(s.values.volume[1]) + ';' +
                      std::to_string(s.show.hoverToggle) + ',' + std::to_string(s.show.hoverSlider) +
                      (s.show.hoverClose ? "x" : "") + ',' + std::to_string(s.show.focus) + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (bool b : s.values.on) key += b ? '1' : '0';
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
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (s.dragging >= 0) {  // a slider held: it follows the cursor until the button comes up
        if (m == WM_MOUSEMOVE || m == WM_LBUTTONUP) {
            setVolume(s, s.dragging, oc::volumeAt(s.layout.sliders[s.dragging], lx));
            if (m == WM_LBUTTONUP) s.dragging = -1;
            result = 0;
            return true;
        }
    }
    if (m == WM_MOUSEMOVE || !s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    const oc::Hit h = oc::hitTest(s.layout, lx, ly);
    if (m == WM_MOUSEWHEEL) {
        if (h.part == oc::Part::Slider)
            setVolume(s, h.index, s.values.volume[h.index] + (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? kPadStep : -kPadStep));
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    switch (h.part) {
        case oc::Part::Toggle: toggle(s, h.index); break;
        case oc::Part::Slider:
            s.dragging = h.index;
            setVolume(s, h.index, oc::volumeAt(s.layout.sliders[h.index], lx));
            break;
        case oc::Part::Close: closeIt(s); break;
        case oc::Part::None: break;
    }
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
    if (s.on && inGameWorld()) hold(s, gameOptions(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    int& f = s.show.focus;
    if (f < 0) f = 0;
    switch (c) {
        case PadCommand::Up: f = f <= 0 ? 5 : f - 1; break;
        case PadCommand::Down: f = f >= 5 ? 0 : f + 1; break;
        case PadCommand::Left:
        case PadCommand::Right:
            if (f < 2) setVolume(s, f, s.values.volume[f] + (c == PadCommand::Right ? kPadStep : -kPadStep));
            else toggle(s, f);
            break;
        case PadCommand::Press: toggle(s, f); break;
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

}  // namespace uagfx::win::optionscardwin
