#include "win/login_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/login_card.hpp"
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

namespace uagfx::win::loginwin {
namespace {

namespace lc = uagfx::logincard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the login window - where the manager keeps it, its class's vtable, its SendMsg (message 6 with a button's
// id); its fields (an edit control each: its words FUN_00443300, their count FUN_00443330 - thiscall), «Запам'ятати»;
// the manager's control with the keys (+ 0x6C) and its SetFocus
constexpr uint32_t kWindowMgr = 0x727ED8, kLoginAt = kWindowMgr + 0x88, kLoginClass = 0x697A70, kLoginSend = 0x457280;
constexpr uint32_t kAccountEdit = 0x64, kPasswordEdit = 0x60, kRemember = 0x68, kRect = 0x14, kSendMsgSlot = 0x94;
constexpr uint32_t kGetText = 0x443300, kTextLength = 0x443330, kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510;
constexpr int kButtonMsg = 6;
constexpr int kAway = -4000;
constexpr ULONGLONG kBlinkMs = 530;
const wchar_t kRegister[] = L"https://uaro.kiev.ua/account/register";
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using LengthFn = int(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);
using ShellExecuteFn = HINSTANCE(WINAPI*)(HWND, const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*, int);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false, moved = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    int oldX = 0;
    uint32_t window = 0;
    lc::Fields fields;
    lc::Show show;
    lc::Layout layout;
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

uint32_t gameLogin() {
    const uint32_t w = read32(kLoginAt);
    return w && read32(w) == kLoginClass && read32(read32(w) + kSendMsgSlot) == kLoginSend ? w : 0;
}

// Off the screen while ours shows (its fields keep the keys); back when ours is off
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
            LOG_INFO("login: the game's login window off the screen for the new one (its fields keep the keys)");
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        s.moved = false;
    }
}

lc::Fields fieldsOf(const State& s, uint32_t window) {
    lc::Fields f;
    const uint32_t account = read32(window + kAccountEdit), password = read32(window + kPasswordEdit);
    if (account) {
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(account);
        char buf[64] = {};
        if (t && readGame(uint32_t(reinterpret_cast<uintptr_t>(t)), buf, sizeof buf - 1)) {
            buf[sizeof buf - 1] = 0;
            f.account = utf8Of(widened(buf, s.codePage));
        }
    }
    if (password) f.passwordLength = reinterpret_cast<LengthFn>(uintptr_t(kTextLength))(password);
    const uint32_t focus = read32(kFocus);
    f.focus = focus && focus == account    ? lc::Focus::Account
              : focus && focus == password ? lc::Focus::Password
                                           : lc::Focus::None;
    f.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
    f.remember = read32(window + kRemember) != 0;
    return f;
}

lc::Texts texts() {
    lc::Texts t;
    t.title = "uaRO";
    t.subtitle = say(settings::Text::LoginSubtitle);
    t.account = say(settings::Text::LoginAccount);
    t.password = say(settings::Text::LoginPassword);
    t.remember = say(settings::Text::LoginRemember);
    t.enter = say(settings::Text::LoginEnter);
    t.exit = say(settings::Text::LoginExit);
    t.register_ = say(settings::Text::LoginRegister);
    return t;
}

std::vector<Words> gdiWords(const std::vector<lc::Words>& in) {
    std::vector<Words> out;
    for (const lc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == lc::Align::Right    ? DT_RIGHT
                                 : w.align == lc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the login", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    lc::paint(s.picture, s.layout, s.fields, s.show);
    drawWords(s.picture, gdiWords(lc::words(s.layout, s.fields, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::Login, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Login, nullptr, nullptr, false);
    s.shown = false;
}

void press(State& s, lc::Part p) {
    const uint32_t window = gameLogin();
    if (!window) return;
    if (p == lc::Part::Account || p == lc::Part::Password) {  // the keys to that field
        const uint32_t edit = read32(window + (p == lc::Part::Account ? kAccountEdit : kPasswordEdit));
        if (edit) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
        s.key.clear();
        return;
    }
    if (p == lc::Part::Register) {
        // shell32 asked for only now (the driver's own imports stay the four it has - as the notifications do)
        const HMODULE shell = LoadLibraryA("shell32.dll");
        const auto open = reinterpret_cast<ShellExecuteFn>(
            reinterpret_cast<void*>(shell ? GetProcAddress(shell, "ShellExecuteW") : nullptr));
        if (open) open(nullptr, L"open", kRegister, nullptr, nullptr, SW_SHOWNORMAL);
        LOG_INFO("login: the registration page %s (uaro.kiev.ua/account/register)", open ? "opened" : "not opened");
        return;
    }
    const int id = lc::gameButton(p);
    if (!id) return;
    const uint32_t send = read32(read32(window) + kSendMsgSlot);
    const int b = p == lc::Part::Remember ? (read32(window + kRemember) ? 0 : 1) : 0;  // the box: its new state
    if (s.told++ < 30)
        LOG_INFO("login: %s - the game's button 0x%X", p == lc::Part::Enter    ? "«Увійти»"
                                                       : p == lc::Part::Exit   ? "«Вийти»"
                                                                               : "«Запам'ятати логін»",
                 unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kButtonMsg, id, b, 0);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("login: [interface] login = %d", on ? 1 : 0);
    }
    const uint32_t window = gameLogin();
    hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    if (!s.shown || window != s.window) LOG_INFO("login: the game's login - the new one shows it");
    s.window = window;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.fields = fieldsOf(s, window);
    s.layout = lc::layout(targetWidth, targetHeight, s.scale);
    s.show.hover = 0;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.show.hover = int(lc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                       float(cy) * float(k) - float(s.layout.y)));
    const lc::Fields& f = s.fields;
    const std::string key = f.account + ';' + std::to_string(f.passwordLength) + ';' + std::to_string(int(f.focus)) +
                            (f.caret ? "|" : "") + (f.remember ? "r" : "") + ';' + std::to_string(s.layout.x) + ',' +
                            std::to_string(s.layout.y) + ';' + std::to_string(s.show.hover) + '#' +
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
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 14 * s.scale,
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
        const lc::Part p = lc::hitTest(s.layout, lx, ly);
        if (p != lc::Part::None) press(s, p);
    }
    return true;
}

bool shown() { return state().shown; }

void beforeGameDraw() {
    State& s = state();
    if (s.on) hold(s, gameLogin(), true);
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

}  // namespace uagfx::win::loginwin
