#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace uagfx::map_textures {

// 2.2.5: the textures of one map - its ground's (.gnd) and its models' (the .rsm files its .rsw places) - so that the
// texture pack's pictures of that map alone are made again by another model, or taken out, from the Texture pack
// page: a test of a model on one place without making the whole pack anew.

// The textures a ground file names, as it names them ("필드바닥\해변모래바닥01.bmp", the game's cp949 bytes); none when
// it is no ground file (version 1.7 on: "GRGN") or is cut short in its names.
std::vector<std::string> groundTextures(const uint8_t* data, size_t size);

// A map's name as the game's files have it: "cmd_fild02" of " Cmd_Fild02.gnd " or "data/cmd_fild02.rsw"; "" when it is
// not a plain name.
std::string mapName(const std::string& text);

// One of the game's files by its normal name (zip::normalName: lower case, backslashes); false when there is none.
using Read = std::function<bool(const std::string& name, std::vector<uint8_t>& out)>;

// The game's files ("data\texture\...", normal names) of a map's textures - its ground's and (models) its models' -
// each once, sorted; none for a map it has no files of.
std::vector<std::string> filesOf(const std::string& map, bool ground, bool models, const Read& read);

}  // namespace uagfx::map_textures
