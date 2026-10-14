#include "core/map_objects.hpp"

namespace uagfx::objects {

Kind kindOf(File file, size_t frame) {
    switch (file) {
        case File::Torch: return frame == 0 ? Kind::TorchGlow : Kind::TorchFlame;
        case File::Smoke: return Kind::Smoke;
        case File::None: break;
    }
    return Kind::None;
}

void Textures::note(uint32_t texture, Kind kind) {
    if (kind == Kind::None)
        kinds_.erase(texture);
    else
        kinds_[texture] = kind;
}

Kind Textures::of(uint32_t texture) const {
    const auto it = kinds_.find(texture);
    return it == kinds_.end() ? Kind::None : it->second;
}

}  // namespace uagfx::objects
