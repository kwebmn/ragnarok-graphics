#pragma once
#include <windows.h>

#include <cstdint>

// 3.0.18-beta.167 (step 3, the shared layer): two lines nearly every redrawn window had its own copy of
namespace uagfx::win {

// A press, a release or a double click of a mouse button (37 copies before)
inline bool mouseButton(UINT m) {
    return m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
           m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
}

// A box of the target's pixels (x, y, w, h) in the game window's `client` pixels, a pixel larger right and down - the
// place the game's mouse is kept from (49 copies before)
inline RECT clientBox(const RECT& client, int targetW, int targetH, float x, float y, float w, float h) {
    return RECT{LONG(int64_t(x) * client.right / targetW), LONG(int64_t(y) * client.bottom / targetH),
                LONG(int64_t(x + w) * client.right / targetW + 1), LONG(int64_t(y + h) * client.bottom / targetH + 1)};
}

}  // namespace uagfx::win
