#pragma once
#include <vector>

#include "core/config.hpp"
#include "core/image.hpp"

namespace uagfx {

// What happens to the game's window when the first frame is shown.
enum class WindowAction {
    Leave,       // the window stays as the game (and ROExt) made it: the picture is shown 1:1
    Window,      // a normal window of window_width x window_height (0 = the game's size), centred on the monitor
    Borderless,  // a borderless window over the whole monitor (the monitor's mode never changes)
};

// [display] mode = auto follows the game (spec §4.1): DDSCL_FULLSCREEN becomes borderless, a windowed game keeps its
// window unless window_width / window_height ask for another size. window and borderless override the game.
WindowAction windowAction(const Config& config, bool gameFullscreen);

// Where a width x height picture goes in an outWidth x outHeight window: as large as fits, shape kept, centred, on
// whole pixels. A fractional offset would make every output pixel sample between two source pixels.
Rect fitPicture(int width, int height, int outWidth, int outHeight);

// [display] monitor = N: 0 is the main monitor, then the others in Windows' order (1B review M9: EnumDisplayMonitors
// does not list the main one first). primary: a flag per monitor in enumeration order. Returns the enumeration index
// of monitor N, or -1 when there is no such monitor.
int monitorIndex(const std::vector<bool>& primary, int wanted);

}  // namespace uagfx
