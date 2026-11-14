#include "core/threat_arc.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::threatarc {

float Hits::alphaOf(uint32_t id, uint32_t nowMs) const {
    const auto it = last.find(id);
    if (it == last.end()) return 0.0f;
    const uint32_t age = nowMs - it->second;
    return age >= kHoldMs ? 0.0f : 1.0f - float(age) / float(kHoldMs);
}

void Hits::forget(uint32_t nowMs) {
    for (auto it = last.begin(); it != last.end();) it = nowMs - it->second > kHoldMs ? last.erase(it) : std::next(it);
}

float bearingOf(float fromX, float fromZ, float toX, float toZ, float fx, float fz, float rx, float rz) {
    const float dx = toX - fromX, dz = toZ - fromZ;
    return std::atan2(dx * rx + dz * rz, dx * fx + dz * fz);
}

void arc(float w, float h, float bearing, float alpha, std::vector<own_ground::Tl1>& out) {
    if (w <= 0.0f || h <= 0.0f || alpha <= 0.0f) return;
    const float cx = w * 0.5f, cy = h * 0.5f, side = std::min(w, h);
    const float outer = kRadius * side, inner = (kRadius - kThick) * side;
    const uint32_t a = uint32_t(std::lround(std::clamp(alpha, 0.0f, 1.0f) * kAlphaMost * 255.0f));
    const uint32_t argb = a << 24 | (kColour & 0xFFFFFFu);
    const auto at = [&](float angle, float r) {
        own_ground::Tl1 v{};
        v.x = cx + r * std::sin(angle);
        v.y = cy - r * std::cos(angle);
        v.z = 0.0f;
        v.rhw = 1.0f;
        v.diffuse = argb;
        return v;
    };
    for (int i = 0; i < kSegments; ++i) {
        const float a0 = bearing - kSpan + 2.0f * kSpan * float(i) / float(kSegments);
        const float a1 = bearing - kSpan + 2.0f * kSpan * float(i + 1) / float(kSegments);
        const own_ground::Tl1 o0 = at(a0, outer), o1 = at(a1, outer), i0 = at(a0, inner), i1 = at(a1, inner);
        out.insert(out.end(), {o0, o1, i0, i0, o1, i1});
    }
}

}  // namespace uagfx::threatarc
