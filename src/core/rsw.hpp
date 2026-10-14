#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::rsw {

// 0.25: a map's .rsw file - what the driver needs of it: the point lights its makers placed (baked into the ground's
// lightmap; the game reads and drops them) and its effects (torches, smoke, fireflies; the game shows ids up to 629
// only). Positions as the file has them: from the map's middle, ten units a ground tile, up along -y.
struct Light {
    float pos[3] = {0, 0, 0};
    float color[3] = {1, 1, 1};  // 0-1
    float range = 0;
};

struct Effect {
    float pos[3] = {0, 0, 0};
    int32_t id = 0;
    float delay = 0;
    float param[4] = {0, 0, 0, 0};
};

struct World {
    int version = 0;  // major * 256 + minor
    std::vector<Light> lights;
    std::vector<Effect> effects;
};

// false (and why) when it is not a map file; a file cut short or an unknown object keeps what came before it.
bool parse(const uint8_t* data, size_t size, World& out, std::string& error);

}  // namespace uagfx::rsw
