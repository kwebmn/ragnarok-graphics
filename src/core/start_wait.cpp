#include "core/start_wait.hpp"

#include <algorithm>

namespace uagfx::startwait {

uint32_t Wait::left(uint64_t nowMs) const {
    const uint64_t end = std::min(startMs + kMostMs, heardMs + kQuietMs);
    return nowMs >= end ? 0 : uint32_t(std::min<uint64_t>(end - nowMs, kMostMs));
}

}  // namespace uagfx::startwait
