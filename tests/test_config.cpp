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
    CHECK(a.monsters && a.players && a.npcs && a.items);  // 0.13-0.15's one key: all four kinds of sprites
    CHECK(a.effects);
    CHECK(a.model == "anime");
    CHECK(a.spriteModel == "general");
    CHECK_EQ(a.denoise, 40);
    CHECK_EQ(a.scale, 2);
    CHECK_EQ(a.budget, 5);
    CHECK_EQ(a.minSize, 32);
    CHECK_EQ(a.maxSize, 1024);
    CHECK(!a.ahead);
    // Words it does not take leave the defaults: the ground and objects, general and anime, x4, 16-512.
    const AiSettings d = parseConfig("[ai]\nmodel = best\nscale = 3\nmin_size = 8\nmax_size = 2048\n").ai;
    CHECK(d == AiSettings());
    CHECK(d.ground && d.objects && !d.sprites() && !d.effects);
    CHECK(d.model == "general" && d.spriteModel == "anime");
    CHECK_EQ(d.scale, 4);
    CHECK_EQ(d.minSize, 16);  // 0.17: shadows and the small things of a map too
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
    a.monsters = a.players = a.npcs = a.items = a.effects = true;
    CHECK(aiPresetOf(a) == AiPreset::All);
    a.denoise = 30;
    CHECK(aiPresetOf(a) == AiPreset::Custom);
    CHECK(aiPresetSettings(AiPreset::All, true) != aiPresetSettings(AiPreset::Recommended, true));
    const AiSettings all = aiPresetSettings(AiPreset::All, true);
    CHECK(all.enabled && all.ground && all.objects && all.monsters && all.players && all.npcs && all.items &&
          all.effects);
    CHECK(!aiPresetSettings(AiPreset::Recommended, false).enabled);
    CHECK(aiKind(all, 2) && !aiKind(AiSettings(), 3) && aiKind(AiSettings(), 0) && !aiKind(all, 5));
    CHECK(aiKind(AiSettings(), 4));  // 0.14: the windows' icons, on in Recommended
}

TEST(config_ai_argument_round_trips) {
    // [ai] as one word on uaro-render64.exe's command line: no spaces, every key back as it was.
    AiSettings a;
    a.enabled = true;
    a.objects = false;
    a.monsters = true;
    a.npcs = true;
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

TEST(config_ai_sprites_by_their_kind) {
    // 0.16: monsters, players, NPCs and items each have a key; 0.13-0.15's "sprites" sets all four only where none
    // of them is in the file.
    const AiSettings a = parseConfig("[ai]\nmonsters = 1\nplayers = 0\nnpcs = 1\n").ai;
    CHECK(a.monsters && !a.players && a.npcs && !a.items && a.sprites());
    const AiSettings b = parseConfig("[ai]\nsprites = 1\nplayers = 0\n").ai;  // one new key: the old one is ignored
    CHECK(!b.monsters && !b.players && !b.npcs && !b.items);
    CHECK(parseConfig("[ai]\nsprites = 0\n").ai.sprites() == false);
    CHECK(!AiSettings().sprites());
}

TEST(config_effects) {
    // 0.18: [effects] - the glow off unless asked for, its strength 0-200 % and size 0-100 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(!d.glow);
    CHECK_EQ(d.glowStrength, 100);
    CHECK_EQ(d.glowSize, 50);
    const EffectSettings e = parseConfig("[effects]\nglow = 1\nglow_strength = 150\nglow_size = 80\n").effects;
    CHECK(e.glow);
    CHECK_EQ(e.glowStrength, 150);
    CHECK_EQ(e.glowSize, 80);
    const EffectSettings bad = parseConfig("[effects]\nglow = maybe\nglow_strength = 201\nglow_size = -1\n").effects;
    CHECK(bad == d);  // what does not parse keeps the default
    CHECK(parseConfig("[quality]\nglow = 1\n").effects == d);  // outside its section
}

TEST(config_effects_water) {
    // 0.19: the modern water, off unless asked for; its waves, reflections and clarity 0-100 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(!d.water);
    CHECK_EQ(d.waterWaves, 50);
    CHECK_EQ(d.waterReflections, 50);
    CHECK_EQ(d.waterClarity, 50);
    const EffectSettings e =
        parseConfig("[effects]\nwater = 1\nwater_waves = 0\nwater_reflections = 100\nwater_clarity = 30\n").effects;
    CHECK(e.water);
    CHECK_EQ(e.waterWaves, 0);
    CHECK_EQ(e.waterReflections, 100);
    CHECK_EQ(e.waterClarity, 30);
    CHECK(parseConfig("[effects]\nwater_waves = 101\nwater_clarity = x\n").effects == d);
}

TEST(config_effects_shadows_and_light) {
    // 0.20: the soft shadows, off unless asked for; the glow's light on the surroundings.
    const EffectSettings d = parseConfig("").effects;
    CHECK(!d.ao);
    CHECK_EQ(d.aoStrength, 60);
    CHECK_EQ(d.aoSize, 50);
    CHECK_EQ(d.glowLight, 50);
    const EffectSettings e = parseConfig("[effects]\nao = 1\nao_strength = 90\nao_size = 20\nglow_light = 0\n").effects;
    CHECK(e.ao);
    CHECK_EQ(e.aoStrength, 90);
    CHECK_EQ(e.aoSize, 20);
    CHECK_EQ(e.glowLight, 0);
    CHECK(parseConfig("[effects]\nao_strength = 101\nglow_light = -5\n").effects == d);
}

TEST(config_effects_colour) {
    // 0.21: the picture's colours, off unless asked for; contrast and brightness 50-150 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(!d.grade);
    CHECK_EQ(d.gradeSaturation, 115);
    CHECK_EQ(d.gradeContrast, 108);
    CHECK_EQ(d.gradeBrightness, 100);
    CHECK_EQ(d.gradeWarmth, 50);
    CHECK_EQ(d.gradeVignette, 25);
    const EffectSettings e = parseConfig("[effects]\ngrade = 1\ngrade_saturation = 0\ngrade_contrast = 150\n"
                                         "grade_brightness = 50\ngrade_warmth = 100\ngrade_vignette = 0\n").effects;
    CHECK(e.grade);
    CHECK_EQ(e.gradeSaturation, 0);
    CHECK_EQ(e.gradeContrast, 150);
    CHECK_EQ(e.gradeBrightness, 50);
    CHECK_EQ(e.gradeWarmth, 100);
    CHECK_EQ(e.gradeVignette, 0);
    CHECK(parseConfig("[effects]\ngrade_contrast = 49\ngrade_brightness = 151\n").effects == d);
}

TEST(config_effects_depth_of_field) {
    // 0.22: off unless asked for; its blur and sharp zone 0-100 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(!d.dof);
    CHECK_EQ(d.dofStrength, 60);
    CHECK_EQ(d.dofRange, 40);
    const EffectSettings e = parseConfig("[effects]\ndof = 1\ndof_strength = 100\ndof_range = 0\n").effects;
    CHECK(e.dof);
    CHECK_EQ(e.dofStrength, 100);
    CHECK_EQ(e.dofRange, 0);
    CHECK(parseConfig("[effects]\ndof_range = 200\n").effects == d);
}

TEST(config_effects_smooth_map_light) {
    // 0.22.6: on unless turned off.
    CHECK(parseConfig("").effects.smoothLight);
    CHECK(!parseConfig("[effects]\nsmooth_light = 0\n").effects.smoothLight);
}

TEST(config_effects_torches) {
    // 0.23: on unless turned off; the glow and the light 0-200 %, the flicker 0-100 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.torch);
    CHECK_EQ(d.torchGlow, 100);
    CHECK_EQ(d.torchLight, 100);
    CHECK_EQ(d.torchFlicker, 50);
    const EffectSettings e =
        parseConfig("[effects]\ntorch = 0\ntorch_glow = 200\ntorch_light = 0\ntorch_flicker = 100\n").effects;
    CHECK(!e.torch);
    CHECK_EQ(e.torchGlow, 200);
    CHECK_EQ(e.torchLight, 0);
    CHECK_EQ(e.torchFlicker, 100);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\ntorch_flicker = 150\ntorch_glow = 300\n").effects == d);
    // 0.23.2: the new flame (else the game's own picture of it) and its brightness, 0-200 %.
    CHECK(d.torchFlame);
    CHECK_EQ(d.torchBrightness, 100);
    const EffectSettings f = parseConfig("[effects]\ntorch_flame = 0\ntorch_brightness = 60\n").effects;
    CHECK(!f.torchFlame);
    CHECK_EQ(f.torchBrightness, 60);
    CHECK(!(f == d));
    CHECK(parseConfig("[effects]\ntorch_brightness = 250\n").effects == d);
}

TEST(config_effects_fire_light) {
    // 0.26: the fire of skills lights what is around it, as a torch - on unless turned off, 0-200 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.fireLight);
    CHECK_EQ(d.fireLightStrength, 100);
    const EffectSettings e = parseConfig("[effects]\nfire_light = 0\nfire_light_strength = 40\n").effects;
    CHECK(!e.fireLight);
    CHECK_EQ(e.fireLightStrength, 40);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nfire_light_strength = 300\n").effects == d);
}

TEST(config_effects_aura) {
    // 0.27: the level 99 aura made softer - on unless turned off, 0-100 % of the game's brightness.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.aura);
    CHECK_EQ(d.auraBrightness, 50);
    const EffectSettings e = parseConfig("[effects]\naura = 0\naura_brightness = 30\n").effects;
    CHECK(!e.aura);
    CHECK_EQ(e.auraBrightness, 30);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\naura_brightness = 150\n").effects == d);
}

TEST(config_effects_fireflies) {
    // 0.28: fireflies drawn anew - glowing, flashing, lighting a little around them - on unless turned off.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.fireflies);
    CHECK_EQ(d.firefliesBrightness, 100);
    const EffectSettings e = parseConfig("[effects]\nfireflies = 0\nfireflies_brightness = 150\n").effects;
    CHECK(!e.fireflies);
    CHECK_EQ(e.firefliesBrightness, 150);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nfireflies_brightness = 250\n").effects == d);
}

TEST(config_effects_map_glows) {
    // 0.29: the maps' hidden lights - glows, pools of light, dust - on unless turned off, 0-200 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.mapGlows);
    CHECK_EQ(d.mapGlowsBrightness, 100);
    const EffectSettings e = parseConfig("[effects]\nmap_glows = 0\nmap_glows_brightness = 70\n").effects;
    CHECK(!e.mapGlows);
    CHECK_EQ(e.mapGlowsBrightness, 70);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nmap_glows_brightness = 201\n").effects == d);
}

TEST(config_effects_rays) {
    // 0.30: the forest's beams of light drawn softer, shimmering, with dust and light where they fall - on, 0-200 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.rays);
    CHECK_EQ(d.raysBrightness, 100);
    const EffectSettings e = parseConfig("[effects]\nrays = 0\nrays_brightness = 140\n").effects;
    CHECK(!e.rays);
    CHECK_EQ(e.raysBrightness, 140);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nrays_brightness = -5\n").effects == d);
}

TEST(config_effects_lava) {
    // 0.31: lava flowing and glowing, lighting what is around it - on unless turned off, 0-200 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.lava);
    CHECK_EQ(d.lavaBrightness, 100);
    const EffectSettings e = parseConfig("[effects]\nlava = 0\nlava_brightness = 160\n").effects;
    CHECK(!e.lava);
    CHECK_EQ(e.lavaBrightness, 160);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nlava_brightness = 900\n").effects == d);
}

TEST(config_effects_actor_shadows) {
    // 0.32: the characters' shadows laid on the ground from their sprites - on unless turned off, 0-100 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.actorShadows);
    CHECK_EQ(d.actorShadowsStrength, 55);
    const EffectSettings e = parseConfig("[effects]\nactor_shadows = 0\nactor_shadows_strength = 80\n").effects;
    CHECK(!e.actorShadows);
    CHECK_EQ(e.actorShadowsStrength, 80);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nactor_shadows_strength = 101\n").effects == d);
}

TEST(config_effects_clouds) {
    // 0.33: clouds' shadows drifting over the fields and towns - on unless turned off, 0-100 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.clouds);
    CHECK_EQ(d.cloudsStrength, 50);
    const EffectSettings e = parseConfig("[effects]\nclouds = 0\nclouds_strength = 70\n").effects;
    CHECK(!e.clouds);
    CHECK_EQ(e.cloudsStrength, 70);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nclouds_strength = 120\n").effects == d);
}

TEST(config_effects_lamps) {
    // 0.25: the lamps' light from the map files, on unless turned off, 0-200 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.lamps);
    CHECK_EQ(d.lampsLight, 60);
    const EffectSettings e = parseConfig("[effects]\nlamps = 0\nlamps_light = 150\n").effects;
    CHECK(!e.lamps);
    CHECK_EQ(e.lampsLight, 150);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nlamps_light = -5\n").effects == d);
}

TEST(config_effects_chimney_smoke) {
    // 0.24: on unless turned off, its density 0-200 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.smoke);
    CHECK_EQ(d.smokeDensity, 100);
    const EffectSettings e = parseConfig("[effects]\nsmoke = 0\nsmoke_density = 40\n").effects;
    CHECK(!e.smoke);
    CHECK_EQ(e.smokeDensity, 40);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nsmoke_density = 201\n").effects == d);
}

