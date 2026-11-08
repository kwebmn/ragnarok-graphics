#include "core/afterimage.hpp"

namespace uagfx::afterimage {

Tint tintOf(bool fury, bool steelBody) {
    if (steelBody) return Tint{150, 175, 205, 0.22f};
    if (fury) return Tint{255, 96, 48, 0.18f};
    return Tint{};
}

}  // namespace uagfx::afterimage
