#include "win/whisper_setup.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
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
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"

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

// 3.0.18-beta.87 (live windows, group 2): the whisper setup's switches a live window
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw ow" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<span class="x ctl" data-event-click="sw_close()">&#x2715;</span>
<div class="hint" data-if="hint != ''">{{hint}}</div>
<div class="orow" data-for="r, i : rows"><span class="tick ctl" data-class-on="r.on" data-event-click="sw_flip(i)"><span
  class="box"></span>{{r.label}}</span></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
struct SwitchRowView {
    Rml::String label;
    bool on = false;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<SwitchRowView> rowViews;
    Rml::String title, hint, ground = "#10141cf0";
    uisheet::Sheet sheet;  // 3.0.18-beta.32: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
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

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // 3.0.18-beta.81: the live window's own box
    if (s.live.box(lx, ly, lw, lh)) x0 = float(lx), y0 = float(ly), b.w = float(lw), b.h = float(lh);
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

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
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
    if (readHoverCursor(cx, cy)) {
        const sw::Hit h = sw::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == sw::Part::Toggle) s.show.hover = h.index;
        s.show.hoverClose = h.part == sw::Part::Close;
    }
    std::string key = std::to_string(s.show.hover) + (s.show.hoverClose ? "x" : "") + ',' +
                      std::to_string(s.show.focus) + ';' + std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
    for (bool b : s.values) key += b ? '1' : '0';
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.32
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("whisper setup", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<SwitchRowView>()) {
            v.RegisterMember("label", &SwitchRowView::label);
            v.RegisterMember("on", &SwitchRowView::on);
        }
        m.RegisterArray<std::vector<SwitchRowView>>();
        m.Bind("rows", &s.rowViews);
        m.Bind("title", &s.title);
        m.Bind("hint", &s.hint);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("sw_flip", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) flip(state(), a[0].Get<int>());
        });
        m.BindEventCallback("sw_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            closeIt(state());
        });
    });
    if (!s.live.made()) return;
    const sw::Texts t = texts();
    std::vector<SwitchRowView> rows;
    for (size_t i = 0; i < t.labels.size(); ++i)
        rows.push_back(SwitchRowView{t.labels[i], i < s.values.size() && s.values[i]});
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const bool same = rows.size() == s.rowViews.size() &&
                      std::equal(rows.begin(), rows.end(), s.rowViews.begin(),
                                 [](const SwitchRowView& a, const SwitchRowView& b) {
                                     return a.label == b.label && a.on == b.on;
                                 });
    if (!same) s.rowViews = std::move(rows), s.live.doc.dirty("rows");
    if (t.title != s.title || t.hint != s.hint || ground != s.ground) {
        s.title = t.title, s.hint = t.hint, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    s.live.frame(s.shown, s.layout.x, s.layout.y, int(winstack::Win::WhisperSetup), ".tick, .x");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    liveFrame(s);  // the old edition (the sheet) gone: the live window alone
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.live.shown) return;  // 3.0.18-beta.81
    s.live.draw(s.shadow, record, renderer, s.scale, s.k);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (s.live.shown) {  // 3.0.18-beta.81: RmlUi's
        if (!s.live.mouse(game, m, wp, lp)) return false;
        result = 0;
        return true;
    }
    return false;  // the old edition gone: the live window alone
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
    if (s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.81
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::whispersetupwin
