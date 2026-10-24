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

// 2.0: a model the map places (its textures for the pack: core/map_textures.hpp, uaRO Studio's catalog) - its file
// under data\model\ (cp949 bytes, as the map names it), its place, its turn (degrees about x, y, z) and its scale.
struct Model {
    std::string file;
    float pos[3] = {0, 0, 0};
    float rot[3] = {0, 0, 0};
    float scale[3] = {1, 1, 1};
};

// 3.0.4-beta.7 (the user's idea 6): a sound its makers placed - birds, a brook, waves, the wind: its file under
// data\wav\, its place, its volume (about 1), its reach and how often it sounds (seconds; 4 before 2.0). uaRO.exe
// loads them and never plays them; the driver does (core/ambience.hpp).
struct Sound {
    std::string file;
    float pos[3] = {0, 0, 0};
    float volume = 1.0f, range = 0.0f, cycle = 4.0f;
};

struct World {
    int version = 0;     // major * 256 + minor
    int waterType = -1;  // 0.31: its water's kind (4: lava); -1 not known
    // 2.1.3: the map's sun as its makers set it - the way its painted shadows fall (degrees; latitude from straight
    // above); known false for a file without it
    int lightLongitude = 0, lightLatitude = 0;
    bool lightKnown = false;
    std::vector<Light> lights;
    std::vector<Effect> effects;
    std::vector<Model> models;  // 2.0
    std::vector<Sound> sounds;  // 3.0.4-beta.7
};

// false (and why) when it is not a map file; a file cut short or an unknown object keeps what came before it.
bool parse(const uint8_t* data, size_t size, World& out, std::string& error);

}  // namespace uagfx::rsw
