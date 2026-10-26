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
    // 3.0.4-beta.20 (the own ground's far water): its level (up along -y), its waves' height, speed (degrees a tick)
    // and pitch (degrees a cell), and its pictures' pace (ticks a picture; 3 before 1.8) - known false before 1.3
    float waterLevel = 0.0f, waveHeight = 0.0f, waveSpeed = 0.0f, wavePitch = 0.0f;
    int waterAnim = 3;
    bool waterKnown = false;
    // 2.1.3: the map's sun as its makers set it - the way its painted shadows fall (degrees; latitude from straight
    // above); known false for a file without it
    int lightLongitude = 0, lightLatitude = 0;
    bool lightKnown = false;
    // 3.0.7-beta.3: its colours (red, green, blue; 1 full) - uaRO.exe lights its ground's vertices with them
    // (core/terrain.hpp Sun); the game's when a file has none
    float lightDiffuse[3] = {1.0f, 1.0f, 1.0f}, lightAmbient[3] = {0.3f, 0.3f, 0.3f};
    std::vector<Light> lights;
    std::vector<Effect> effects;
    std::vector<Model> models;  // 2.0
    std::vector<Sound> sounds;  // 3.0.4-beta.7
};

// false (and why) when it is not a map file; a file cut short or an unknown object keeps what came before it.
bool parse(const uint8_t* data, size_t size, World& out, std::string& error);

// 3.0.7-beta.4: the way the map's light goes, as uaRO.exe turns the file's longitude and latitude (FUN_0043a200:
// about x by the latitude, then about y by the longitude) - (sin lat sin lon, cos lat, sin lat cos lon), +y down, in
// the file's places; the way the maps' painted shadows fall (amatsu, longitude 10: darkest 90 degrees from +x towards
// +z; prt_monk 30: 67.5; job_sword1 315: 135). Impl::mapSun had sin and cos of the longitude the other way round -
// the same at 45 (722 of the client's maps), turned at the rest (79 maps of 315 the other way)
void lightWay(int longitude, int latitude, float way[3]);

// 3.0.1-beta.3 (grass_beds), 3.0.4-beta.14 shared: a point of a model as it stands (core/rsm.hpp's frame) where the map
// puts it - scaled, turned about y, x and z (degrees), moved to its place
void placeInMap(const Model& at, float p[3]);

}  // namespace uagfx::rsw
