#include "core/grid_cell.hpp"

#include <cstring>

namespace uagfx::gridcell {

void original(uint8_t out[9]) {
    out[0] = 0xA1;  // mov eax, [kFlag]
    std::memcpy(out + 1, &kFlag, 4);
    out[5] = 0x85;  // test eax, eax
    out[6] = 0xC0;
    out[7] = 0x75;  // jne +0x27
    out[8] = 0x27;
}

bool hides(bool arrowWalk, bool padShown) { return arrowWalk && padShown; }

uint32_t answer(uint32_t gameFlag, bool hidden) { return gameFlag != 0 || hidden ? 1u : 0u; }

}  // namespace uagfx::gridcell
