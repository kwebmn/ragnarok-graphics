#include <fstream>
#include <sstream>

#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

TEST(schema_every_word_reads_back_through_the_driver) {
    // Each word a page can write is what parseConfig reads, and tokenOf gives the same word back.
    for (const IniSetting& s : iniSettings()) {
        std::vector<std::string> words;
        if (s.control == Control::Slider || s.control == Control::Number)
            words = {std::to_string(s.min), std::to_string(s.max), std::to_string((s.min + s.max) / 2)};
        else
            for (const char* t : s.tokens) words.push_back(t);
        for (const std::string& w : words) {
            CHECK(validToken(s, w));
            const Config c = parseConfig(std::string("[") + s.section + "]\n" + s.key + " = " + w + "\n");
            CHECK(tokenOf(s, c) == w);
        }
        CHECK(!validToken(s, "nonsense"));
        CHECK(findSetting(sectionKey(s)) == &s);
    }
    CHECK(!validToken(*findSetting("quality.render_scale"), "99"));
    CHECK(!validToken(*findSetting("frames.fps_limit"), "1001"));
}

TEST(schema_compare_key_turns_every_effect_off) {
    // 0.39.4: Pause shows the game's own picture - every switch of the Effects page off, the ones added last too
    // (0.28-0.39 stayed on: clouds, haze, the characters' shadows, weather...).
    Config c;
    c.effects = effectsOff();
    int switches = 0;
    for (const IniSetting& s : iniSettings()) {
        if (std::string(s.section) != "effects" || s.control != Control::Check) continue;
        ++switches;
        if (tokenOf(s, c) != s.tokens[0]) std::printf("  still on with Pause: effects.%s\n", s.key);
        CHECK(tokenOf(s, c) == s.tokens[0]);
    }
    CHECK(switches >= 20);  // the page's switches were found
}

TEST(schema_defaults_match_the_driver) {
    const Config defaults;
    CHECK(tokenOf(*findSetting("quality.sharpen"), defaults) == "50");
    CHECK(tokenOf(*findSetting("renderer.backend"), defaults) == "dgvoodoo");
    // 0.46: the graphics API on the Render page - Direct3D 12 by default; 11 and auto for older graphics cards.
    const IniSetting* api = findSetting("renderer.api");
    CHECK(api && api->page == Page::Render && api->control == Control::Choice);
    CHECK(api && api->tokens.size() == 3 && std::string(api->tokens[0]) == "d3d12" &&
          std::string(api->tokens[1]) == "d3d11" && std::string(api->tokens[2]) == "auto");
    CHECK(api && tokenOf(*api, defaults) == "d3d12");
    Config eleven;
    eleven.api = Api::D3d11;
    CHECK(api && tokenOf(*api, eleven) == "d3d11");
    CHECK(tokenOf(*findSetting("debug.log"), defaults) == "normal");
}

TEST(schema_covers_every_key_of_the_shipped_ini) {
    // A new driver key must reach a page: every key of dist/uaro-graphics.ini is here or written by the Screen page.
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/uaro-graphics.ini", std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    const auto keys = readIni(ss.str());
    CHECK(keys.size() >= 14);
    for (const auto& kv : keys) {
        const bool screen = kv.first == "display.mode" || kv.first == "display.monitor" ||
                            kv.first == "display.window_width" || kv.first == "display.window_height";
        CHECK(screen || findSetting(kv.first) != nullptr);
    }
}

TEST(schema_live_keys_are_the_specs) {
    // Spec §2.2: the renderer and allow_tearing wait for a restart, the rest of the pages' keys apply at once (the
    // 64-bit renderer's features too: DLSS, frame generation, AI textures); [effects] at once (0.18).
    for (const IniSetting& s : iniSettings()) {
        const std::string k = sectionKey(s);
        CHECK(appliesNow(k) == (k != "renderer.backend" && k != "renderer.bridge" && k != "renderer.remix" &&
                                k != "renderer.api" &&
                                k != "quality.dlss" &&
                                k != "frames.allow_tearing" && k != "frames.frame_generation" &&
                                k.compare(0, 3, "ai.") != 0));
    }
}

TEST(schema_pages_follow_the_spec) {
    // Stage 2 spec §6.1 with 0.6.2: Sound, Game, and Chat and login after Frames - the last holds the whisper, login
    // and chat macro rows the Game page has no room for.
    CHECK_EQ(kPageCount, 14);
    CHECK(pageName(Page::Sound) == Text::PageSound);
    CHECK(pageName(Page::Game) == Text::PageGame);
    CHECK(pageName(Page::Chat) == Text::PageChat);
    CHECK(pageName(Page::Smoothing) == Text::PageSmoothing);  // stage 4, after Quality
    CHECK_EQ(int(Page::Smoothing), 3);
    // 0.18: Effects after Smoothing; 0.12: the AI textures' settings and what upscaled.grf holds after them.
    CHECK(pageName(Page::Effects) == Text::PageEffects);
    CHECK_EQ(int(Page::Effects), 4);
    CHECK(pageName(Page::Ai) == Text::PageAi);
    CHECK(pageName(Page::AiCache) == Text::PageAiCache);
    CHECK_EQ(int(Page::Ai), 5);
    CHECK_EQ(int(Page::AiCache), 6);
    // 0.43: the texture pack after them.
    CHECK(pageName(Page::Pack) == Text::PagePack);
    CHECK_EQ(int(Page::Pack), 7);
    CHECK_EQ(int(Page::Diagnostics), 12);
    CHECK_EQ(int(Page::Service), 13);
}
