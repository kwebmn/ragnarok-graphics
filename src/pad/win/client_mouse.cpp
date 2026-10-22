#include "pad/win/client_mouse.hpp"

#include <cstdint>

#include "pad/win/client_memory.hpp"

namespace uapad::clientmouse {
namespace {

// uaRO.exe / uaRO-1.1.exe: the global CMouse g_mouse and the game's resolution.
constexpr uintptr_t kMouse = 0x6ee6f0;
constexpr uintptr_t kCursorX = kMouse + 0x14;
constexpr uintptr_t kCursorY = kMouse + 0x18;
constexpr uintptr_t kGameWidth = 0x6ebe1c;
constexpr uintptr_t kGameHeight = 0x6ebe20;
constexpr int kMaxSide = 8192;

// CMouse::ReadState clamping the cursor (+0x14, +0x18) to the resolution (0x6ebe1c, 0x6ebe20), and the game
// mode calling it on g_mouse: mov ecx, 0x6ee6f0; call 0x419c60.
const uint8_t kSigClamp[] = {0x39, 0x5e, 0x14, 0x7d, 0x03, 0x89, 0x5e, 0x14, 0x39, 0x5e, 0x18, 0x7d, 0x03,
                             0x89, 0x5e, 0x18, 0xa1, 0x1c, 0xbe, 0x6e, 0x00, 0x8b, 0x4e, 0x14, 0x3b, 0xc8,
                             0x7c, 0x04, 0x48, 0x89, 0x46, 0x14, 0xa1, 0x20, 0xbe, 0x6e, 0x00};
const uint8_t kSigCall[] = {0xb9, 0xf0, 0xe6, 0x6e, 0x00, 0xe8, 0xcb, 0x2f, 0xeb, 0xff};

}  // namespace

bool supported() {
    return clientmem::matches(kSigClamp, sizeof kSigClamp, 0x419cf7) &&
           clientmem::matches(kSigCall, sizeof kSigCall, 0x566c8b);
}

bool read(int& x, int& y, int& width, int& height) {
    int32_t cx = 0;
    int32_t cy = 0;
    int32_t w = 0;
    int32_t h = 0;
    if (!clientmem::load(kCursorX, cx) || !clientmem::load(kCursorY, cy) || !clientmem::load(kGameWidth, w) ||
        !clientmem::load(kGameHeight, h))
        return false;
    if (w < 1 || w > kMaxSide || h < 1 || h > kMaxSide) return false;
    x = cx;
    y = cy;
    width = w;
    height = h;
    return true;
}

}  // namespace uapad::clientmouse
