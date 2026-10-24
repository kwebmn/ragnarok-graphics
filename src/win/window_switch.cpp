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
    return CallWindowProcA(g_original, w, m, wp, lp);
}

const char* what(const gamewindows::KeyAction& a) {
    return a.action == gamewindows::Action::Back ? "Ctrl+Tab" : "Ctrl+Alt+digit";
}

}  // namespace

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
            LOG_INFO("windows: this game is window %d among the player's; its keys %s", std::max(1, wlog::slot()),
                     windowKeysToken(keys_));
        }
    }
    mine_.pid = GetCurrentProcessId();
    mine_.window = uint32_t(reinterpret_cast<uintptr_t>(window));
    mine_.number = uint32_t(std::max(1, wlog::slot()));
    gamewindows::putName(mine_, utf8Of(widened(name, gameCodePage(config))));
    charstatus::Status s;
    gamewindows::putStatus(mine_, !name.empty() && readOwnStatus(s) ? &s : nullptr);
    publish(foreground);
}

void WindowSwitch::publish(bool frontNow) {
    if (frontNow) mine_.frontAt = GetTickCount64();
    shared_.publish(mine_);
}

bool WindowSwitch::key(unsigned vk, LPARAM lParam) {
    const gamewindows::KeyAction a = gamewindows::keyAction(vk, GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_MENU) < 0,
                                                            GetKeyState(VK_SHIFT) < 0, keys_);
    // Ctrl+Alt+0 shows and hides the panel of the windows - 3.0.3-beta.7's; the game's own key till then
    if (a.action == gamewindows::Action::None || a.action == gamewindows::Action::Panel) return false;
    if (lParam & (1 << 30)) return true;  // held down: taken as its first press was, nothing done again
    act(a);
    return true;
}

void WindowSwitch::act(const gamewindows::KeyAction& a) {
    const DWORD self = GetCurrentProcessId();
    const std::vector<gamewindows::Window> all = shared_.live();
    const gamewindows::Window* to = a.action == gamewindows::Action::Back ? gamewindows::previous(all, self)
                                                                          : gamewindows::byNumber(all, a.number);
    if (!to || to->pid == self) {
        if (a.action == gamewindows::Action::Back)
            LOG_INFO("windows: Ctrl+Tab - no other game window");
        else
            LOG_INFO("windows: Ctrl+Alt+%d - %s", a.number, to ? "this window" : "no game window of that number");
        return;
    }
    publish(true);  // in front till now: Ctrl+Tab in the other one comes back here
    const bool ok = switchTo(*to);
    LOG_INFO("windows: %s - to window %d%s", what(a), to->number,
             ok ? "" : " (Windows did not bring it to the front)");
}

}  // namespace uagfx::win
