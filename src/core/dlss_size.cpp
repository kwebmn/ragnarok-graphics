#include "core/dlss_size.hpp"

#include <algorithm>
#include <cstdlib>

namespace uagfx::dlss {

Size asked(int outW, int outH, int percent, int width, int height) {
    Size s;
    if (width > 0 && height > 0) {
        s = {width, height};
    } else if (width > 0 && outW > 0) {
        s = {width, int((long long)width * outH * 2 / outW + 1) / 2};
    } else if (height > 0 && outH > 0) {
        s = {int((long long)height * outW * 2 / outH + 1) / 2, height};
    } else {
        s = {int((long long)outW * percent * 2 / 100 + 1) / 2, int((long long)outH * percent * 2 / 100 + 1) / 2};
    }
    s.width = std::clamp(s.width, 1, std::max(outW, 1));
    s.height = std::clamp(s.height, 1, std::max(outH, 1));
    return s;
}

Pick pick(const std::vector<Range>& modes, Size want) {
    Pick best;
    long long bestOff = 0, bestOwn = 0;
    for (const Range& r : modes) {
        const Size taken{std::clamp(want.width, r.minWidth, std::max(r.minWidth, r.maxWidth)),
                         std::clamp(want.height, r.minHeight, std::max(r.minHeight, r.maxHeight))};
        const long long off = std::llabs(taken.width - want.width) + std::llabs(taken.height - want.height);
        const long long own = std::llabs(r.width - want.width) + std::llabs(r.height - want.height);
        if (best.mode == Mode::None || off < bestOff || (off == bestOff && own < bestOwn)) {
            best = {r.mode, taken};
            bestOff = off;
            bestOwn = own;
        }
    }
    return best;
}

}  // namespace uagfx::dlss
