#include "core/depth_sort.hpp"

#include <cstring>
#include <limits>

namespace uagfx::depthsort {
namespace {

bool notANumber(float f) {  // by its bits (the build's float options left aside)
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof bits);
    return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0;
}

}  // namespace

int mend(Pair* first, Pair* last) {
    int n = 0;
    for (Pair* p = first; first && p < last; ++p)
        if (notANumber(p->depth)) {
            p->depth = -std::numeric_limits<float>::max();
            ++n;
        }
    return n;
}

}  // namespace uagfx::depthsort
