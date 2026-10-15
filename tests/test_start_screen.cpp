#include "core/start_screen.hpp"
#include "test.hpp"

using namespace uagfx;
using start::Progress;
using start::Stage;

TEST(start_screen_says_what_the_start_is_doing) {
    // 0.40: over the game's black window while the renderer starts - in English, as the player asked.
    CHECK(start::line(Progress{Stage::Renderer, 0, 0}) == "Starting the renderer...");
    CHECK(start::line(Progress{Stage::Dlss, 0, 0}) == "Starting NVIDIA DLSS...");
    CHECK(start::line(Progress{Stage::ShadersLoad, 5, 43}) == "Loading shaders...");
    CHECK(start::line(Progress{Stage::ShadersBuild, 12, 43}) == "Compiling shaders: 12 of 43");
    CHECK(start::line(Progress{Stage::AiTextures, 0, 0}) == "Starting AI textures...");
    // Compiling comes once a version: said so under it; the other stages have no second line.
    CHECK(start::hint(Progress{Stage::ShadersBuild, 1, 43}) ==
          "First start of this version - the next ones skip this");
    CHECK(start::hint(Progress{Stage::ShadersLoad, 1, 43}).empty());
    CHECK(start::hint(Progress{Stage::Renderer, 0, 0}).empty());
}

TEST(start_screen_bar_only_moves_forward) {
    // The whole start on one bar: the renderer, DLSS, then the shaders one by one (most of it), the AI textures last.
    float last = -1.0f;
    const Progress steps[] = {{Stage::Renderer, 0, 0},       {Stage::Dlss, 0, 0},
                              {Stage::ShadersBuild, 0, 43},  {Stage::ShadersBuild, 1, 43},
                              {Stage::ShadersBuild, 30, 43}, {Stage::ShadersBuild, 43, 43},
                              {Stage::AiTextures, 0, 0}};
    for (const Progress& p : steps) {
        const float f = start::filled(p);
        CHECK(f > last);
        CHECK(f >= 0.0f && f <= 1.0f);
        last = f;
    }
    // Loaded shaders fill it the same way; odd counts stay on the bar.
    CHECK(start::filled(Progress{Stage::ShadersLoad, 43, 43}) == start::filled(Progress{Stage::ShadersBuild, 43, 43}));
    CHECK(start::filled(Progress{Stage::ShadersBuild, 50, 43}) == start::filled(Progress{Stage::ShadersBuild, 43, 43}));
    CHECK(start::filled(Progress{Stage::ShadersBuild, 3, 0}) == start::filled(Progress{Stage::ShadersBuild, 0, 43}));
    CHECK(start::filled(Progress{Stage::ShadersBuild, -2, 43}) == start::filled(Progress{Stage::ShadersBuild, 0, 43}));
}
