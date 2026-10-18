#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace uagfx::map_textures {

// 2.2.5: the textures of one map - its ground's (.gnd) and its models' (the .rsm files its .rsw places) - so that the
// texture pack's pictures of that map alone are made again by another model, or taken out, from the Texture pack
// page: a test of a model on one place without making the whole pack anew.

// The textures a ground file names, as it names them ("필드바닥\해변모래바닥01.bmp", the game's cp949 bytes); none when
// it is no ground file (version 1.7 on: "GRGN") or is cut short in its names.
std::vector<std::string> groundTextures(const uint8_t* data, size_t size);

// 2.3, uaRO Studio: how a ground texture lies on a map. A rectangle of the texture (UV, 0..1) a surface takes.
struct UvRect {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
};
struct GroundUse {
    std::vector<UvRect> rects;  // each distinct one its surfaces take, in the order first met
    uint32_t pairs = 0;         // neighbouring cells whose top surfaces both take this texture
    uint32_t joined = 0;        // of those, the ones whose rectangles go on across their edge (one picture spread)
};
// A ground file's textures (normal names: "data\texture\...") and how its surfaces take them - after the names:
// the lightmaps (count, width, height, cells; then count * width * height * 4 bytes), the surfaces (count; 40 bytes
// each: u[4], v[4] floats, i16 texture, u16 lightmap, 4 bytes colour), the cells (28 bytes each: 4 heights, the top,
// front and right surfaces). Empty when it is no ground file or is cut short anywhere.
std::map<std::string, GroundUse> groundUses(const uint8_t* data, size_t size);
// A cell of the map's ground (x, y in the patch) whose top surface takes the texture: its corners' UV - (x, y),
// (x+1, y), (x, y+1), (x+1, y+1).
struct PatchCell {
    int x = 0, y = 0;
    float u[4] = {}, v[4] = {};
};
// The cells taking `texture` (a normal name) in the side x side window of the map where it is laid most; none when
// the map does not lay it.
std::vector<PatchCell> groundPatch(const uint8_t* data, size_t size, const std::string& texture, int side = 8);

// A map's name as the game's files have it: "cmd_fild02" of " Cmd_Fild02.gnd " or "data/cmd_fild02.rsw"; "" when it is
// not a plain name.
std::string mapName(const std::string& text);

// One of the game's files by its normal name (zip::normalName: lower case, backslashes); false when there is none.
using Read = std::function<bool(const std::string& name, std::vector<uint8_t>& out)>;

// The game's files ("data\texture\...", normal names) of a map's textures - its ground's and (models) its models' -
// each once, sorted; none for a map it has no files of.
std::vector<std::string> filesOf(const std::string& map, bool ground, bool models, const Read& read);

}  // namespace uagfx::map_textures
