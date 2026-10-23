#pragma once
#include <string>
#include <vector>

namespace uagfx::ai {

// 3.0.2-beta.8: the AI textures' compute shaders (win/gpu/ai_shaders.cpp) a start makes - the pack's pictures need
// ai_mip alone (their mip levels); ai_conv, ai_shuffle, ai_alpha and ai_place run the networks.
std::vector<std::string> shadersFor(bool network);

}  // namespace uagfx::ai
