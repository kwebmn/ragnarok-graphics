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

uint8_t ownerSeed(uint32_t actor) {
    const uint8_t s = uint8_t((actor >> 4) * 2654435761u >> 24);  // the address's bits spread
    return s ? s : 1;
}

void Owner::shadow(uint32_t actor) { seed_ = ownerSeed(actor); }

Tag Owner::faceTag(Tag current, uint32_t flags) const {
    if (current.kind != Kind::None) return current;
    Tag t;
    if (flags & 1) t.seed = seed_;  // a blended list's: a sprite's
    return t;
}

void Owner::clear() { seed_ = 0; }

bool softEffect(bool addsLight, uint8_t kind) {
    if (kind == uint8_t(Kind::HitFlash) || kind == uint8_t(Kind::Glint)) return false;
    return addsLight || kind == uint8_t(Kind::Skill) || kind == uint8_t(Kind::Portal);
}

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
    case 334:  // 2.6.18: blindness; the darkness's ten levels
    case 475:
    case 476:
    case 477:
    case 478:
    case 479:
    case 480:
    case 481:
    case 482:
    case 483:
    case 484:
        return Kind::Darkness;
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

Soften softenSkill(bool blend, uint32_t srcBlend, uint32_t dstBlend) {
    if (blend && srcBlend == d3d7::BLEND_SRCALPHA && dstBlend == d3d7::BLEND_INVSRCALPHA) return Soften::None;
    return softenFor(blend, srcBlend, dstBlend);
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


bool softAdd(bool skillsSoft, bool addsLight, uint8_t kind, uint32_t srcBlend) {
    return skillsSoft && addsLight && kind == uint8_t(Kind::Skill) &&
           (srcBlend == d3d7::BLEND_ONE || srcBlend == d3d7::BLEND_SRCALPHA);
}

bool isCasting(int id) { return id == 12 || (id >= 54 && id <= 59); }

int castPercent(int skillPercent) { return int(float(std::clamp(skillPercent, 0, 100)) * 0.4f + 0.5f); }

size_t Faces::slotOf(uint32_t vertices) const {
    const size_t mask = slots_.size() - 1;
    size_t i = size_t((vertices * 0x9E3779B1u) >> 8) & mask;  // the address's bits spread (Fibonacci hashing)
    while (slots_[i].stamp == stamp_ && slots_[i].vertices != vertices) i = (i + 1) & mask;
    return i;
}

void Faces::grow() {
    std::vector<Slot> old;
    old.swap(slots_);
    slots_.assign(std::max<size_t>(old.size() * 2, 1024), Slot{});
    for (const Slot& s : old)
        if (s.stamp == stamp_) slots_[slotOf(s.vertices)] = s;
}

void Faces::note(uint32_t vertices, Tag tag) {
    if (!vertices || (tag.kind == Kind::None && !tag.seed)) return;  // 2.6.3: a character's sprite has its seed
    if ((count_ + 1) * 2 > slots_.size()) grow();
    Slot& s = slots_[slotOf(vertices)];
    if (s.stamp != stamp_) ++count_;
    s.vertices = vertices;
    s.stamp = stamp_;
    s.tag = tag;
}

Tag Faces::of(uint32_t vertices) const {
    if (!count_) return Tag{};
    const Slot& s = slots_[slotOf(vertices)];
    return s.stamp == stamp_ ? s.tag : Tag{};
}

void Faces::clear() {
    count_ = 0;
    if (++stamp_ == 0) {  // after four billion frames: every slot free by hand once
        for (Slot& s : slots_) s.stamp = 0;
        stamp_ = 1;
    }
}

}  // namespace uagfx::effect_exe
