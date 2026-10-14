#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace uagfx::objects {

// 0.23-0.24: things on the maps the game draws as small pictures blown up many times, which the driver draws anew -
// the torches (core/torch.hpp, win/gpu/torches.cpp), the chimneys' smoke. Known by the sprite file the game loads them
// from: its loader gives each 32-bit frame a texture of its own (CSprRes::Load) and the effect draws from it, so the
// renderer learns at the load which texture is which (Renderer::noteObjectTexture).
enum class File : uint8_t { None, Torch, Smoke };
enum class Kind : uint8_t { None, TorchGlow, TorchFlame, Smoke };

// A 32-bit frame of the file: the torch's first its glow, the rest its flame; the smoke's each a puff.
Kind kindOf(File file, size_t frame);

// The textures that are such a frame now.
class Textures {
public:
    void note(uint32_t texture, Kind kind);  // None: no longer one
    void forget(uint32_t texture) { kinds_.erase(texture); }
    bool empty() const { return kinds_.empty(); }
    Kind of(uint32_t texture) const;

private:
    std::unordered_map<uint32_t, Kind> kinds_;
};

}  // namespace uagfx::objects
