#pragma once
#include <vector>

namespace uagfx::dlss {

// 2.4.2: DLSS at a size of the player's own ([quality] dlss = custom): a share of the screen or the pixels written by
// hand, in the mode whose range takes it (slDLSSGetOptimalSettings: each mode's size and the smallest and the largest
// it takes - DLSS draws any size between).
enum class Mode { None, Dlaa, Quality, Balanced, Performance, Ultra };

struct Size {
    int width = 0, height = 0;
    bool operator==(const Size& o) const { return width == o.width && height == o.height; }
};
struct Range {
    Mode mode = Mode::None;
    int width = 0, height = 0;        // the mode's own size
    int minWidth = 0, minHeight = 0;  // the smallest it takes
    int maxWidth = 0, maxHeight = 0;  // the largest
};
struct Pick {
    Mode mode = Mode::None;
    Size size;
};

// The size asked for on a screen (outW x outH): width and height when both are written, one of them with the screen's
// proportions, else percent of the screen; never past the screen, at least a pixel.
Size asked(int outW, int outH, int percent, int width, int height);
// The mode that takes the size, the nearest size it takes when none does; of those, the mode whose own size is the
// nearest. Mode::None when no mode was given.
Pick pick(const std::vector<Range>& modes, Size want);

}  // namespace uagfx::dlss
