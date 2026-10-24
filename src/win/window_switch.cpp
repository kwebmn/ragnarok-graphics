#include "win/window_switch.hpp"

#include <algorithm>

#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/log_slot.hpp"

namespace uagfx::win {
namespace {

WindowSwitch* g_switch = nullptr;
WNDPROC g_original = nullptr;

LRESULT CALLBACK gameProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    if ((m == WM_KEYDOWN || m == WM_SYSKEYDOWN) && g_switch && g_switch->key(unsigned(wp), lp)) return 0;
    const LRESULT r = CallWindowProcA(g_original, w, m, wp, lp);
    // the panel over it while the game window is dragged too: Windows' loop then, not the game's frames
    if ((m == WM_MOVE || m == WM_SIZE) && g_switch) g_switch->moved();
    return r;
}

}  // namespace

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
    gamewindows::putName(mine_, utf8Of(widened(name, gameCodePage(config))));
    charstatus::Status s;
    gamewindows::putStatus(mine_, !name.empty() && readOwnStatus(s) ? &s : nullptr);
    publish(foreground);
    panel_.update(shared_.live(), mine_.pid);
}

void WindowSwitch::publish(bool frontNow) {
    if (frontNow) mine_.frontAt = GetTickCount64();
    shared_.publish(mine_);
}

bool WindowSwitch::key(unsigned vk, LPARAM lParam) {
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
    const bool ok = switchTo(*w);
    LOG_INFO("windows: %s - to window %d%s", by, number, ok ? "" : " (Windows did not bring it to the front)");
}

}  // namespace uagfx::win
