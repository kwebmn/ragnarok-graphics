#include "core/effect_exe.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::effect_exe {

Kind kindOf(int id) {
    switch (id) {
    case 200:
    case 201:
    case 202:
    case 362:
    case 397:
    case 398:
        return Kind::Aura;
    default:
        return Kind::None;
    }
}

void soften(uint8_t* vertices, uint32_t count, uint32_t stride, int percent) {
    if (!vertices || stride < 20 || percent >= 100) return;
    const float f = std::sqrt(float(std::max(percent, 0)) / 100.0f);
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t* at = vertices + size_t(i) * stride + 16;
        uint32_t c = 0;
        std::memcpy(&c, at, 4);
        uint32_t out = 0;
        for (int shift = 0; shift < 32; shift += 8)
            out |= uint32_t(float((c >> shift) & 255) * f + 0.5f) << shift;
        std::memcpy(at, &out, 4);
    }
}

void Prims::note(uint32_t prim, Kind kind) {
    if (kind == Kind::None) return;
    if (!prims_.empty() && prim < prims_.back().first) sorted_ = false;
    prims_.emplace_back(prim, kind);
}

Kind Prims::of(uint32_t vertices) {
    if (prims_.empty()) return Kind::None;
    if (!sorted_) {
        std::sort(prims_.begin(), prims_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        sorted_ = true;
    }
    // The last primitive starting at or before the address.
    auto it = std::upper_bound(prims_.begin(), prims_.end(), vertices,
                               [](uint32_t v, const auto& p) { return v < p.first; });
    if (it == prims_.begin()) return Kind::None;
    --it;
    return vertices - it->first < kPrimSize ? it->second : Kind::None;
}

void Prims::clear() {
    prims_.clear();
    sorted_ = true;
}

}  // namespace uagfx::effect_exe
