#pragma once
#include <cstdint>
#include <set>

namespace uagfx {

// Remembers which (place, code) pairs were reported, so a failure that repeats every frame is logged once.
class LogOnce {
public:
    bool first(uint32_t place, uint32_t code) { return seen_.insert(uint64_t(place) << 32 | code).second; }

private:
    std::set<uint64_t> seen_;
};

}  // namespace uagfx
