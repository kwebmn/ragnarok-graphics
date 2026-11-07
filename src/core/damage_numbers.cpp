#include "core/damage_numbers.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::damagenum {

Kind kindOf(int gameKind) {
    switch (gameKind) {
        case 0x15: return Kind::Hit;
        case 0x16: return Kind::Critical;
        case 0x72: return Kind::Heal;
        default: return Kind::Other;
    }
}

std::string textOf(int value, Kind k) {
    std::string digits = std::to_string(std::max(0, value)), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out += ' ';  // thousands apart
        out += digits[i];
    }
    return k == Kind::Heal ? "+" + out : out;
}

Pose poseAt(uint32_t age, Kind k) {
    Pose p;
    const float t = std::min(1.0f, float(age) / float(kLifeMs));
    p.rise = 0.15f + 0.55f * (1.0f - (1.0f - t) * (1.0f - t));  // up fast, slowing
    p.opacity = age + kFadeMs > kLifeMs ? std::max(0.0f, float(kLifeMs - std::min(age, kLifeMs)) / float(kFadeMs))
                                        : 1.0f;
    if (k == Kind::Critical) {  // a pop: large at first, settling
        const float pop = std::max(0.0f, 1.0f - float(age) / 180.0f);
        p.size = 1.35f + 0.45f * pop;
    }
    return p;
}

uint32_t rgbOf(uint32_t argb, Kind k) {
    if (k == Kind::Critical) return 0xFFB347;
    const uint32_t rgb = argb & 0xFFFFFF;
    if (k == Kind::Hit && rgb == 0xFFFFFF) return 0xFFF4E0;
    return rgb ? rgb : 0xFFFFFF;
}

}  // namespace uagfx::damagenum
