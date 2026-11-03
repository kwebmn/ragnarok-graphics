#pragma once
#include <cstdint>
#include <cstdlib>

namespace uagfx::dblclick {

// 3.0.11-beta.202 (the tester: «треба повернути знімання дабл кліком по речі»): the game's window class has no
// CS_DBLCLKS - Windows never sends WM_LBUTTONDBLCLK; the driver's parts get one where a left press comes within
// `limitMs` of the one before and within `slopX` / `slopY` pixels of it (Windows' own double-click time and box); the
// press after a double click begins again
struct Clicks {
    int64_t at = -1;  // the first press's time; -1 none waiting for its second
    int x = 0, y = 0;
};
inline bool second(Clicks& c, int64_t now, int x, int y, int limitMs, int slopX, int slopY) {
    if (c.at >= 0 && now - c.at <= limitMs && std::abs(x - c.x) <= slopX && std::abs(y - c.y) <= slopY) {
        c.at = -1;
        return true;
    }
    c.at = now;
    c.x = x;
    c.y = y;
    return false;
}

}  // namespace uagfx::dblclick
