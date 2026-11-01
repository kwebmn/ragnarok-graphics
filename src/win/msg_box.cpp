#include "win/msg_box.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/ask_card.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::msgboxwin {
namespace {

namespace ac = uagfx::askcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: ErrorMsg (thiscall, the manager; the words, the kind: 0 OK, 2 Yes and No, 4 OK and Cancel; three more)
// and the wait (the manager; the words, one more) - their first instructions, whole; the box at the manager's + 0x70,
// the error's class and the wait's, its SendMsg (message 6 with a button's id: the game's + 0x224 set, its wait ends);
// its Esc's answer + 0x58, its Enter's + 0x5C
constexpr uint32_t kErrorMsg = 0x4CEDF0, kWaitMsg = 0x4CEFA0, kWindowMgr = 0x727ED8, kBoxAt = kWindowMgr + 0x70;
const uint8_t kErrorCode[] = {0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00};
const uint8_t kWaitCode[] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF};
constexpr uint32_t kErrorClass = 0x697940, kWaitClass = 0x6978A8, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kEscAnswer = 0x58, kEnterAnswer = 0x5C;
constexpr int kButtonMsg = 6;
using ErrorFn = int(__attribute__((thiscall)) *)(uint32_t manager, const char* text, int kind, int a, int b, int c);
using WaitFn = int(__attribute__((thiscall)) *)(uint32_t manager, const char* text, int a);
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

uintptr_t g_error = 0, g_wait = 0;  // the copies of their first instructions, then on to the rest

struct State {
    WindowShadow shadow;
    bool on = false, shown = false, hid = false, padNow = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::string text;  // the box's words as the game gave them (its code page)
    ac::Kind kind = ac::Kind::Message;
    uint32_t box = 0;
    ac::Card card;
    ac::Show show;
    ac::Layout layout;
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

std::string copied(const char* text) {
    if (!text) return "";
    char buf[600] = {};
    if (!readGame(uint32_t(reinterpret_cast<uintptr_t>(text)), buf, sizeof buf - 1)) return "";
    buf[sizeof buf - 1] = 0;
    return buf;
}

int __attribute__((thiscall)) errorMsg(uint32_t manager, const char* text, int kind, int a, int b, int c) {
    State& s = state();
    const std::string words = copied(text);
    if (read32(kBoxAt)) {  // a box up already: the game adds the line to it
        if (!words.empty()) s.text += "\n" + words;
    } else {
        s.text = words;
        s.kind = kind == 2 ? ac::Kind::Question : kind == 4 ? ac::Kind::Choice : ac::Kind::Message;
    }
    if (s.told++ < 30) LOG_INFO("messages: the game's box (kind %d) - «%s»", kind, words.c_str());
    return reinterpret_cast<ErrorFn>(g_error)(manager, text, kind, a, b, c);
}

int __attribute__((thiscall)) waitMsg(uint32_t manager, const char* text, int a) {
    State& s = state();
    if (!read32(kBoxAt)) {
        s.text = copied(text);
        s.kind = ac::Kind::Wait;
        if (s.told++ < 30) LOG_INFO("messages: the game's wait - «%s»", s.text.c_str());
    }
    return reinterpret_cast<WaitFn>(g_wait)(manager, text, a);
}

// A function's first instructions to a copy that goes on to the rest, a jump to ours in their place; its copy's
// address, 0 when the bytes are not this build's
uintptr_t detour(uint32_t at, const uint8_t* code, size_t size, const void* ours) {
    uint8_t now[16] = {};
    SIZE_T got = 0;
    if (size > sizeof now || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), now,
                                                size, &got) ||
        got != size || std::memcmp(now, code, size) != 0)
        return 0;
    auto* t = static_cast<uint8_t*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!t) return 0;
    std::memcpy(t, code, size);
    t[size] = 0xE9;
    const int32_t back = int32_t(uintptr_t(at) + size - (reinterpret_cast<uintptr_t>(t) + size + 5));
    std::memcpy(t + size + 1, &back, 4);
    FlushInstructionCache(GetCurrentProcess(), t, 32);
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(uintptr_t(at)), size, PAGE_EXECUTE_READWRITE, &old)) return 0;
    uint8_t jump[16];
    std::memset(jump, 0x90, size);  // the rest of the first instructions nothing
    jump[0] = 0xE9;
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(ours) - (uintptr_t(at) + 5));
    std::memcpy(jump + 1, &to, 4);
    std::memcpy(reinterpret_cast<void*>(uintptr_t(at)), jump, size);
    DWORD unused = 0;
    VirtualProtect(reinterpret_cast<void*>(uintptr_t(at)), size, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), size);
    return reinterpret_cast<uintptr_t>(t);
}

// The game's box up now: the error's or the wait's
uint32_t gameBox(bool& wait) {
    const uint32_t b = read32(kBoxAt);
    if (!b) return 0;
    const uint32_t cls = read32(b);
    wait = cls == kWaitClass;
    return cls == kErrorClass || cls == kWaitClass ? b : 0;
}

std::string utf8(const State& s, const std::string& game) { return utf8Of(widened(game, s.codePage)); }

ac::Texts texts(const State& s) {
    ac::Texts t;
    t.ok = say(settings::Text::AskOk);
    t.yes = say(settings::Text::MsgYes);
    t.no = say(settings::Text::MsgNo);
    t.cancel = say(settings::Text::MsgCancel);
    t.acceptKey = s.padNow ? uapad::buttonLabel(0) : "Enter";
    t.okKey = s.padNow ? uapad::buttonLabel(0) : "Enter";
    t.declineKey = s.padNow ? uapad::buttonLabel(1) : "Esc";
    return t;
}

std::vector<Words> gdiWords(const std::vector<ac::Words>& in) {
    std::vector<Words> out;
    for (const ac::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ac::Align::Right    ? DT_RIGHT
                                 : w.align == ac::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            w.wrap});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("a message", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    ac::paint(s.picture, s.layout, s.card, s.show);
    drawWords(s.picture, gdiWords(ac::words(s.layout, s.card, s.show, texts(s))));
    s.upload = true;
}

// The message's rows at the card's width, word by word as GDI measures them (its own line breaks kept)
int rowsOf(const State& s, const std::string& utf8Text) {
    const float width = ac::messageWidth(s.scale), px = 13 * s.scale;
    int rows = 0;
    size_t at = 0;
    while (at <= utf8Text.size()) {
        size_t end = utf8Text.find('\n', at);
        if (end == std::string::npos) end = utf8Text.size();
        const std::string line = utf8Text.substr(at, end - at);
        float x = 0;
        int lineRows = 1;
        size_t from = 0;
        while (from < line.size()) {
            size_t sp = line.find(' ', from);
            sp = sp == std::string::npos ? line.size() : sp + 1;
            const float ww = float(wordsWidth(utf8Wide(line.substr(from, sp - from)), px, false));
            if (x > 0 && x + ww > width) {
                ++lineRows;
                x = 0;
            }
            x += ww;
            from = sp;
        }
        rows += lineRows;
        at = end + 1;
        if (end == utf8Text.size()) break;
    }
    return std::max(1, rows);
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(Blocker::Message, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Message, nullptr, nullptr, false);
    s.shown = false;
}

void pressId(State& s, int id, const char* why) {
    bool wait = false;
    const uint32_t b = gameBox(wait);
    if (!b || wait || !id) return;
    const uint32_t send = read32(read32(b) + kSendMsgSlot);
    if (s.told++ < 30) LOG_INFO("messages: the box answered 0x%X (%s)", unsigned(id), why);
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(b, 0, kButtonMsg, id, 0, 0);
    s.key.clear();
}

void press(State& s, ac::Button b) { pressId(s, ac::gameButton(b), "its button"); }

}  // namespace

void install() {
    static bool done = false;
    if (done) return;
    done = true;
    g_error = detour(kErrorMsg, kErrorCode, sizeof kErrorCode, reinterpret_cast<const void*>(&errorMsg));
    g_wait = detour(kWaitMsg, kWaitCode, sizeof kWaitCode, reinterpret_cast<const void*>(&waitMsg));
    LOG_INFO("messages: the game's message box's words %s, its wait's %s",
             g_error ? "caught" : "not caught (another build)", g_wait ? "caught" : "not caught (another build)");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("messages: [interface] messages = %d", on ? 1 : 0);
    }
    bool wait = false;
    const uint32_t box = g_error || g_wait ? gameBox(wait) : 0;
    if (box) {  // the game's box hidden while ours shows; back when ours is off
        if (on && read32(box + kShown) && write32(box + kShown, 0)) s.hid = true;
        else if (!on && s.hid && write32(box + kShown, 1)) s.hid = false;
    } else {
        s.hid = false;
    }
    if (!on || !box || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || s.text.empty()) {
        hide(s);
        s.box = 0;
        return;
    }
    if (!s.shown || box != s.box) s.show = ac::Show{};
    s.box = box;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.padNow = uapad::runtime::look().shown;
    s.card = ac::Card{};
    s.card.kind = wait ? ac::Kind::Wait : s.kind;
    s.card.title = say(wait ? settings::Text::MsgWaitTitle : settings::Text::MsgTitle);
    s.card.message = utf8(s, s.text);
    s.card.messageLines = rowsOf(s, s.card.message);
    s.layout = ac::layout(s.card, targetWidth, targetHeight, s.scale, texts(s));
    s.show.hover = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.show.hover = ac::buttonAt(s.layout, float(cx) * float(k) - float(s.layout.x),
                                    float(cy) * float(k) - float(s.layout.y));
    const std::string key = std::to_string(int(s.card.kind)) + ';' + s.card.message + ';' +
                            std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' +
                            std::to_string(s.show.hover) + (s.padNow ? "P" : "") + '#' +
                            std::to_string(uistyle::generation());
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
        const int i = ac::buttonAt(s.layout, lx, ly);
        const std::vector<ac::Button> bs = ac::buttons(s.card.kind);
        if (i >= 0 && i < int(bs.size())) press(s, bs[size_t(i)]);
    }
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (!s.shown || s.card.kind == ac::Kind::Wait || m != WM_KEYDOWN || (wp != VK_RETURN && wp != VK_ESCAPE))
        return false;
    bool wait = false;
    const uint32_t b = gameBox(wait);
    if (!b || wait) return false;
    pressId(s, int(read32(b + (wp == VK_RETURN ? kEnterAnswer : kEscAnswer))), wp == VK_RETURN ? "Enter" : "Esc");
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown || s.card.kind == ac::Kind::Wait) return false;
    bool wait = false;
    const uint32_t b = gameBox(wait);
    if (!b || wait) return false;
    pressId(s, int(read32(b + kEscAnswer)), "Esc");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    bool wait = false;
    if (!s.on) return;
    if (const uint32_t b = gameBox(wait)) {
        if (read32(b + kShown) && write32(b + kShown, 0)) s.hid = true;
    }
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const std::vector<ac::Button> bs = ac::buttons(s.card.kind);
    const size_t i = c == PadCommand::First ? 0 : 1;
    if (i < bs.size()) press(s, bs[i]);
    else if (c == PadCommand::Second) close();
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

}  // namespace uagfx::win::msgboxwin
