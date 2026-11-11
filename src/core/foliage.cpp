#include "core/foliage.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::foliage {

bool leafy(const uint32_t* argb, int width, int height) {
    if (!argb || width < 8 || height < 8) return false;
    uint64_t holes = 0, solid = 0, r = 0, g = 0, b = 0;
    const size_t n = size_t(width) * size_t(height);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = argb[i];
        if ((p >> 24) < 128) {
            ++holes;
            continue;
        }
        ++solid;
        r += p >> 16 & 255;
        g += p >> 8 & 255;
        b += p & 255;
    }
    if (!solid) return false;
    const double hole = double(holes) / double(n);
    const double mr = double(r) / solid, mg = double(g) / solid, mb = double(b) / solid;
    // Leaves: holes between them (not a solid wall, not a lone small shape), green over red and blue.
    // 3.0.11-beta.300 (the players: «в пронтере лавочка вместе с травой движется»): and a leaf's green is a yellow one
    // - Prontera's wrought bench (외부소품\pron-bench1-3.bmp: R 20-28, B 32-44) is painted a blue green; the client's
    // 45 leaves' textures have blue at most 2 over red (867 of its trees' and props' textures looked at)
    return hole > 0.12 && hole < 0.85 && mg > mr * 1.08 && mg > mb * 1.15 && mg > 40.0 && mb <= mr + 8.0;
}

bool leafyTexture(const uint32_t* argb, int width, int height, bool spriteCache) {
    return !spriteCache && leafy(argb, width, height);
}

}  // namespace uagfx::foliage
