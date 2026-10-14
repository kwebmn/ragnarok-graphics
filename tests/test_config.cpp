#include <cstring>

#include "core/config.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(config_defaults_without_a_file) {
    const Config c = parseConfig("");
    CHECK(c.backend == Backend::DgVoodoo);
    CHECK(c.mode == DisplayMode::Auto);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.highResolution);
    CHECK(c.uiFilter == UiFilter::Sharp);
    CHECK_EQ(c.renderScale, 100);
    CHECK_EQ(c.sharpen, 50);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(!c.allowTearing);
    CHECK(c.crispText);
    CHECK(c.logLevel == LogLevel::Info);
}

TEST(config_reads_every_key) {
    const Config c = parseConfig(
        "[renderer]\nbackend = uaro\n"
        "[display]\nmode = window\nmonitor = 1\nwindow_width = 1600\nwindow_height = 900\n"
        "high_resolution = 0\nui_filter = smooth\ncrisp_text = 0\n"
        "[quality]\nrender_scale = 150\nsharpen = 80\n"
        "[frames]\nvsync = 0\nfps_limit = 144\nallow_tearing = 1\n"
        "[debug]\nlog = verbose\n");
    CHECK(c.backend == Backend::Uaro);
    CHECK(c.mode == DisplayMode::Window);
    CHECK_EQ(c.monitor, 1);
    CHECK_EQ(c.windowWidth, 1600);
    CHECK_EQ(c.windowHeight, 900);
    CHECK(!c.highResolution);
    CHECK(c.uiFilter == UiFilter::Smooth);
    CHECK(!c.crispText);
    CHECK_EQ(c.renderScale, 150);
    CHECK_EQ(c.sharpen, 80);
    CHECK(!c.vsync);
    CHECK_EQ(c.fpsLimit, 144);
    CHECK(c.allowTearing);
    CHECK(c.logLevel == LogLevel::Debug);
}

TEST(config_invalid_values_keep_defaults) {
    const Config c = parseConfig(
        "[renderer]\nbackend = vulkan\n"
        "[display]\nmode = fullscreen\nmonitor = -1\nwindow_width = 100\nwindow_height = abc\n"
        "high_resolution = maybe\nui_filter = blurry\ncrisp_text = maybe\n"
        "[quality]\nrender_scale = 300\nsharpen = 101\n"
        "[frames]\nvsync = maybe\nfps_limit = -5\n"
        "[debug]\nlog = loud\n");
    CHECK(c.backend == Backend::DgVoodoo);
    CHECK(c.mode == DisplayMode::Auto);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.highResolution);
    CHECK(c.uiFilter == UiFilter::Sharp);
    CHECK_EQ(c.renderScale, 100);
    CHECK_EQ(c.sharpen, 50);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(c.crispText);
    CHECK(c.logLevel == LogLevel::Info);
}

TEST(config_ai_denoise_is_a_percent) {
    // Below Real-ESRGAN's 50: a game texture's grain is its detail (sand at 100 came out softer than the game's own,
    // 0.11.2), and walls and floors look alike at every strength.
    CHECK_EQ(parseConfig("").ai.denoise, 25);
    CHECK_EQ(parseConfig("[ai]\ndenoise = 0\n").ai.denoise, 0);
    CHECK_EQ(parseConfig("[ai]\ndenoise = 100\n").ai.denoise, 100);
    CHECK_EQ(parseConfig("[ai]\ndenoise = 101\n").ai.denoise, 25);
    CHECK_EQ(parseConfig("[ai]\ndenoise = some\n").ai.denoise, 25);
}

TEST(config_ai_section_reads_every_key) {
    const Config c = parseConfig(
        "[ai]\nenabled = 1\nground = 0\nobjects = 0\nsprites = 1\neffects = 1\nmodel = anime\n"
        "sprite_model = general\ndenoise = 40\nscale = 2\nbudget = 5\nmin_size = 32\nmax_size = 1024\nahead = 0\n");
    const AiSettings& a = c.ai;
    CHECK(a.enabled);
    CHECK(!a.ground);
    CHECK(!a.objects);
    CHECK(a.sprites);
    CHECK(a.effects);
    CHECK(a.model == "anime");
    CHECK(a.spriteModel == "general");
    CHECK_EQ(a.denoise, 40);
    CHECK_EQ(a.scale, 2);
    CHECK_EQ(a.budget, 5);
    CHECK_EQ(a.minSize, 32);
    CHECK_EQ(a.maxSize, 1024);
    CHECK(!a.ahead);
    // Words it does not take leave the defaults: the ground and objects, general and anime, x4, 64-512.
    const AiSettings d = parseConfig("[ai]\nmodel = best\nscale = 3\nmin_size = 8\nmax_size = 2048\n").ai;
    CHECK(d == AiSettings());
    CHECK(d.ground && d.objects && !d.sprites && !d.effects);
    CHECK(d.model == "general" && d.spriteModel == "anime");
    CHECK_EQ(d.scale, 4);
    CHECK_EQ(d.minSize, 64);
    CHECK_EQ(d.maxSize, 512);
}

TEST(config_ai_reads_0_11_keys_where_ai_has_none) {
    // A 0.11 file: [quality] ai_textures, ai_budget, ai_denoise - the AI textures stay on after the update.
    const AiSettings old = parseConfig("[quality]\nai_textures = 1\nai_budget = 4\nai_denoise = 50\n").ai;
    CHECK(old.enabled);
    CHECK_EQ(old.budget, 4);
    CHECK_EQ(old.denoise, 50);
    // [ai]'s own key wins wherever it stands; a key [ai] lacks still comes from [quality].
    const AiSettings both =
        parseConfig("[ai]\nenabled = 0\ndenoise = 10\n[quality]\nai_textures = 1\nai_denoise = 50\nai_budget = 3\n")
            .ai;
    CHECK(!both.enabled);
    CHECK_EQ(both.denoise, 10);
    CHECK_EQ(both.budget, 3);
    // An [ai] word it does not take is no key of its own.
    CHECK(parseConfig("[quality]\nai_textures = 1\n[ai]\nenabled = maybe\n").ai.enabled);
}

TEST(config_ai_presets) {
    // Recommended is the defaults (the ground and objects), All adds sprites and effects; anything else is Custom.
    AiSettings a;
    CHECK(aiPresetOf(a) == AiPreset::Recommended);
    a.enabled = true;
    CHECK(aiPresetOf(a) == AiPreset::Recommended);
    a.sprites = a.effects = true;
    CHECK(aiPresetOf(a) == AiPreset::All);
    a.denoise = 30;
    CHECK(aiPresetOf(a) == AiPreset::Custom);
    CHECK(aiPresetSettings(AiPreset::All, true) != aiPresetSettings(AiPreset::Recommended, true));
    const AiSettings all = aiPresetSettings(AiPreset::All, true);
    CHECK(all.enabled && all.ground && all.objects && all.sprites && all.effects);
    CHECK(!aiPresetSettings(AiPreset::Recommended, false).enabled);
    CHECK(aiKind(all, 2) && !aiKind(AiSettings(), 3) && aiKind(AiSettings(), 0) && !aiKind(all, 4));
}

TEST(config_ai_argument_round_trips) {
    // [ai] as one word on uaro-render64.exe's command line: no spaces, every key back as it was.
    AiSettings a;
    a.enabled = true;
    a.objects = false;
    a.sprites = true;
    a.model = "anime";
    a.spriteModel = "general";
    a.denoise = 35;
    a.scale = 2;
    a.budget = 7;
    a.minSize = 32;
    a.maxSize = 1024;
    a.ahead = false;
    const std::string word = aiArgument(a);
    CHECK(word.find(' ') == std::string::npos);
    CHECK(parseAiArgument(word) == a);
    CHECK(parseAiArgument(aiArgument(AiSettings())) == AiSettings());
}

TEST(config_notepad_file_with_bom_crlf_comments_and_capitals) {
    const Config c = parseConfig("\xEF\xBB\xBF; uaRO Graphics\r\n[Renderer]\r\nBackend = UARO ; the new one\r\n");
    CHECK(c.backend == Backend::Uaro);
}

TEST(config_display_modes) {
    CHECK(parseConfig("[display]\nmode = borderless\n").mode == DisplayMode::Borderless);
    CHECK(parseConfig("[display]\nmode = window\nmode = auto\n").mode == DisplayMode::Auto);
}

TEST(config_keys_outside_their_section_are_ignored) {
    const Config c = parseConfig("backend = uaro\n[display]\nbackend = uaro\n");
    CHECK(c.backend == Backend::DgVoodoo);
}

TEST(config_tokens) {
    CHECK(std::strcmp(backendToken(Backend::DgVoodoo), "dgvoodoo") == 0);
    CHECK(std::strcmp(backendToken(Backend::Uaro), "uaro") == 0);
    CHECK(std::strcmp(displayModeToken(DisplayMode::Window), "window") == 0);
    CHECK(std::strcmp(displayModeToken(DisplayMode::Borderless), "borderless") == 0);
    CHECK(std::strcmp(displayModeToken(DisplayMode::Auto), "auto") == 0);
    CHECK(std::strcmp(uiFilterToken(UiFilter::Sharp), "sharp") == 0);
    CHECK(std::strcmp(uiFilterToken(UiFilter::Smooth), "smooth") == 0);
}

// install.bat adds the sharpen key to a 0.3 settings file as a second [quality] section at its end.
TEST(config_a_section_may_come_twice) {
    const Config c = parseConfig("[quality]\nrender_scale = 150\n[frames]\nvsync = 0\n[quality]\nsharpen = 0\n");
    CHECK_EQ(c.renderScale, 150);
    CHECK_EQ(c.sharpen, 0);
    CHECK(!c.vsync);
}
