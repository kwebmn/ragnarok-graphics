#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "core/config.hpp"
#include "core/game_camera.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(config_defaults_without_a_file) {
    const Config c = parseConfig("");
    CHECK(c.mode == DisplayMode::Auto);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.highResolution);
    CHECK(c.uiFilter == UiFilter::Sharp);
    CHECK_EQ(c.renderScale, 100);
    CHECK_EQ(c.sharpen, 50);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 60);  // 0.85
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
    CHECK(c.mode == DisplayMode::Auto);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.highResolution);
    CHECK(c.uiFilter == UiFilter::Sharp);
    CHECK_EQ(c.renderScale, 100);
    CHECK_EQ(c.sharpen, 50);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 60);  // 0.85
    CHECK(c.crispText);
    CHECK(c.logLevel == LogLevel::Info);
}

TEST(config_window_art) {
    // 0.41: how the windows' buttons, frames and labels are enlarged - xBR unless the file says otherwise.
    CHECK(parseConfig("").windowArt == WindowArt::Xbr);
    CHECK(parseConfig("[display]\nwindow_art = ai\n").windowArt == WindowArt::Ai);
    CHECK(parseConfig("[display]\nwindow_art = pixels\n").windowArt == WindowArt::Pixels);
    CHECK(parseConfig("[display]\nwindow_art = xbr\n").windowArt == WindowArt::Xbr);
    CHECK(parseConfig("[display]\nwindow_art = blurry\n").windowArt == WindowArt::Xbr);
    CHECK(std::string(windowArtToken(WindowArt::Pixels)) == "pixels");
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

TEST(config_ai_dyes_on_by_default) {
    // 2.5.23: a sprite frame in a dye from the pack's picture in its own colours
    CHECK(AiSettings().dyes);
    CHECK(!parseConfig("[ai]\ndyes = 0\n").ai.dyes);
    AiSettings off;
    off.dyes = false;
    CHECK(off != AiSettings());
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
    // 1.5: Real-ESRGAN's RRDBNet for drawn pictures.
    CHECK(parseConfig("[ai]\nsprite_model = anime6b\n").ai.spriteModel == "anime6b");
    // 1.5.8: making new pictures is a switch of its own, the pack's pictures another.
    CHECK(parseConfig("[ai]\nenabled = 0\ngenerate = 1\n").ai.on());
    CHECK(!parseConfig("[ai]\nenabled = 0\ngenerate = 0\n").ai.on());
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
    const Config c = parseConfig("\xEF\xBB\xBF; uaRO Graphics\r\n[Renderer]\r\nApi = D3D11 ; the old one\r\n");
    CHECK(c.api == Api::D3d11);
}

TEST(config_display_modes) {
    CHECK(parseConfig("[display]\nmode = borderless\n").mode == DisplayMode::Borderless);
    CHECK(parseConfig("[display]\nmode = window\nmode = auto\n").mode == DisplayMode::Auto);
}

TEST(config_keys_outside_their_section_are_ignored) {
    const Config c = parseConfig("api = d3d11\n[display]\napi = d3d11\n");
    CHECK(c.api == Api::D3d12);
}

TEST(config_tokens) {
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
    // 0.36: the other skills too, each in its colour - on unless turned off.
    CHECK(d.skillColours);
    CHECK(!parseConfig("[effects]\nskill_colours = 0\n").effects.skillColours);
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

TEST(config_effects_heat) {
    // 0.35: heat haze over fire, lava and the desert far off - on unless turned off, 0-200 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.heat);
    CHECK_EQ(d.heatStrength, 100);
    const EffectSettings e = parseConfig("[effects]\nheat = 0\nheat_strength = 150\n").effects;
    CHECK(!e.heat);
    CHECK_EQ(e.heatStrength, 150);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nheat_strength = 300\n").effects == d);
}

TEST(config_effects_skills_soft) {
    // 0.37: the skills a little softer - on unless turned off, 0-100 % of the game's brightness.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.skillsSoft);
    CHECK_EQ(d.skillBrightness, 80);
    const EffectSettings e = parseConfig("[effects]\nskills_soft = 0\nskill_brightness = 50\n").effects;
    CHECK(!e.skillsSoft);
    CHECK_EQ(e.skillBrightness, 50);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nskill_brightness = 101\n").effects == d);
}

TEST(config_effects_portals) {
    // 0.38: the portals glow around and the air turns in them - on unless turned off, 0-200 %.
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.portals);
    CHECK_EQ(d.portalsStrength, 100);
    const EffectSettings e = parseConfig("[effects]\nportals = 0\nportals_strength = 30\n").effects;
    CHECK(!e.portals);
    CHECK_EQ(e.portalsStrength, 30);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nportals_strength = 250\n").effects == d);
}

TEST(config_effects_weather) {
    // 0.39: the driver's own weather on some maps - on unless turned off, 0-200 % (how much of it).
    const EffectSettings d = parseConfig("").effects;
    CHECK(d.weather);
    CHECK_EQ(d.weatherAmount, 100);
    const EffectSettings e = parseConfig("[effects]\nweather = 0\nweather_amount = 40\n").effects;
    CHECK(!e.weather);
    CHECK_EQ(e.weatherAmount, 40);
    CHECK(!(e == d));
    CHECK(parseConfig("[effects]\nweather_amount = 999\n").effects == d);
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

TEST(config_reads_the_graphics_api) {
    // 0.46: [renderer] api - d3d12 (the default), d3d11 or auto; anything else keeps the default.
    CHECK(parseConfig("").api == Api::D3d12);
    CHECK(parseConfig("[renderer]\napi = d3d11\n").api == Api::D3d11);
    CHECK(parseConfig("[renderer]\napi = AUTO\n").api == Api::Auto);
    CHECK(parseConfig("[renderer]\napi = d3d12\n").api == Api::D3d12);
    CHECK(parseConfig("[renderer]\napi = vulkan\n").api == Api::D3d12);
    CHECK(parseConfig("api = d3d11\n").api == Api::D3d12);  // outside [renderer]
    CHECK(std::strcmp(apiToken(Api::D3d12), "d3d12") == 0);
    CHECK(std::strcmp(apiToken(Api::D3d11), "d3d11") == 0);
    CHECK(std::strcmp(apiToken(Api::Auto), "auto") == 0);
}

TEST(config_effects_drawn_by_the_renderer) {
    // 0.46.1: Direct3D 11 draws none of the Effects page's own - only the aura's and the skills' softness (the
    // game's draws made softer on their way) stay; the log says so when one of the others is on.
    CHECK(drawnByRenderer(parseConfig("").effects));
    CHECK(!drawnByRenderer(effectsOff()));
    EffectSettings soft = effectsOff();
    soft.aura = soft.skillsSoft = true;
    CHECK(!drawnByRenderer(soft));
    EffectSettings water = effectsOff();
    water.water = true;
    CHECK(drawnByRenderer(water));
    EffectSettings glow = effectsOff();
    glow.glow = true;
    CHECK(drawnByRenderer(glow));
}

TEST(config_reads_xray_off_by_default) {
    // 2.3, uaRO Studio's X-ray ([debug] xray, Ctrl+Shift+F9): off unless asked
    CHECK(!parseConfig("").xray);
    CHECK(parseConfig("[debug]\nxray = 1\n").xray);
    CHECK(!parseConfig("[debug]\nxray = 0\n").xray);
}

TEST(config_render_thread) {
    // 2.5.12: [renderer] render_thread - Direct3D 11 on a thread of its own; off unless asked for (a test)
    CHECK(!parseConfig("").renderThread);
    CHECK(parseConfig("[renderer]\nrender_thread = 1\n").renderThread);
    CHECK(!parseConfig("[renderer]\nrender_thread = 0\n").renderThread);
    const settings::IniSetting* s = settings::findSetting("renderer.render_thread");
    CHECK(s != nullptr);
    if (s) CHECK(settings::tokenOf(*s, parseConfig("[renderer]\nrender_thread = 1\n")) == "1");
}

TEST(config_world_scale) {
    // 2.5.16: Direct3D 11 draws the world at 50-100 % of the picture, the interface sharp
    CHECK_EQ(parseConfig("").worldScale, 100);
    CHECK_EQ(parseConfig("[quality]\nworld_scale = 75\n").worldScale, 75);
    CHECK_EQ(parseConfig("[quality]\nworld_scale = 20\n").worldScale, 100);  // out of range: the default
    CHECK_EQ(parseConfig("[quality]\nworld_scale = 140\n").worldScale, 100);
    const settings::IniSetting* s = settings::findSetting("quality.world_scale");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->control == settings::Control::Slider && s->min == 50 && s->max == 100);
    CHECK(s->page == settings::Page::Quality);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[quality]\nworld_scale = 80\n")), std::string("80"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[quality]\nworld_scale = 75\n"));
    CHECK(u.waiting.empty() && u.applied.size() == 1 && u.applied[0] == "world_scale 100 -> 75");
    CHECK(appliesNow("quality.world_scale"));
}

TEST(config_full_colour) {
    // 2.5.18: the textures in their pictures' own colours - on unless the file says otherwise
    CHECK(parseConfig("").fullColour);
    CHECK(!parseConfig("[quality]\nfull_colour = 0\n").fullColour);
    const settings::IniSetting* s = settings::findSetting("quality.full_colour");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->control == settings::Control::Check && s->page == settings::Page::Quality);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[quality]\nfull_colour = 0\n")), std::string("0"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[quality]\nfull_colour = 0\n"));
    CHECK(u.waiting.empty() && u.applied.size() == 1 && u.applied[0] == "full_colour 1 -> 0");
    CHECK(appliesNow("quality.full_colour"));
}

TEST(config_light_modes) {
    // 2.5.20: super-light and the battery saver - off unless the file says otherwise, applied in a running game
    const Config d = parseConfig("");
    CHECK(!d.superLight && !d.batterySaver);
    CHECK_EQ(d.batteryFps, 30);
    const Config c = parseConfig("[quality]\nsuper_light = 1\n[frames]\nbattery_saver = 1\nbattery_fps = 20\n");
    CHECK(c.superLight && c.batterySaver);
    CHECK_EQ(c.batteryFps, 20);
    CHECK_EQ(parseConfig("[frames]\nbattery_fps = 5\n").batteryFps, 30);  // out of range: the default
    const settings::IniSetting* s = settings::findSetting("quality.super_light");
    const settings::IniSetting* b = settings::findSetting("frames.battery_saver");
    const settings::IniSetting* f = settings::findSetting("frames.battery_fps");
    CHECK(s && b && f);
    if (!s || !b || !f) return;
    CHECK(s->page == settings::Page::Quality && b->page == settings::Page::Frames && f->min == 15 && f->max == 60);
    CHECK_EQ(settings::tokenOf(*f, c), std::string("20"));
    const LiveUpdate u = liveUpdate(d, c);
    CHECK(u.waiting.empty() && u.applied.size() == 3);
    CHECK(appliesNow("quality.super_light") && appliesNow("frames.battery_saver") && appliesNow("frames.battery_fps"));
}

TEST(config_shader_variants) {
    // 2.5.16: [debug] shader_variants - on unless turned off (the draw shaders made for the game's states)
    CHECK(parseConfig("").shaderVariants);
    CHECK(!parseConfig("[debug]\nshader_variants = 0\n").shaderVariants);
}

TEST(config_direct_start_is_off_by_default) {
    // 2.11: a direct start of uaRO.exe opens the launcher «Гра» unless [launcher] direct_start = 1
    CHECK(!parseConfig("").directStart);
    CHECK(parseConfig("[launcher]\ndirect_start = 1\n").directStart);
}

TEST(config_the_pad_is_on_by_default) {
    // 2.9: uaRO Pad inside the driver, mounted unless [pad] enabled = 0
    CHECK(parseConfig("").padEnabled);
    CHECK(!parseConfig("[pad]\nenabled = 0\n").padEnabled);
}

// 2.12: the shipped settings ask for Direct3D 12, then 11, then the game's own look (api = auto)
TEST(the_shipped_settings_ask_for_auto) {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/uaro-graphics.ini", std::ios::binary);
    const std::string ini((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(!ini.empty());
    CHECK(parseConfig(ini).api == Api::Auto);
}

TEST(developer_mode_comes_from_the_apps_mode) {
    // 2.15: uaRO Studio's keys (Ctrl+Shift+F7, F8, F9) for the settings app's Developer mode only
    CHECK(!parseConfig("").developer);
    CHECK(!parseConfig("[utility]\nmode = detailed\n").developer);
    CHECK(parseConfig("[utility]\nmode = developer\n").developer);
    CHECK(parseConfig("[utility]\nmode = Developer\n").developer);
}

// 3.0.1-beta.7: [client] mss - our mss32.dll (Miles over XAudio2), on by default, a switch of «Сервіс»
TEST(config_reads_the_sound_librarys_switch) {
    CHECK(parseConfig("").clientMss);
    CHECK(!parseConfig("[client]\nmss = 0\n").clientMss);
    const settings::IniSetting* s = settings::findSetting("client.mss");
    CHECK(s != nullptr);
    if (s) {
        CHECK(s->page == settings::Page::Service && s->control == settings::Control::Check);
        CHECK_EQ(settings::tokenOf(*s, parseConfig("[client]\nmss = 0\n")), std::string("0"));
        CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("1"));
    }
}

// 3.0.1-beta.8: [client] roext - ROExt's work in the driver (core/roext.hpp); [roext] its settings, the defaults the
// client's own dinput8.ini
TEST(config_reads_roexts_settings) {
    const Config d = parseConfig("");
    CHECK(d.clientRoext && d.roext.windowLock && !d.roext.windowOnTop && !d.roext.mouseButtons && !d.roext.altF4);
    CHECK(!d.roext.minimizeKey && d.roext.freeCpu == 4 && d.roext.codePage == -1);
    CHECK(d.roext.windowWidth == 1600 && d.roext.windowHeight == 863);  // the client's dinput8.ini: a full client
                                                                        // (its ini the template) as with ROExt
    const Config c = parseConfig("[client]\nroext = 0\n[roext]\nwindow_lock = 0\nwindow_on_top = 1\nwindow_x = 10\n"
                                 "window_y = 20\nwindow_width = 1280\nwindow_height = 720\nmouse_buttons = 1\n"
                                 "alt_f4 = 1\nminimize_key = 1\nfree_cpu = 0\ncode_page = 1251\n");
    CHECK(!c.clientRoext && !c.roext.windowLock && c.roext.windowOnTop && c.roext.mouseButtons && c.roext.altF4);
    CHECK(c.roext.minimizeKey && c.roext.freeCpu == 0 && c.roext.codePage == 1251);
    CHECK(c.roext.windowX == 10 && c.roext.windowY == 20 && c.roext.windowWidth == 1280 && c.roext.windowHeight == 720);
    CHECK(parseConfig("[roext]\nfree_cpu = 1000\n").roext.freeCpu == 1000);  // a second at most
    CHECK(parseConfig("[roext]\nfree_cpu = 1001\n").roext.freeCpu == 4);     // past it: the default
    CHECK(parseConfig("[roext]\nfree_cpu = -3\n").roext.freeCpu == 4);
    CHECK(parseConfig("[roext]\ncode_page = 70000\n").roext.codePage == -1);
    for (const char* key : {"client.roext", "roext.window_lock", "roext.window_on_top", "roext.mouse_buttons",
                            "roext.alt_f4", "roext.minimize_key", "roext.free_cpu", "roext.code_page"}) {
        const settings::IniSetting* s = settings::findSetting(key);
        CHECK(s != nullptr);
        if (!s) continue;
        CHECK(s->page == (std::string(key) == "client.roext" ? settings::Page::Service : settings::Page::Game));
        CHECK(settings::tokenOf(*s, d) == settings::tokenOf(*s, parseConfig("[client]\nroext = 1\n")));
    }
    CHECK_EQ(settings::tokenOf(*settings::findSetting("roext.code_page"), c), std::string("1251"));
    CHECK_EQ(settings::tokenOf(*settings::findSetting("roext.free_cpu"), d), std::string("4"));
    CHECK_EQ(settings::tokenOf(*settings::findSetting("client.roext"), c), std::string("0"));
}

// 3.0.2-beta.5: [client] updates - the GRFs in updates\ over the game's archives, on by default, a switch of «Сервіс»
TEST(config_reads_the_updates_switch) {
    CHECK(parseConfig("").clientUpdates);
    CHECK(!parseConfig("[client]\nupdates = 0\n").clientUpdates);
    const settings::IniSetting* s = settings::findSetting("client.updates");
    CHECK(s != nullptr);
    if (s) {
        CHECK(s->page == settings::Page::Service && s->control == settings::Control::Check);
        CHECK_EQ(settings::tokenOf(*s, parseConfig("[client]\nupdates = 0\n")), std::string("0"));
        CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("1"));
    }
}

// 3.0.3-beta.2: several game windows - [windows] title_name (the character's name in the window's title) and
// remember_place (each character's window at its own place), both on by default, rows of «Гра й чат»
TEST(config_reads_the_several_windows_switches) {
    const Config d = parseConfig("");
    CHECK(d.windowTitleName && d.windowRememberPlace);
    const Config off = parseConfig("[windows]\ntitle_name = 0\nremember_place = 0\n");
    CHECK(!off.windowTitleName && !off.windowRememberPlace);
    for (const char* key : {"windows.title_name", "windows.remember_place"}) {
        const settings::IniSetting* s = settings::findSetting(key);
        CHECK(s != nullptr);
        if (!s) continue;
        CHECK(s->page == settings::Page::Game && s->control == settings::Control::Check);
        CHECK_EQ(settings::tokenOf(*s, d), std::string("1"));
        CHECK_EQ(settings::tokenOf(*s, off), std::string("0"));
    }
}

// 3.0.3-beta.3: [windows] others_light (the game windows not played in drawn light, «Кадри») and others_music (their
// music; off by default, «Звук»)
TEST(config_reads_the_other_windows_switches) {
    const Config d = parseConfig("");
    CHECK(d.windowOthersLight && !d.windowOthersMusic);
    const Config c = parseConfig("[windows]\nothers_light = 0\nothers_music = 1\n");
    CHECK(!c.windowOthersLight && c.windowOthersMusic);
    const settings::IniSetting* light = settings::findSetting("windows.others_light");
    const settings::IniSetting* music = settings::findSetting("windows.others_music");
    CHECK(light != nullptr && music != nullptr);
    if (!light || !music) return;
    CHECK(light->page == settings::Page::Frames && music->page == settings::Page::Sound);
    CHECK_EQ(settings::tokenOf(*light, d), std::string("1"));
    CHECK_EQ(settings::tokenOf(*music, d), std::string("0"));
    CHECK_EQ(settings::tokenOf(*music, c), std::string("1"));
    CHECK(appliesNow("windows.others_light") && appliesNow("windows.others_music"));
}

// 3.0.3-beta.6: [windows] switch_keys - all (Ctrl+Tab and Ctrl+Alt+1-8), back (Ctrl+Tab alone) or off; a row of
// «Гра й чат», applied at once
TEST(config_reads_the_window_keys) {
    CHECK(parseConfig("").windowKeys == WindowKeys::All);
    CHECK(parseConfig("[windows]\nswitch_keys = back\n").windowKeys == WindowKeys::Back);
    CHECK(parseConfig("[windows]\nswitch_keys = off\n").windowKeys == WindowKeys::Off);
    CHECK(parseConfig("[windows]\nswitch_keys = sometimes\n").windowKeys == WindowKeys::All);
    const settings::IniSetting* s = settings::findSetting("windows.switch_keys");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Game && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[windows]\nswitch_keys = back\n")), std::string("back"));
    CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("all"));
    CHECK(appliesNow("windows.switch_keys"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[windows]\nswitch_keys = off\n"));
    CHECK(u.next.windowKeys == WindowKeys::Off);
}

// 3.0.3-beta.8: the windows' panel - a section of its own: [panel] show (on), layout (row | column), size (75-150 %),
// opacity (40-100 %, 90): rows of «Гра й чат» under «Панель вікон» and in the game's console; hidden and place the
// driver's own (Ctrl+Alt+0, its cross, a drag), no rows
TEST(config_reads_the_windows_panel) {
    const Config d = parseConfig("");
    CHECK(d.panelShow && !d.panelHidden);
    CHECK(d.panelLayout == PanelLayout::Row);
    CHECK_EQ(d.panelSize, 100);
    CHECK_EQ(d.panelOpacity, 90);
    CHECK_EQ(d.panelPlace, std::string(""));
    const Config c = parseConfig("[panel]\nshow = 0\nlayout = column\nsize = 150\nopacity = 60\nhidden = 1\n"
                                 "place = 1.000 0.500\n");
    CHECK(!c.panelShow && c.panelHidden);
    CHECK(c.panelLayout == PanelLayout::Column);
    CHECK_EQ(c.panelSize, 150);
    CHECK_EQ(c.panelOpacity, 60);
    CHECK_EQ(c.panelPlace, std::string("1.000 0.500"));
    CHECK_EQ(parseConfig("[panel]\nsize = 400\n").panelSize, 150);
    CHECK_EQ(parseConfig("[panel]\nsize = 10\n").panelSize, 75);
    CHECK_EQ(parseConfig("[panel]\nopacity = 5\n").panelOpacity, 40);
    for (const char* key : {"panel.show", "panel.layout", "panel.size", "panel.opacity"}) {
        const settings::IniSetting* s = settings::findSetting(key);
        CHECK(s != nullptr);
        if (s) CHECK(s->page == settings::Page::Game);
        CHECK(appliesNow(key));
    }
    CHECK(settings::findSetting("panel.hidden") == nullptr && settings::findSetting("panel.place") == nullptr);
    const settings::IniSetting* layout = settings::findSetting("panel.layout");
    if (layout) CHECK_EQ(settings::tokenOf(*layout, c), std::string("column"));
    const LiveUpdate u = liveUpdate(d, c);
    CHECK(!u.next.panelShow && u.next.panelHidden && u.next.panelSize == 150 && u.next.panelOpacity == 60);
    CHECK(u.next.panelLayout == PanelLayout::Column && u.next.panelPlace == "1.000 0.500");
}

// 3.0.3-beta.11 (the user: «супер минималистичный режим в котором типа иконки маленькие были этих окон»): [panel]
// layout = icons - small tiles with the windows' numbers; the third choice of «Вигляд» in the settings and the console
TEST(config_reads_the_panels_icons) {
    const Config c = parseConfig("[panel]\nlayout = icons\n");
    CHECK(c.panelLayout == PanelLayout::Icons);
    CHECK_EQ(std::string(panelLayoutToken(PanelLayout::Icons)), std::string("icons"));
    const settings::IniSetting* layout = settings::findSetting("panel.layout");
    CHECK(layout != nullptr);
    if (!layout) return;
    CHECK_EQ(layout->tokens.size(), size_t(3));
    CHECK_EQ(layout->choices.size(), size_t(3));
    CHECK_EQ(settings::tokenOf(*layout, c), std::string("icons"));
    CHECK(liveUpdate(parseConfig(""), c).next.panelLayout == PanelLayout::Icons);
}

// 3.0.3-beta.9 (the user: «звуки идут с паралельного окна»): [windows] others_sounds - the other game windows' sounds,
// off by default (the main window's alone), a row of «Звук» beside their music
TEST(config_reads_the_other_windows_sounds) {
    CHECK(!parseConfig("").windowOthersSounds);
    CHECK(parseConfig("[windows]\nothers_sounds = 1\n").windowOthersSounds);
    const settings::IniSetting* s = settings::findSetting("windows.others_sounds");
    CHECK(s != nullptr);
    if (s) CHECK(s->page == settings::Page::Sound && s->control == settings::Control::Check);
    CHECK(appliesNow("windows.others_sounds"));
    CHECK(liveUpdate(parseConfig(""), parseConfig("[windows]\nothers_sounds = 1\n")).next.windowOthersSounds);
}

// 3.0.4-beta.4 (the user's idea 8): «Грати» starts a set of game windows - [windows] start_count, 1-8
TEST(config_windows_start_count) {
    CHECK_EQ(parseConfig("").windowStartCount, 1);
    CHECK_EQ(parseConfig("[windows]\nstart_count = 3\n").windowStartCount, 3);
    CHECK_EQ(parseConfig("[windows]\nstart_count = 12\n").windowStartCount, 1);  // out of range: the default
    const settings::IniSetting* s = settings::findSetting("windows.start_count");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Game && s->control == settings::Control::Slider && s->min == 1 && s->max == 8);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[windows]\nstart_count = 4\n")), std::string("4"));
}

// 3.0.4-beta.5 (the user's idea 5): [camera] indoor - building maps' camera as free as outdoors
TEST(config_camera_indoor) {
    CHECK(parseConfig("").cameraIndoor);
    CHECK(!parseConfig("[camera]\nindoor = 0\n").cameraIndoor);
    const settings::IniSetting* s = settings::findSetting("camera.indoor");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Game && s->control == settings::Control::Check);
    CHECK(appliesNow("camera.indoor"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[camera]\nindoor = 0\n"));
    CHECK(!u.next.cameraIndoor && u.applied.size() == 1 && u.waiting.empty());
}

// 3.0.4-beta.17 (the user chose «Ограничить отдаление», R-35N5QN): [camera] zoom_limit - the camera out no further
// than the character stays this side of the game's far plane
TEST(config_camera_zoom_limit) {
    CHECK(parseConfig("").cameraZoomLimit);
    CHECK(!parseConfig("[camera]\nzoom_limit = 0\n").cameraZoomLimit);
    const settings::IniSetting* s = settings::findSetting("camera.zoom_limit");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Game && s->control == settings::Control::Check);
    CHECK(appliesNow("camera.zoom_limit"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[camera]\nzoom_limit = 0\n"));
    CHECK(!u.next.cameraZoomLimit && u.applied.size() == 1 && u.waiting.empty());
    // the limit itself this side of the far plane, with room for the character's own height
    CHECK(camera::kZoomLimit + 50.0f < camera::kFar && camera::kZoomLimit > 1000.0f);
}

// 3.0.4-beta.6 (the user's idea 4): [buffs] timers - the time left on the buff icons
TEST(config_buff_timers) {
    CHECK(parseConfig("").buffTimers);
    CHECK(!parseConfig("[buffs]\ntimers = 0\n").buffTimers);
    const settings::IniSetting* s = settings::findSetting("buffs.timers");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Game && s->control == settings::Control::Check);
    CHECK(appliesNow("buffs.timers"));
    CHECK(!liveUpdate(parseConfig(""), parseConfig("[buffs]\ntimers = 0\n")).next.buffTimers);
}

TEST(config_interface_minimap) {
    // 3.0.11-beta.1: the new interface's minimap prototype - off unless asked for, changed in a running game
    CHECK(!parseConfig("").newMinimap);
    CHECK(parseConfig("[interface]\nminimap = 1\n").newMinimap);
    CHECK(!parseConfig("[interface]\nminimap = 0\n").newMinimap);
    const settings::IniSetting* s = settings::findSetting("interface.minimap");
    CHECK(s != nullptr);
    if (s) CHECK_EQ(settings::tokenOf(*s, parseConfig("[interface]\nminimap = 1\n")), std::string("1"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[interface]\nminimap = 1\n"));
    // 3.0.11-beta.37: the buffs' column and the character block come with it; 3.0.11-beta.38: the target's card too;
    // 3.0.11-beta.39: the party's panel; 3.0.11-beta.40: the hotbar; 3.0.11-beta.41: the chat; 3.0.11-beta.42: the
    // windows; 3.0.11-beta.48: the cast's label (3.0.11-beta.49's dimmed world no longer comes with it: beta.55)
    CHECK(u.waiting.empty() && u.applied.size() == 10 && u.applied[0] == "minimap 0 -> 1");
    CHECK(u.applied[1] == "buffs off -> column" && u.applied[2] == "items 0 -> 1" && u.applied[3] == "status 0 -> 1");
    CHECK(u.applied[4] == "cast 0 -> 1" && u.applied[5] == "chat 0 -> 1" && u.applied[6] == "hotbar 0 -> 1");
    CHECK(u.applied[7] == "party 0 -> 1" && u.applied[8] == "target 0 -> 1" && u.applied[9] == "character off -> bars");
    CHECK(appliesNow("interface.minimap"));
}

// 3.0.11-beta.65 (the concept's «Подсветка, куда можно дойти» - an experiment, off): [interface] minimap_reach - the
// minimap dims what the player cannot reach on foot; a box of «Інтерфейс», applied at once
TEST(config_reads_the_reach_on_the_minimap) {
    CHECK(!parseConfig("").minimapReach);
    CHECK(parseConfig("[interface]\nminimap_reach = 1\n").minimapReach);
    CHECK(!parseConfig("[interface]\nminimap = 1\n").minimapReach);  // an experiment: not with the new minimap
    const settings::IniSetting* s = settings::findSetting("interface.minimap_reach");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[interface]\nminimap_reach = 1\n")), std::string("1"));
    CHECK(appliesNow("interface.minimap_reach"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[interface]\nminimap_reach = 1\n"));
    CHECK(u.next.minimapReach);
}

// 3.0.11-beta.33 (the owner: «полоска с маршрутом может лучше бы была выполнена такими типа следами типа шажков»):
// [interface] minimap_way - the way on the ground as footprints (steps), the strip (line) or none; a choice of
// «Інтерфейс», applied at once
TEST(config_reads_the_way_on_the_ground) {
    CHECK(parseConfig("").minimapWay == WayOnGround::Steps);
    CHECK(parseConfig("[interface]\nminimap_way = line\n").minimapWay == WayOnGround::Line);
    CHECK(parseConfig("[interface]\nminimap_way = off\n").minimapWay == WayOnGround::Off);
    CHECK(parseConfig("[interface]\nminimap_way = dots\n").minimapWay == WayOnGround::Steps);
    const settings::IniSetting* s = settings::findSetting("interface.minimap_way");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[interface]\nminimap_way = line\n")), std::string("line"));
    CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("steps"));
    CHECK(appliesNow("interface.minimap_way"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[interface]\nminimap_way = off\n"));
    CHECK(u.next.minimapWay == WayOnGround::Off);
}

// 3.0.11-beta.35 (stage 7 of the new interface, the owner's approved buffs column, the row a choice): [interface] buffs
// - the game's own icons (off), the driver's column, its row; a choice of «Інтерфейс», applied at once
TEST(config_reads_the_buffs_look) {
    CHECK(parseConfig("").buffs == BuffsLook::Off);
    CHECK(parseConfig("[interface]\nbuffs = column\n").buffs == BuffsLook::Column);
    CHECK(parseConfig("[interface]\nbuffs = row\n").buffs == BuffsLook::Row);
    CHECK(parseConfig("[interface]\nbuffs = grid\n").buffs == BuffsLook::Off);
    const settings::IniSetting* s = settings::findSetting("interface.buffs");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[interface]\nbuffs = row\n")), std::string("row"));
    CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("off"));
    CHECK(appliesNow("interface.buffs"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[interface]\nbuffs = column\n"));
    CHECK(u.next.buffs == BuffsLook::Column);
}

// 3.0.11-beta.63 (the concept's question 17: variant 1 «ще N» by default, variant 2 - a second column - a choice):
// [interface] buffs_overflow - what the buffs' column does not hold: under «ще N» or in a second column on its left
// (the row: a second row); a choice of «Інтерфейс», applied at once
TEST(config_reads_the_buffs_overflow) {
    CHECK(parseConfig("").buffsOverflow == BuffsOverflow::More);
    CHECK(parseConfig("[interface]\nbuffs_overflow = column\n").buffsOverflow == BuffsOverflow::Column);
    CHECK(parseConfig("[interface]\nbuffs_overflow = more\n").buffsOverflow == BuffsOverflow::More);
    CHECK(parseConfig("[interface]\nbuffs_overflow = scroll\n").buffsOverflow == BuffsOverflow::More);
    const settings::IniSetting* s = settings::findSetting("interface.buffs_overflow");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[interface]\nbuffs_overflow = column\n")), std::string("column"));
    CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("more"));
    CHECK(appliesNow("interface.buffs_overflow"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[interface]\nbuffs_overflow = column\n"));
    CHECK(u.next.buffsOverflow == BuffsOverflow::Column);
}

// 3.0.11-beta.36 (the owner: «кольца или полоски на выбор. приступай»): [interface] character - the game's own Basic
// Info window (off), the character block with bars, with rings; a choice of «Інтерфейс», applied at once
TEST(config_reads_the_character_block) {
    CHECK(parseConfig("").character == CharacterLook::Off);
    CHECK(parseConfig("[interface]\ncharacter = bars\n").character == CharacterLook::Bars);
    CHECK(parseConfig("[interface]\ncharacter = rings\n").character == CharacterLook::Rings);
    CHECK(parseConfig("[interface]\ncharacter = boxes\n").character == CharacterLook::Off);
    const settings::IniSetting* s = settings::findSetting("interface.character");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[interface]\ncharacter = rings\n")), std::string("rings"));
    CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("off"));
    CHECK(appliesNow("interface.character"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[interface]\ncharacter = bars\n"));
    CHECK(u.next.character == CharacterLook::Bars);
}

// 3.0.11-beta.37 (R-TWU5W8: «не появились ни касты ни новый блок персонажа» - both new parts off, and the new minimap
// over the game's buff icons hid them): with the new minimap on, the buffs' column and the character block come with it
// unless the file says otherwise
TEST(config_new_interface_parts_come_with_the_minimap) {
    const Config on = parseConfig("[interface]\nminimap = 1\n");
    CHECK(on.buffs == BuffsLook::Column && on.character == CharacterLook::Bars);
    const Config chosen = parseConfig("[interface]\nminimap = 1\nbuffs = off\ncharacter = rings\n");
    CHECK(chosen.buffs == BuffsLook::Off && chosen.character == CharacterLook::Rings);
    const Config off = parseConfig("[interface]\nminimap = 0\n");
    CHECK(off.buffs == BuffsLook::Off && off.character == CharacterLook::Off);
    const settings::IniSetting* s = settings::findSetting("interface.buffs");
    CHECK(s != nullptr && settings::tokenOf(*s, on) == "column");  // the app shows what is in effect
    const LiveUpdate u = liveUpdate(parseConfig(""), on);  // the minimap switched on: they come at once
    CHECK(u.next.buffs == BuffsLook::Column && u.next.character == CharacterLook::Bars);
}

// 3.0.11-beta.38 (the owner: «делай карточку цели»): [interface] target - the target's card at the top middle; it
// comes with the new minimap as the other parts do, unless the file names it
TEST(config_reads_the_target_card) {
    CHECK(!parseConfig("").targetCard);
    CHECK(parseConfig("[interface]\ntarget = 1\n").targetCard);
    CHECK(parseConfig("[interface]\nminimap = 1\n").targetCard);
    CHECK(!parseConfig("[interface]\nminimap = 1\ntarget = 0\n").targetCard);
    const settings::IniSetting* s = settings::findSetting("interface.target");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[interface]\nminimap = 1\n")), std::string("1"));
    CHECK(appliesNow("interface.target"));
    CHECK(liveUpdate(parseConfig(""), parseConfig("[interface]\ntarget = 1\n")).next.targetCard);
}

// 3.0.11-beta.39 (the owner: «делай» - the party): [interface] party - the party's panel at the top left; it comes with
// the new minimap as the other parts do, unless the file names it
TEST(config_reads_the_party_panel) {
    CHECK(!parseConfig("").partyPanel);
    CHECK(parseConfig("[interface]\nparty = 1\n").partyPanel);
    CHECK(parseConfig("[interface]\nminimap = 1\n").partyPanel);
    CHECK(!parseConfig("[interface]\nminimap = 1\nparty = 0\n").partyPanel);
    const settings::IniSetting* s = settings::findSetting("interface.party");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.party"));
    CHECK(liveUpdate(parseConfig(""), parseConfig("[interface]\nparty = 1\n")).next.partyPanel);
}

// 3.0.11-beta.40 (the owner: «делай дальше» - stage 8, the hotbar first): [interface] hotbar - the hotkeys' bar
// redrawn at the bottom middle; it comes with the new minimap as the other parts do, unless the file names it
TEST(config_reads_the_hotbar) {
    CHECK(!parseConfig("").hotbar);
    CHECK(parseConfig("[interface]\nhotbar = 1\n").hotbar);
    CHECK(parseConfig("[interface]\nminimap = 1\n").hotbar);
    CHECK(!parseConfig("[interface]\nminimap = 1\nhotbar = 0\n").hotbar);
    const settings::IniSetting* s = settings::findSetting("interface.hotbar");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.hotbar"));
    CHECK(liveUpdate(parseConfig(""), parseConfig("[interface]\nhotbar = 1\n")).next.hotbar);
}

// 3.0.11-beta.41 (the owner: «делай чат»): [interface] chat - the chat redrawn at the bottom left; it comes with the
// new minimap as the other parts do, unless the file names it
TEST(config_reads_the_chat) {
    CHECK(!parseConfig("").chat);
    CHECK(parseConfig("[interface]\nchat = 1\n").chat);
    CHECK(parseConfig("[interface]\nminimap = 1\n").chat);
    CHECK(!parseConfig("[interface]\nminimap = 1\nchat = 0\n").chat);
    const settings::IniSetting* s = settings::findSetting("interface.chat");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.chat"));
    CHECK(liveUpdate(parseConfig(""), parseConfig("[interface]\nchat = 1\n")).next.chat);
}

// 3.0.11-beta.42 (the owner: «делай окна»): [interface] items and status - «Речі» and «Характеристики» redrawn; they
// come with the new minimap as the other parts do, unless the file names them
TEST(config_reads_the_windows) {
    CHECK(!parseConfig("").itemsWindow && !parseConfig("").statusWindow);
    CHECK(parseConfig("[interface]\nitems = 1\n").itemsWindow && parseConfig("[interface]\nstatus = 1\n").statusWindow);
    const Config c = parseConfig("[interface]\nminimap = 1\nstatus = 0\n");
    CHECK(c.itemsWindow && !c.statusWindow);
    for (const char* key : {"interface.items", "interface.status"}) {
        const settings::IniSetting* s = settings::findSetting(key);
        CHECK(s != nullptr);
        if (!s) continue;
        CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
        CHECK(appliesNow(key));
    }
    CHECK(liveUpdate(parseConfig(""), parseConfig("[interface]\nitems = 1\n")).next.itemsWindow);
}

// 3.0.11-beta.46 (the concept's motion): [interface] animations - on unless the file says 0, at once
TEST(config_reads_the_animations) {
    CHECK(parseConfig("").animations && parseConfig("[interface]\nanimations = 1\n").animations);
    CHECK(!parseConfig("[interface]\nanimations = 0\n").animations);
    const settings::IniSetting* s = settings::findSetting("interface.animations");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.animations"));
    CHECK(!liveUpdate(parseConfig(""), parseConfig("[interface]\nanimations = 0\n")).next.animations);
}

// 3.0.11-beta.47 (the concept's «Непрозрачность»): [interface] panel_opacity 50-100 (80), window_opacity 80-100 (90)
TEST(config_reads_the_opacities) {
    const Config d = parseConfig("");
    CHECK(d.hudOpacity == 80 && d.windowOpacity == 90);
    const Config c = parseConfig("[interface]\npanel_opacity = 65\nwindow_opacity = 95\n");
    CHECK(c.hudOpacity == 65 && c.windowOpacity == 95);
    const Config low = parseConfig("[interface]\npanel_opacity = 10\nwindow_opacity = 40\n");
    CHECK(low.hudOpacity == 50 && low.windowOpacity == 80);
    for (const char* key : {"interface.panel_opacity", "interface.window_opacity"}) {
        const settings::IniSetting* s = settings::findSetting(key);
        CHECK(s != nullptr);
        if (!s) continue;
        CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Slider);
        CHECK(appliesNow(key));
    }
    CHECK(liveUpdate(d, c).next.hudOpacity == 65 && liveUpdate(d, c).next.windowOpacity == 95);
}

// 3.0.11-beta.48 (the concept's cast label): [interface] cast - on with the new minimap unless the file names it
TEST(config_reads_the_cast_label) {
    CHECK(!parseConfig("").castLabel && parseConfig("[interface]\ncast = 1\n").castLabel);
    CHECK(parseConfig("[interface]\nminimap = 1\n").castLabel);
    CHECK(!parseConfig("[interface]\nminimap = 1\ncast = 0\n").castLabel);
    const settings::IniSetting* s = settings::findSetting("interface.cast");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.cast"));
    CHECK(liveUpdate(parseConfig(""), parseConfig("[interface]\ncast = 1\n")).next.castLabel);
}

// 3.0.11-beta.49 (the concept's open windows): [interface] dim_world; 3.0.11-beta.55 (the owner: «надо убрать
// затемнение интерфейса... мешает») off unless the file asks for it, the new minimap or not
TEST(config_reads_the_world_dim) {
    CHECK(!parseConfig("").dimWorld && parseConfig("[interface]\ndim_world = 1\n").dimWorld);
    CHECK(!parseConfig("[interface]\nminimap = 1\n").dimWorld);
    CHECK(parseConfig("[interface]\nminimap = 1\ndim_world = 1\n").dimWorld);
    const settings::IniSetting* s = settings::findSetting("interface.dim_world");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.dim_world"));
}

// 3.0.11-beta.50 (the concept's low HP): [interface] low_hp_edge 0-100 - none unless asked for
TEST(config_reads_the_low_hp_edge) {
    CHECK(parseConfig("").lowHpEdge == 0 && parseConfig("[interface]\nminimap = 1\n").lowHpEdge == 0);
    CHECK(parseConfig("[interface]\nlow_hp_edge = 60\n").lowHpEdge == 60);
    CHECK(parseConfig("[interface]\nlow_hp_edge = 250\n").lowHpEdge == 100);
    const settings::IniSetting* s = settings::findSetting("interface.low_hp_edge");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Slider);
    CHECK(appliesNow("interface.low_hp_edge"));
}

// 3.0.11-beta.54 (R-JK5455): [interface] size - auto unless a per cent is named
TEST(config_reads_the_interface_size) {
    CHECK(parseConfig("").uiSize == 0 && parseConfig("[interface]\nsize = auto\n").uiSize == 0);
    CHECK(parseConfig("[interface]\nsize = 85\n").uiSize == 85);
    CHECK(parseConfig("[interface]\nsize = 400\n").uiSize == 150);
    const settings::IniSetting* s = settings::findSetting("interface.size");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[interface]\nsize = 115\n")), std::string("115"));
    CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("auto"));
    CHECK(appliesNow("interface.size"));
}
