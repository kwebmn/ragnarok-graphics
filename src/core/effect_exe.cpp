#include "core/effect_exe.hpp"

#include "core/d3d7.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::effect_exe {

bool isSortEntry(const uint8_t bytes[6]) {
    static const uint8_t kEntry[6] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C};
    return std::memcmp(bytes, kEntry, sizeof kEntry) == 0;
}

bool isShadowEntry(const uint8_t bytes[6]) {
    static const uint8_t kEntry[6] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x64};
    return std::memcmp(bytes, kEntry, sizeof kEntry) == 0;
}

bool auraExtra(int id) { return id == 200 || id == 201 || id == 397 || id == 398; }

Kind kindOf(int id) {
    switch (id) {
    case 45:
        return Kind::Firefly;
    case 307:
    case 322:
    case 323:
    case 324:
        return Kind::Ray;
    case 200:
    case 201:
    case 202:
    case 362:
    case 397:
    case 398:
        return Kind::Aura;
    case 35:  // 0.38: readyportal, portal, warpzone; portal2-5, warpzone2 (the warp NPC's), big_portal, 2
    case 36:
    case 61:
    case 316:
    case 317:
    case 321:
    case 341:
    case 389:
    case 402:
    case 561:
    case 562:
        return Kind::Portal;
    // 0.37: the maps' own - torches, smoke, bubbles, pillars of light, the waterfall, the ghost and the bats, falls of
    // blue light - stay as they are.
    case 44:
    case 47:
    case 109:
    case 231:
    case 247:
    case 259:
    case 353:
    case 358:
    case 359:
    case 360:
    case 557:
    case 558:
    case 559:
    case 560:
        return Kind::None;
    default:
        return id > 0 && id <= 629 ? Kind::Skill : Kind::None;  // the game shows ids up to 629
    }
}

Soften softenFor(bool blend, uint32_t srcBlend, uint32_t dstBlend) {
    if (!blend) return Soften::None;
    if (dstBlend == d3d7::BLEND_ONE)
        return srcBlend == d3d7::BLEND_SRCALPHA ? Soften::Alpha
               : srcBlend == d3d7::BLEND_ONE    ? Soften::Colour
                                                : Soften::None;
    if (srcBlend == d3d7::BLEND_SRCALPHA && dstBlend == d3d7::BLEND_INVSRCALPHA) return Soften::Alpha;
    if ((srcBlend == d3d7::BLEND_ZERO && dstBlend == d3d7::BLEND_SRCCOLOR) ||
        (srcBlend == d3d7::BLEND_DESTCOLOR && dstBlend == d3d7::BLEND_ZERO))
        return Soften::White;
    return Soften::None;
}

void soften(uint8_t* vertices, uint32_t count, uint32_t stride, int percent, Soften how) {
    if (!vertices || stride < 20 || percent >= 100 || how == Soften::None) return;
    const float f = float(std::max(percent, 0)) / 100.0f;
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t* at = vertices + size_t(i) * stride + 16;
        uint32_t c = 0;
        std::memcpy(&c, at, 4);
        uint32_t out = how == Soften::Alpha ? c & 0x00FFFFFFu : c & 0xFF000000u;
        if (how == Soften::Alpha) {
            out |= uint32_t(float(c >> 24) * f + 0.5f) << 24;
        } else {
            for (int shift = 0; shift < 24; shift += 8) {
                const float v = float((c >> shift) & 255);
                const float to = how == Soften::White ? 255.0f - (255.0f - v) * f : v * f;
                out |= uint32_t(to + 0.5f) << shift;
            }
        }
        std::memcpy(at, &out, 4);
    }
}

void Faces::note(uint32_t vertices, Tag tag) {
    if (vertices && tag.kind != Kind::None) faces_[vertices] = tag;
}

Tag Faces::of(uint32_t vertices) const {
    if (faces_.empty()) return Tag{};
    const auto it = faces_.find(vertices);
    return it == faces_.end() ? Tag{} : it->second;
}

void Faces::clear() { faces_.clear(); }

}  // namespace uagfx::effect_exe
