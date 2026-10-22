#include "pad/core/call_targets.hpp"

#include <algorithm>

#include "pad/core/bytes.hpp"

namespace uapad {

std::vector<uint32_t> indirectCallTargets(const uint8_t* code, size_t len, uint32_t lo, uint32_t hi) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i + 6 <= len; ++i) {
        if (code[i] != 0xff || code[i + 1] != 0x15) continue;
        const uint32_t x = rd32(code, i + 2);
        if (x >= lo && x < hi) out.push_back(x);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

}  // namespace uapad
