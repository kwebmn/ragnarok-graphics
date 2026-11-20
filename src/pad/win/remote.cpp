#include "pad/win/remote.hpp"

#include <windows.h>

#include <atomic>

namespace uapad::remote {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;  // MinGW here has no std::mutex
Mouse g_mouse;
std::atomic<uint8_t> g_mods{0};  // 1 Ctrl, 2 Shift, 4 Alt
std::atomic<bool> g_polled{false};

}  // namespace

void setMouse(const Mouse& m) {
    AcquireSRWLockExclusive(&g_lock);
    g_mouse = m;
    ReleaseSRWLockExclusive(&g_lock);
}

Mouse mouse() {
    AcquireSRWLockShared(&g_lock);
    const Mouse m = g_mouse;
    ReleaseSRWLockShared(&g_lock);
    return m;
}

void setMods(bool ctrl, bool shift, bool alt) {
    g_mods = uint8_t((ctrl ? 1 : 0) | (shift ? 2 : 0) | (alt ? 4 : 0));
}

bool modDown(int vk) {
    const uint8_t m = g_mods;
    switch (vk) {
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL: return (m & 1) != 0;
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT: return (m & 2) != 0;
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU: return (m & 4) != 0;
        default: return false;
    }
}

void notePolled() { g_polled = true; }
bool polled() { return g_polled; }

}  // namespace uapad::remote
