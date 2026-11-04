#include "win/tip_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

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
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

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
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
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

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
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

std::vector<Words> gdiWords(const std::vector<tc::Words>& in) {
    std::vector<Words> out;
    for (const tc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == tc::Align::Right    ? DT_RIGHT
                                 : w.align == tc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            w.wrap});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the tip of the day", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    tc::paint(s.picture, s.layout, s.card, s.show);
    drawWords(s.picture, gdiWords(tc::words(s.layout, s.card, s.show, texts(s))));
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

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
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
    if (readGameCursor(cx, cy))
        s.show.hover = tc::partAt(s.layout, float(cx) * float(k) - float(s.layout.x),
                                  float(cy) * float(k) - float(s.layout.y));
    const std::string key = std::to_string(s.number) + (s.card.startup ? "S" : "") + ';' +
                            std::to_string(int(s.show.hover)) + ';' + std::to_string(s.layout.x) + ',' +
                            std::to_string(s.layout.y) + ',' + std::to_string(s.layout.w) + (s.padNow ? "P" : "") +
                            '#' + std::to_string(uistyle::generation());
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
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        const tc::Part p = tc::partAt(s.layout, lx, ly);
        if (p != tc::Part::None) press(s, p, "a click");
    }
    return true;
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
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::tipwin
