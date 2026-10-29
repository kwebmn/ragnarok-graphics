#include "win/window_switch.hpp"

#include <algorithm>

#include "pad/win/game_window.hpp"
#include "win/answer_card.hpp"
#include "win/minimap_proto.hpp"
#include "win/reference_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/log_slot.hpp"
#include "win/notifier.hpp"

namespace uagfx::win {
namespace {

constexpr UINT_PTR kLeaveTimer = 0x75A9;  // the focus gone from the full screen: where to (WM_TIMER)
constexpr UINT_PTR kBehindTimer = 0x75AA;  // 3.0.3-beta.12: behind another window - the presents' work by the clock

WindowSwitch* g_switch = nullptr;
WNDPROC g_original = nullptr;
uint32_t g_diedAt = 0, g_hitAt = 0, g_whisperAt = 0;  // 3.0.4-beta.4: windowmarks' (the game's thread)

// 3.0.11-beta.33 (the owner: «при нажатии на кнопку закрыть справочник персонаж идет по локации»): a press one of the
// driver's parts took is not the game's until it is let go - the game reads the mouse (DirectInput) after this
// message, when the part may be gone already (its ✕ shuts it on the press), and saw the button go down over the world
void claimed(UINT m) {
    const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;  // the messages' left is the device's right then
    switch (m) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: uapad::gamewindow::claimPress(swapped ? 2 : 1); break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK: uapad::gamewindow::claimPress(swapped ? 1 : 2); break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK: uapad::gamewindow::claimPress(4); break;
        default: break;
    }
}

LRESULT CALLBACK gameProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    if (g_switch) {
        if (referencewin::key(m, wp, lp)) return 0;  // 3.0.11-beta.7: Ctrl+F, and every key while «Довідник» is open
        if (m == WM_KEYDOWN && minimapproto::key(unsigned(wp))) return 0;  // 3.0.11-beta.4: Ctrl+Tab, the new minimap's
        if ((m == WM_KEYDOWN || m == WM_SYSKEYDOWN) && g_switch->key(unsigned(wp), lp)) return 0;
        if (m == WM_ACTIVATE && g_switch->activate(w, wp)) return 1;  // the game's own answer to it
        if (m == WM_TIMER && g_switch->timer(w, wp)) return 0;
        if (notifier::message(w, m, wp, lp)) return 0;  // 3.0.4-beta.3: a click on a notification
        if (m == WM_DESTROY) notifier::gone(w);
        LRESULT answer = 0;  // 3.0.3-beta.8: the mouse over the panel in the game's picture is the panel's
        if (answercard::mouse(w, m, lp, answer) ||       // 3.0.9-beta.4: and over the answer card the card's
            referencewin::mouse(w, m, wp, lp, answer) ||  // 3.0.11-beta.7: over «Довідник» its own
            minimapproto::mouse(w, m, wp, lp, answer) ||  // 3.0.11-beta.1: the new minimap's
            g_switch->mouse(w, m, lp, answer)) {
            claimed(m);  // 3.0.11-beta.33: its press stays the part's until let go
            return answer;
        }
    }
    return CallWindowProcA(g_original, w, m, wp, lp);
}

// Window a is above window b (nearer the top of Windows' order)
bool above(HWND a, HWND b) {
    int steps = 0;
    for (HWND w = GetWindow(b, GW_HWNDPREV); w && steps < 1024; w = GetWindow(w, GW_HWNDPREV), ++steps)
        if (w == a) return true;
    return false;
}

}  // namespace

namespace windowmarks {
void died() { g_diedAt = gamewindows::stamp(GetTickCount64()); }
void hit() { g_hitAt = gamewindows::stamp(GetTickCount64()); }
void whispered() { g_whisperAt = gamewindows::stamp(GetTickCount64()); }
}  // namespace windowmarks

WindowSwitch::WindowSwitch() {
    panel_.onPick = [this](int number) { to(number, "the panel"); };
}

WindowSwitch::~WindowSwitch() {
    if (g_switch == this) g_switch = nullptr;
    shared_.leave(int(mine_.number));
}

void WindowSwitch::check(HWND window, bool foreground, const std::string& name, const Config& config) {
    keys_ = config.windowKeys;
    if (!window_ && window) {  // the game's one window, ours to see its keys first
        const LONG_PTR prev = SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&gameProc));
        if (prev) {
            g_original = reinterpret_cast<WNDPROC>(prev);
            g_switch = this;
            window_ = window;
            LOG_INFO("windows: this game's window among the player's - its keys %s", windowKeysToken(keys_));
        }
    }
    mine_.pid = GetCurrentProcessId();
    mine_.window = uint32_t(reinterpret_cast<uintptr_t>(window));
    if (mine_.number == 0) mine_.number = uint32_t(wlog::slot() > 0 ? wlog::slot() : shared_.claim());
    if (mine_.number == 0) return;  // eight games run: no place for a ninth
    mine_.flags = fullScreen_ ? 0 : gamewindows::kDrawsBehind;  // on its full screen the game draws only in front
    gamewindows::putName(mine_, utf8Of(widened(name, gameCodePage(config))));
    charstatus::Status s;
    gamewindows::putStatus(mine_, !name.empty() && readOwnStatus(s) ? &s : nullptr);
    mine_.diedAt = g_diedAt;  // 3.0.4-beta.4
    mine_.hitAt = g_hitAt;
    mine_.whisperAt = g_whisperAt;
    publish(foreground);
    const std::vector<gamewindows::Window> all = shared_.live();
    // wholly behind another of the player's game windows, that window above it: nobody sees it
    bool hidden = false;
    RECT me{};
    if (window && !foreground && GetWindowRect(window, &me)) {
        const gamewindows::Box mine{me.left, me.top, me.right, me.bottom};
        for (const gamewindows::Window& w : all) {
            const HWND h = reinterpret_cast<HWND>(uintptr_t(w.window));
            RECT r{};
            if (w.pid == mine_.pid || !IsWindowVisible(h) || IsIconic(h) || !GetWindowRect(h, &r)) continue;
            if (gamewindows::covers(gamewindows::Box{r.left, r.top, r.right, r.bottom}, mine) && above(h, window)) {
                hidden = true;
                break;
            }
        }
    }
    hidden_ = hidden;
    panel_.update(all, mine_.pid);
}

void WindowSwitch::frame(HWND window, bool main, Config& config, double k, int targetWidth, int targetHeight,
                         bool fullScreen) {
    fullScreen_ = fullScreen;
    panel_.frame(window, main, config, k, targetWidth, targetHeight);
    if (!pendingNumber_) return;
    const uint64_t now = GetTickCount64();
    const uint32_t ready = shared_.readyOf(pendingNumber_);
    if (!gamewindows::switchNow(ready, uint32_t(pendingSerial_), pendingSince_, now)) return;
    const int number = pendingNumber_;
    pendingNumber_ = 0;
    show(number, pendingBy_, ready == uint32_t(pendingSerial_), now - pendingSince_);
}

void WindowSwitch::publish(bool frontNow) {
    if (frontNow) mine_.frontAt = GetTickCount64();
    if (mine_.number) shared_.publish(mine_);
}

bool WindowSwitch::key(unsigned vk, LPARAM lParam) {
    if (vk == VK_ESCAPE && panel_.closeMenu()) return true;  // 3.0.3-beta.10: its menu closed, not the game's opened
    const gamewindows::KeyAction a = gamewindows::keyAction(vk, GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_MENU) < 0,
                                                            GetKeyState(VK_SHIFT) < 0, keys_);
    if (a.action == gamewindows::Action::None || (a.action == gamewindows::Action::Panel && !panel_.on())) return false;
    if (lParam & (1 << 30)) return true;  // held down: taken as its first press was, nothing done again
    if (a.action == gamewindows::Action::Panel)
        panel_.toggle();  // 3.0.3-beta.7: Ctrl+Alt+0 shows and hides the windows' panel
    else
        act(a);
    return true;
}

void WindowSwitch::act(const gamewindows::KeyAction& a) {
    if (a.action == gamewindows::Action::Window) {
        to(a.number, "Ctrl+Alt+digit");
        return;
    }
    const std::vector<gamewindows::Window> all = shared_.live();
    const gamewindows::Window* back = gamewindows::previous(all, GetCurrentProcessId());
    if (back)
        to(back->number, "Ctrl+Tab");
    else
        LOG_INFO("windows: Ctrl+Tab - no other game window");
}

void WindowSwitch::to(int number, const char* by) {
    const DWORD self = GetCurrentProcessId();
    const std::vector<gamewindows::Window> all = shared_.live();
    const gamewindows::Window* w = gamewindows::byNumber(all, number);
    if (!w || w->pid == self) {
        LOG_INFO("windows: %s, window %d - %s", by, number, w ? "this window" : "no game window of that number");
        return;
    }
    publish(true);  // in front till now: Ctrl+Tab in the other one comes back here
    // 3.0.3-beta.9: one that draws behind this one is asked to get its picture whole first, and woken to it; a
    // minimised one, or one on its full screen (it draws nothing while not active), is shown at once
    const bool asks = w->drawsBehind && !IsIconic(reinterpret_cast<HWND>(uintptr_t(w->window)));
    const int32_t serial = asks ? shared_.summon(w->pid) : 0;
    if (!serial) {
        show(number, by, false, 0);
        return;
    }
    pendingNumber_ = number;
    pendingSerial_ = serial;
    pendingSince_ = GetTickCount64();
    pendingBy_ = by;
    wake(w->pid);
}

void WindowSwitch::show(int number, const char* by, bool ready, uint64_t waited) {
    const std::vector<gamewindows::Window> all = shared_.live();
    const gamewindows::Window* w = gamewindows::byNumber(all, number);
    if (!w) {
        LOG_INFO("windows: %s - window %d is gone", by, number);
        return;
    }
    const bool ok = switchTo(*w);
    LOG_INFO("windows: %s - to window %d, %s in %llu ms%s", by, number,
             ready ? "its picture whole" : "shown as it is", static_cast<unsigned long long>(waited),
             ok ? "" : " (Windows did not bring it to the front)");
}

bool WindowSwitch::summoned(bool foreground) {
    int32_t serial = 0;
    if (shared_.summonFor(GetCurrentProcessId(), seenSummon_, serial)) {
        summonSerial_ = serial;
        summonedAt_ = GetTickCount64();
        wholeFrames_ = 0;
    }
    if (!summonedAt_) return false;
    if (foreground || GetTickCount64() - summonedAt_ >= gamewindows::kSummonKeepMs) {
        summonedAt_ = 0;  // in front now (the main one by itself), or the ask is old
        return false;
    }
    return true;
}

void WindowSwitch::presented(bool light) {
    if (!summonedAt_ || light || ++wholeFrames_ != 3) return;  // a frame or two for a new size and DLSS's history
    mine_.ready = uint32_t(summonSerial_);
    publish(false);
}

bool WindowSwitch::activate(HWND window, WPARAM wp) {
    if (LOWORD(wp) == WA_INACTIVE) {  // 3.0.3-beta.11: the panel's menu closed, the frame counter hidden
        panel_.closeMenu();
        if (onLeave) onLeave();
        SetTimer(window, kBehindTimer, 100, nullptr);  // 3.0.3-beta.12
    } else {
        KillTimer(window, kBehindTimer);
    }
    if (!fullScreen_) return false;  // in a window the game does nothing there
    if (LOWORD(wp) == WA_INACTIVE) {  // not minimised at once: where the focus went decides (timer)
        leaveSince_ = GetTickCount64();
        SetTimer(window, kLeaveTimer, 50, nullptr);
        return true;
    }
    KillTimer(window, kLeaveTimer);
    leaveSince_ = 0;
    return !IsIconic(window);  // shown already: nothing to maximise; minimised (back from the desktop): the game's own
}

bool WindowSwitch::timer(HWND window, WPARAM id) {
    if (id == kBehindTimer) {  // 3.0.3-beta.12: its sounds' fade and its entry, while it presents no more
        if (GetForegroundWindow() == window)
            KillTimer(window, kBehindTimer);  // in front again: its presents do it
        else if (onBehind)
            onBehind();
        return true;
    }
    if (id != kLeaveTimer) return false;
    const HWND front = GetForegroundWindow();
    DWORD pid = 0;
    char cls[64] = {};
    if (front) {
        GetWindowThreadProcessId(front, &pid);
        GetClassNameA(front, cls, sizeof cls);
    }
    const gamewindows::Leave l = gamewindows::leaving(pid, cls, shared_.live(), GetCurrentProcessId());
    if (l == gamewindows::Leave::Wait && GetTickCount64() - leaveSince_ < 3000) return true;  // asked again
    KillTimer(window, kLeaveTimer);
    leaveSince_ = 0;
    if (l == gamewindows::Leave::Stay) return true;  // another game window: no blink
    ShowWindow(window, SW_MINIMIZE);  // the desktop, another program: as the game itself does
    return true;
}

}  // namespace uagfx::win
