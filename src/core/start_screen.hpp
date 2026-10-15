#pragma once
#include <cstdint>
#include <string>

namespace uagfx::start {

// 0.40: what the driver does while the game's window waits for its first picture - shown over it with a bar
// (win/start_screen.hpp), in English: the renderer's start, DLSS's, the shaders loaded as the last start kept them
// or compiled (the first start of a version), the AI textures getting ready.
enum class Stage : uint8_t { Renderer, Dlss, ShadersLoad, ShadersBuild, AiTextures };

struct Progress {
    Stage stage = Stage::Renderer;
    int done = 0;   // shaders so far
    int total = 0;  // of them
};

std::string line(const Progress& p);  // "Compiling shaders: 12 of 43"
std::string hint(const Progress& p);  // a second, smaller line; "" none
float filled(const Progress& p);      // the bar, 0-1: the whole start, the shaders most of it

}  // namespace uagfx::start
