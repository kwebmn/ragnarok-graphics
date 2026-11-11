#include "win/tip_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/settings/strings.hpp"
#include "core/tip_card.hpp"
#include "core/tip_texts.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/chat_window.hpp"
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
#include "win/words.hpp"

namespace uagfx::win::tipwin {
namespace {

namespace tc = uagfx::tipcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the tip window (where the manager keeps it, its class's vtable, its SendMsg; its shown flag, the tip's
// number, the start box's state) and the tips' list (its loaded flag; + 8 the first tip, + 0xC past the last - 16
// bytes each: + 4 its first line, + 8 past the last; a line 16 bytes: + 4 its words, + 8 their length)
constexpr uint32_t kWindowMgr = 0x727ED8, kTipAt = kWindowMgr + 0x13C, kTipClass = 0x699540, kTipSend = 0x47FA80;
constexpr uint32_t kSendMsgSlot = 0x94, kShown = 0x38, kNumber = 0x64, kStartup = 0x68;
constexpr uint32_t kTips = 0x74B9F8;
constexpr int kButtonMsg = 6, kMostTips = 5000, kMostLines = 40;

// 3.0.18-beta.81 (the big six round 2, item 4): the tip a live window - its paragraphs wrapped by RmlUi, its
// buttons a flex row
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw tw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span><span class="count">{{counter}}</span></div>
<span class="x ctl" data-event-click="tip_press(3)">&#x2715;</span>
<div class="msg"><p data-for="p : paragraphs">{{p}}</p></div>
<div class="foot"><span class="tick ctl" data-class-on="startup" data-event-click="tip_press(4)"><span class="box"></span>{{startup_word}}</span>
<div class="btns"><div class="btn ctl" data-event-click="tip_press(1)">{{back}}<span class="key">{{back_key}}</span></div>
<div class="btn main ctl" data-event-click="tip_press(2)">{{next}}<span class="key">{{next_key}}</span></div></div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<Rml::String> paragraphViews;
    Rml::String title, counterText, back, next, startupWord, backKey, nextKey, ground = "#10141cf0";
    bool startupOn = false;
    uisheet::Sheet sheet;  // 3.0.18-beta.34: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, hid = false, padNow = false;
    int oursTold = 0;  // 3.0.11-beta.219
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    uint32_t window = 0;
    int number = -1;  // the tip read last (the game's, from 0)
    tc::Card card;
    tc::Show show;
    tc::Layout layout;
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

uint32_t gameTip() {
    const uint32_t w = read32(kTipAt);
    return w && read32(w) == kTipClass && read32(kTipClass + kSendMsgSlot) == kTipSend ? w : 0;
}

// The game's tip `number` (from 0) - its lines as the game keeps them (its code page); `count` the tips'
std::vector<std::string> tipLines(uint32_t number, int& count) {
    count = 0;
    uint8_t loaded = 0;
    if (!readGame(kTips, &loaded, 1) || !loaded) return {};
    const uint32_t first = read32(kTips + 8), last = read32(kTips + 0xC);
    if (!first || last <= first || (last - first) / 16 > uint32_t(kMostTips)) return {};
    count = int((last - first) / 16);
    if (number >= uint32_t(count)) return {};
    const uint32_t tip = first + 16 * number, from = read32(tip + 4), to = read32(tip + 8);
    std::vector<std::string> out;
    for (uint32_t line = from; line && line < to && out.size() < size_t(kMostLines); line += 16) {
        const uint32_t words = read32(line + 4), length = std::min<uint32_t>(read32(line + 8), 1000);
        std::string t(length, '\0');
        if (words && length && !readGame(words, t.data(), t.size())) t.clear();
        while (!t.empty() && (t.back() == '\r' || t.back() == '\0')) t.pop_back();  // 3.0.17-beta.26: \r\n files
        out.push_back(t);
    }
    return out;
}

tc::Texts texts(const State& s) {
    tc::Texts t;
    t.title = say(settings::Text::TipTitle);
    t.back = say(settings::Text::TipBack);
    t.next = say(settings::Text::TipNext);
    t.close = say(settings::Text::TipClose);
    t.startup = say(settings::Text::TipStartup);
    t.backKey = s.padNow ? uapad::buttonLabel(2) : "\xE2\x86\x90";  // X, ←
    t.nextKey = s.padNow ? uapad::buttonLabel(0) : "\xE2\x86\x92";  // A, →
    t.closeKey = s.padNow ? uapad::buttonLabel(1) : "Esc";
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
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(Blocker::Tip, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Tip, nullptr, nullptr, false);
    s.shown = false;
}

void press(State& s, tc::Part p, const char* why) {
    const uint32_t w = gameTip();
    const int id = tc::gameButton(p);
    if (!w || !id) return;
    const int value = p == tc::Part::Startup ? (read32(w + kStartup) ? 0 : 1) : 0;
    if (s.told++ < 30) LOG_INFO("tips: the window's 0x%X%s (%s)", unsigned(id), p == tc::Part::Startup
                                ? (value ? " - shown at the start" : " - not shown at the start") : "", why);
    reinterpret_cast<SendMsgFn>(uintptr_t(kTipSend))(w, 0, kButtonMsg, id, value, 0);
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
        LOG_INFO("tips: [interface] tips = %d", on ? 1 : 0);
    }
    const uint32_t window = gameTip();
    if (window) {  // the game's window hidden while ours shows; back when ours is off
        if (on && read32(window + kShown) && write32(window + kShown, 0)) s.hid = true;
        else if (!on && s.hid && write32(window + kShown, 1)) s.hid = false;
    } else {
        s.hid = false;
    }
    if (!on || !window || !inGameWorld() || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        s.number = -1;
        return;
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.padNow = uapad::runtime::look().shown;
    const int number = int(read32(window + kNumber));
    if (window != s.window || number != s.number) {
        int count = 0;
        std::vector<std::string> lines = tipLines(uint32_t(number), count);
        for (std::string& l : lines) l = utf8Of(widened(l, codePage));
        // 3.0.11-beta.219 (core/tip_texts): uaRO's tips in the interface's language
        int all = 0;
        const std::vector<std::string> first = tipLines(0, all);
        if (const std::string ours = tiptexts::tip(number, language());
            !first.empty() && tiptexts::isUaroList(all, first.front()) && !ours.empty()) {
            lines.clear();
            for (size_t at = 0;;) {
                const size_t end = ours.find('\n', at);
                lines.push_back(ours.substr(at, end == std::string::npos ? std::string::npos : end - at));
                if (end == std::string::npos) break;
                at = end + 1;
            }
            if (s.oursTold++ == 0) LOG_INFO("tips: uaRO's tips in the interface's language (the driver's words)");
            count = std::min(count, tiptexts::kCount);  // 3.0.17-beta.26: not the \r\n file's empty one
        }
        s.card.paragraphs = tc::paragraphs(lines);
        s.card.number = number + 1;
        s.card.count = count;
        if (window != s.window) LOG_INFO("tips: the game's tip of the day - the new card shows it (%d of %d)",
                                         number + 1, count);
        s.window = window;
        s.number = number;
    }
    s.card.startup = read32(window + kStartup) != 0;
    std::string text;
    for (const std::string& p : s.card.paragraphs) text += (text.empty() ? "" : "\n") + p;
    const float px = 13 * s.scale;
    s.card.rows = tc::rows(text, tc::textWidth(s.scale), [&](const std::string& piece) {
        return float(wordsWidth(utf8Wide(piece), px, false));
    });
    s.layout = tc::layout(s.card, targetWidth, targetHeight, s.scale, texts(s));
    s.show.hover = tc::Part::None;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        s.show.hover = tc::partAt(s.layout, float(cx) * float(k) - float(s.layout.x),
                                  float(cy) * float(k) - float(s.layout.y));
    const std::string key = std::to_string(s.number) + (s.card.startup ? "S" : "") + ';' +
                            std::to_string(int(s.show.hover)) + ';' + std::to_string(s.layout.x) + ',' +
                            std::to_string(s.layout.y) + ',' + std::to_string(s.layout.w) + (s.padNow ? "P" : "") +
                            '#' + std::to_string(uistyle::generation());
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.34
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.anywhere = true;
    s.live.make("tip", kLive, [&](Rml::DataModelConstructor& m) {
        m.RegisterArray<std::vector<Rml::String>>();
        m.Bind("paragraphs", &s.paragraphViews);
        m.Bind("title", &s.title);
        m.Bind("counter", &s.counterText);
        m.Bind("back", &s.back);
        m.Bind("next", &s.next);
        m.Bind("startup_word", &s.startupWord);
        m.Bind("back_key", &s.backKey);
        m.Bind("next_key", &s.nextKey);
        m.Bind("startup", &s.startupOn);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("tip_press", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (a.empty()) return;
            const int p = a[0].Get<int>();
            press(state(), p == 1 ? tc::Part::Back : p == 2 ? tc::Part::Next : p == 3 ? tc::Part::Close
                                                                               : tc::Part::Startup, "a click");
        });
    });
    if (!s.live.made()) return;
    const tc::Texts t = texts(s);
    const std::vector<Rml::String> paragraphs(s.card.paragraphs.begin(), s.card.paragraphs.end());
    const Rml::String counter = tc::counter(s.card.number, s.card.count);
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    if (paragraphs != s.paragraphViews || counter != s.counterText || t.title != s.title || t.back != s.back ||
        t.next != s.next || t.startup != s.startupWord || t.backKey != s.backKey || t.nextKey != s.nextKey ||
        s.card.startup != s.startupOn || ground != s.ground) {
        s.paragraphViews = paragraphs, s.counterText = counter, s.title = t.title, s.back = t.back, s.next = t.next;
        s.startupWord = t.startup, s.backKey = t.backKey, s.nextKey = t.nextKey, s.startupOn = s.card.startup;
        s.ground = ground;
        s.live.doc.dirtyAll();
    }
    int x = s.layout.x, y = s.layout.y, bx = 0, by = 0, bw = 0, bh = 0;
    if (s.live.box(bx, by, bw, bh)) x = (s.targetW - bw) / 2, y = std::max(0, s.targetH / 3 - bh / 2);
    s.live.frame(s.shown, x, y, int(winstack::Win::Tip), ".btn, .tick, .x");
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

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (!s.shown || m != WM_KEYDOWN || (wp != VK_LEFT && wp != VK_RIGHT) || !chatwin::inputEmpty()) return false;
    press(s, wp == VK_LEFT ? tc::Part::Back : tc::Part::Next, wp == VK_LEFT ? "\xE2\x86\x90" : "\xE2\x86\x92");
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    press(s, tc::Part::Close, "Esc");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on) return;
    const uint32_t w = gameTip();
    if (w && read32(w + kShown) && write32(w + kShown, 0)) s.hid = true;
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    switch (c) {
        case PadCommand::Back: press(s, tc::Part::Back, "the controller"); break;
        case PadCommand::Next: press(s, tc::Part::Next, "the controller"); break;
        case PadCommand::Close: press(s, tc::Part::Close, "the controller"); break;
        case PadCommand::Startup: press(s, tc::Part::Startup, "the controller"); break;
    }
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

}  // namespace uagfx::win::tipwin
