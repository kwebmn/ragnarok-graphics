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

TEST(schema_defaults_match_the_driver) {
    const Config defaults;
    CHECK(tokenOf(*findSetting("quality.sharpen"), defaults) == "50");
    CHECK(tokenOf(*findSetting("renderer.backend"), defaults) == "dgvoodoo");
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
    // Spec §2.2: the renderer and allow_tearing wait for a restart, the rest of the pages' keys apply at once.
    for (const IniSetting& s : iniSettings()) {
        const std::string k = sectionKey(s);
        CHECK(appliesNow(k) == (k != "renderer.backend" && k != "renderer.bridge" && k != "quality.dlss" &&
                                k != "frames.allow_tearing" && k != "frames.frame_generation"));
    }
}

TEST(schema_pages_follow_the_spec) {
    // Stage 2 spec §6.1 with 0.6.2: Sound, Game, and Chat and login after Frames - the last holds the whisper, login
    // and chat macro rows the Game page has no room for.
    CHECK_EQ(kPageCount, 10);
    CHECK(pageName(Page::Sound) == Text::PageSound);
    CHECK(pageName(Page::Game) == Text::PageGame);
    CHECK(pageName(Page::Chat) == Text::PageChat);
    CHECK(pageName(Page::Smoothing) == Text::PageSmoothing);  // stage 4, after Quality
    CHECK_EQ(int(Page::Smoothing), 3);
    CHECK_EQ(int(Page::Diagnostics), 8);
    CHECK_EQ(int(Page::Service), 9);
}
