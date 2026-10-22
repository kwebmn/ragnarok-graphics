#include <string>

#include "core/settings/strings.hpp"
#include "core/start_screen.hpp"
#include "test.hpp"

using namespace uagfx;
using start::Progress;
using start::Stage;

TEST(start_screen_says_what_the_start_is_doing) {
    // 0.40: over the game's black window while the renderer starts (2.12: in the player's language).
    CHECK(start::line(Progress{Stage::Renderer, 0, 0}, settings::Lang::En) == "Starting the renderer...");
    CHECK(start::line(Progress{Stage::Dlss, 0, 0}, settings::Lang::En) == "Starting NVIDIA DLSS...");
    CHECK(start::line(Progress{Stage::ShadersLoad, 5, 43}, settings::Lang::En) == "Loading shaders...");
    CHECK(start::line(Progress{Stage::ShadersBuild, 12, 43}, settings::Lang::En) == "Compiling shaders: 12 of 43");
    CHECK(start::line(Progress{Stage::AiTextures, 0, 0}, settings::Lang::En) == "Starting AI textures...");
    // Compiling comes once a version: said so under it; 2.13: the other stages name «Звернення»'s key.
    CHECK(start::hint(Progress{Stage::ShadersBuild, 1, 43}, settings::Lang::En) ==
          "First start of this version - the next ones skip this");
    const std::string key = "Something wrong with the picture? Ctrl+Shift+F10 in the game - a screenshot and a report "
                            "to the developers";
    CHECK(start::hint(Progress{Stage::ShadersLoad, 1, 43}, settings::Lang::En) == key);
    CHECK(start::hint(Progress{Stage::Renderer, 0, 0}, settings::Lang::En) == key);
    CHECK(start::hint(Progress{Stage::AiTextures, 0, 0}, settings::Lang::Uk) ==
          "Щось не так з картинкою? Ctrl+Shift+F10 у грі — знімок і звернення до розробників");
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

// 2.12: the splash in the player's language - the settings app's choice, else Windows' (uk, ru, else en)
TEST(the_splash_speaks_the_players_language) {
    CHECK_EQ(start::line(Progress{Stage::ShadersBuild, 12, 43}, settings::Lang::Uk),
             std::string("Збираємо шейдери: 12 з 43"));
    CHECK_EQ(start::line(Progress{Stage::Renderer, 0, 0}, settings::Lang::Ru), std::string("Запускаем рендер…"));
    CHECK_EQ(start::hint(Progress{Stage::ShadersBuild, 1, 43}, settings::Lang::Uk),
             std::string("Перший запуск цієї версії — наступні його пропустять"));
    const settings::Lang german = settings::windowLang(std::nullopt, 0x07);  // a German Windows
    CHECK_EQ(start::line(Progress{Stage::Renderer, 0, 0}, german), std::string("Starting the renderer..."));
}
