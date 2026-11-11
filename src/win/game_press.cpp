#include "win/game_press.hpp"

#include <cstdint>
#include <cstring>

#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"

namespace uagfx::win::gamepress {
namespace {

// uaRO.exe's: the windows' manager, its windows' list (a node: next +0, previous +4, the window +8), a window's hit
// test (thiscall, the screen's x, y: the window or its child there, or 0) and its first bytes; a window's place +0x14,
// +0x18 and size +0x1C, +0x20
constexpr uint32_t kWindowMgr = 0x727ED8, kList = 0x38, kHitTest = 0x4CB550;
constexpr uint8_t kHitBytes[] = {0x55, 0x8B, 0xEC, 0x53, 0x56, 0x8B, 0xF1, 0x57, 0x8B, 0x06, 0xFF, 0x50, 0x28};
constexpr size_t kMostWindows = 128;
constexpr int kMostTold = 8;
using HitFn = uint32_t(__attribute__((thiscall)) *)(uint32_t window, int x, int y);

int g_told = 0;

bool known() {
    static int k = -1;  // 1 this build's, 0 not
    if (k >= 0) return k == 1;
    uint8_t bytes[sizeof kHitBytes] = {};
    k = readGame(kHitTest, bytes, sizeof bytes) && std::memcmp(bytes, kHitBytes, sizeof bytes) == 0 ? 1 : 0;
    if (!k)
        LOG_INFO("windows: the game's window hit test is not where this build has it (0x%X) - a press over its own "
                 "windows drawn over the driver's stays the driver's", kHitTest);
    return k == 1;
}

// The top window of the game's own at the game's (x, y): the list's last first, as the game's drop looks
uint32_t windowAt(int x, int y) {
    const uint32_t head = read32(kWindowMgr + kList);
    if (!head) return 0;
    const uint32_t first = read32(head);
    uint32_t node = head;
    for (size_t n = 0; n < kMostWindows; ++n) {
        const uint32_t prev = read32(node + 4);
        if (!prev || prev == head) break;
        const uint32_t window = read32(prev + 8);
        if (window && reinterpret_cast<HitFn>(uintptr_t(kHitTest))(window, x, y)) return window;
        node = prev;
        if (node == first) break;
    }
    return 0;
}

}  // namespace

bool pressOver(UINT m) {
    const bool press = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN || m == WM_RBUTTONDBLCLK ||
                       m == WM_MBUTTONDOWN || m == WM_MBUTTONDBLCLK || m == WM_MOUSEWHEEL;
    if (!press || !inGameWorld() || !known()) return false;
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy)) return false;
    const uint32_t window = windowAt(cx, cy);
    if (!window) return false;
    if (g_told < kMostTold) {
        ++g_told;
        int32_t box[4] = {};
        readGame(window + 0x14, box, sizeof box);
        LOG_INFO("windows: a press (0x%X) at (%d, %d) over the game's own window (its class 0x%X at %d, %d %dx%d) - "
                 "the game's, not the driver's part under it", m, cx, cy, read32(window), box[0], box[1], box[2],
                 box[3]);
    }
    return true;
}

}  // namespace uagfx::win::gamepress
