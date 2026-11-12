#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "core/config.hpp"
#include "core/game_camera.hpp"
#include "core/live_config.hpp"
#include "core/settings/effects_layout.hpp"
#include "core/settings/navigation.hpp"
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

// 3.0.11-beta.184 (R-ANMTEJ): [camera] remember - the camera's zoom and tilt kept between the games
TEST(config_camera_remember) {
    CHECK(parseConfig("").cameraRemember);
    CHECK(!parseConfig("[camera]\nremember = 0\n").cameraRemember);
    const settings::IniSetting* s = settings::findSetting("camera.remember");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Game && s->control == settings::Control::Check);
    CHECK(appliesNow("camera.remember"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[camera]\nremember = 0\n"));
    CHECK(!u.next.cameraRemember && u.applied.size() == 1 && u.waiting.empty());
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[camera]\nremember = 0\n")), std::string("0"));
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

// 3.0.11 (the owner: the new interface for all): the interface's tests of a part off until «Новий інтерфейс» turns it
// on start from a file that turns it off
const std::string kNoUi = "[interface]\nminimap = 0\n";
Config noUi(const std::string& more) { return parseConfig(kNoUi + more); }  // more: [interface]'s lines, or sections

TEST(config_interface_minimap) {
    // 3.0.11-beta.1: the new interface's minimap prototype - off unless asked for, changed in a running game;
    // 3.0.11 (the owner: «Включить всем»): on unless the file turns it off
    CHECK(parseConfig("").newMinimap && parseConfig("").referenceWindow && parseConfig("").targetCard);
    CHECK(noUi("minimap = 1\n").newMinimap);
    CHECK(!noUi("minimap = 0\n").newMinimap);
    const settings::IniSetting* s = settings::findSetting("interface.minimap");
    CHECK(s != nullptr);
    if (s) CHECK_EQ(settings::tokenOf(*s, noUi("minimap = 1\n")), std::string("1"));
    const LiveUpdate u = liveUpdate(noUi("minimap = 0\n"), noUi("minimap = 1\n"));
    // 3.0.11-beta.37: the buffs' column and the character block come with it; 3.0.11-beta.38: the target's card too;
    // 3.0.11-beta.39: the party's panel; 3.0.11-beta.40: the hotbar; 3.0.11-beta.41: the chat; 3.0.11-beta.42: the
    // windows; 3.0.11-beta.48: the cast's label (3.0.11-beta.49's dimmed world no longer comes with it: beta.55);
    // 3.0.11-beta.70: the tracker; 3.0.11-beta.96: «Навички»; 3.0.11-beta.97: «Спорядження»; .98: the NPC talk;
    // .100: the shops; .115: the storage; .116: the trade; .140: the menu; .149: the item and skill cards; .150: the
    // requests and the NPC's inputs; .151: the game's message boxes and its login; .204: the tip of the day; .205:
    // the emotion list; .206: the command list; .207: the chat rooms; .209: Alt+D's console; .211: the popups;
    // .212: the whisper windows; 3.0.18-beta.103: opening one's shop (as my_shop); 3.0.18-beta.180: the map loading bar
    CHECK(u.waiting.empty() && u.applied.size() == 48 && u.applied[0] == "minimap 0 -> 1");
    CHECK(u.applied[1] == "buffs off -> column" && u.applied[2] == "items 0 -> 1" && u.applied[3] == "status 0 -> 1");
    CHECK(u.applied[4] == "skills 0 -> 1" && u.applied[5] == "equip 0 -> 1" && u.applied[6] == "npc_talk 0 -> 1");
    CHECK(u.applied[7] == "shop 0 -> 1" && u.applied[8] == "storage 0 -> 1" && u.applied[9] == "trade 0 -> 1");
    CHECK(u.applied[10] == "menu 0 -> 1" && u.applied[11] == "cards 0 -> 1" && u.applied[12] == "asks 0 -> 1");
    CHECK(u.applied[13] == "messages 0 -> 1" && u.applied[14] == "login 0 -> 1" &&
          u.applied[15] == "char_select 0 -> 1" && u.applied[16] == "char_make 0 -> 1" &&
          u.applied[17] == "cart 0 -> 1" && u.applied[18] == "counts 0 -> 1" && u.applied[19] == "vending 0 -> 1");
    CHECK(u.applied[20] == "picks 0 -> 1" && u.applied[21] == "party_setup 0 -> 1" && u.applied[22] == "pet 0 -> 1");
    CHECK(u.applied[23] == "homun 0 -> 1" && u.applied[24] == "merc 0 -> 1" && u.applied[25] == "options 0 -> 1");
    CHECK(u.applied[26] == "social 0 -> 1" && u.applied[27] == "servers 0 -> 1" && u.applied[28] == "guild 0 -> 1");
    CHECK(u.applied[29] == "my_shop 0 -> 1" && u.applied[30] == "vend_setup 0 -> 1" &&
          u.applied[31] == "whisper_setup 0 -> 1" &&
          u.applied[32] == "macros 0 -> 1" && u.applied[33] == "tips 0 -> 1" &&
          u.applied[34] == "loading_bar 0 -> 1" && u.applied[35] == "emotes 0 -> 1");
    CHECK(u.applied[36] == "commands 0 -> 1" && u.applied[37] == "chat_rooms 0 -> 1" &&
          u.applied[38] == "detail_console 0 -> 1" && u.applied[39] == "popups 0 -> 1" &&
          u.applied[40] == "whispers 0 -> 1");
    CHECK(u.applied[41] == "cast 0 -> 1" && u.applied[42] == "chat 0 -> 1" && u.applied[43] == "hotbar 0 -> 1");
    CHECK(u.applied[44] == "tracker 0 -> 1" && u.applied[45] == "party 0 -> 1" && u.applied[46] == "target 0 -> 1");
    CHECK(u.applied[47] == "character off -> bars");
    CHECK(appliesNow("interface.minimap"));
}

// 3.0.18-beta.181 (the owner: Storm Gust «как в игре, только чётко»): [effects] crisp_storm_gust - off as new looks
// are, a box of the Effects page (with the mages' looks), by hand only (no effects' set turns it on)
TEST(config_crisp_storm_gust) {
    CHECK(!parseConfig("").effects.crispStormGust);
    CHECK(parseConfig("[effects]\ncrisp_storm_gust = 1\n").effects.crispStormGust);
    const settings::IniSetting* s = settings::findSetting("effects.crisp_storm_gust");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Effects && s->control == settings::Control::Check && s->flag != nullptr);
    bool shown = false;
    const settings::EffectLayout* fx = settings::effectLayout();
    for (int e = 0; e < settings::effectLayoutCount(); ++e)
        shown = shown || std::strcmp(fx[e].box, "effects.crisp_storm_gust") == 0;
    CHECK(shown);
    CHECK(parseConfig("[effects]\npreset = strong\n").effects.crispStormGust == false);
}

// 3.0.18-beta.180: [interface] loading_bar - the game's map loading bar in the new look; on with the new interface,
// a box of «Інтерфейс» → «Вікна в новому вигляді», applied at once
TEST(config_interface_loading_bar) {
    CHECK(parseConfig("").loadingBar);
    CHECK(!noUi("").loadingBar);
    CHECK(noUi("loading_bar = 1\n").loadingBar);
    CHECK(!parseConfig("[interface]\nloading_bar = 0\n").loadingBar);
    const settings::IniSetting* s = settings::findSetting("interface.loading_bar");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("1"));
    CHECK(appliesNow("interface.loading_bar"));
    CHECK(!liveUpdate(parseConfig(""), parseConfig("[interface]\nloading_bar = 0\n")).next.loadingBar);
    bool listed = false;
    for (const settings::InterfaceSection& sec : settings::interfaceSections())
        for (const char* k : sec.keys) listed = listed || std::strcmp(k, "interface.loading_bar") == 0;
    CHECK(listed);
}

// 3.0.11-beta.65 (the concept's «Подсветка, куда можно дойти» - an experiment, off): [interface] minimap_reach - the
// minimap dims what the player cannot reach on foot; a box of «Інтерфейс», applied at once
TEST(config_reads_the_reach_on_the_minimap) {
    CHECK(!noUi("").minimapReach);
    CHECK(noUi("minimap_reach = 1\n").minimapReach);
    CHECK(!noUi("minimap = 1\n").minimapReach);  // an experiment: not with the new minimap
    const settings::IniSetting* s = settings::findSetting("interface.minimap_reach");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK_EQ(settings::tokenOf(*s, noUi("minimap_reach = 1\n")), std::string("1"));
    CHECK(appliesNow("interface.minimap_reach"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("minimap_reach = 1\n"));
    CHECK(u.next.minimapReach);
}

// 3.0.11-beta.72 (the owner: «сами картинки карт маленькие и не аккуратные»): [interface] minimap_hd - the map's
// picture four times bigger when the game has one (the AI pack's, the old client's texture_x4.zip); on by default, a
// box of «Інтерфейс», applied at once
TEST(config_reads_the_sharp_map_pictures) {
    CHECK(noUi("").minimapHd);
    CHECK(!noUi("minimap_hd = 0\n").minimapHd);
    const settings::IniSetting* s = settings::findSetting("interface.minimap_hd");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK_EQ(settings::tokenOf(*s, noUi("minimap_hd = 0\n")), std::string("0"));
    CHECK(appliesNow("interface.minimap_hd"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("minimap_hd = 0\n"));
    CHECK(!u.next.minimapHd);
}

// 3.0.11-beta.74 (the owner: «иконки скилов, предметов, и всего остального также по возможности грузить в текстур
// которые обработаны»): [interface] pack_icons - the panels' icons from the AI texture pack; on, a box of «Інтерфейс»
TEST(config_reads_the_pack_icons) {
    CHECK(noUi("").packIcons);
    CHECK(!noUi("pack_icons = 0\n").packIcons);
    const settings::IniSetting* s = settings::findSetting("interface.pack_icons");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK_EQ(settings::tokenOf(*s, noUi("pack_icons = 0\n")), std::string("0"));
    CHECK(appliesNow("interface.pack_icons"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("pack_icons = 0\n"));
    CHECK(!u.next.packIcons);
}

// 3.0.11-beta.77 (the owner: «чтобы без быстрых кнопок можно было двигать, чтобы сразу все мышкой»): the parts kept
// where they are. 3.0.18-beta.119 (the concept «Ігровий інтерфейс», the owner: «скрытой настройкой, но скорее всего оно
// не понадобится никому»): no row anywhere - [interface] lock_parts alone, written by hand; the old lock_layout read no
// more (nobody left locked with no way back); applied at once
TEST(config_reads_the_layout_lock) {
    CHECK(!noUi("").lockLayout);
    CHECK(!noUi("lock_layout = 1\n").lockLayout);
    CHECK(noUi("lock_parts = 1\n").lockLayout);
    CHECK(settings::findSetting("interface.lock_layout") == nullptr);
    CHECK(settings::findSetting("interface.lock_parts") == nullptr);
    CHECK(liveUpdate(noUi(""), noUi("lock_parts = 1\n")).next.lockLayout);
}

// 3.0.11-beta.78 (the owner: «на панели скилов не те скилы появляются, с других аккаунтов»): [interface]
// hotbar_per_character - each character's own bar (the game keeps one by the selection screen's place); on, a box of
// «Інтерфейс», applied at once
TEST(config_reads_the_bar_per_character) {
    CHECK(noUi("").hotbarPerCharacter);
    CHECK(!noUi("hotbar_per_character = 0\n").hotbarPerCharacter);
    const settings::IniSetting* s = settings::findSetting("interface.hotbar_per_character");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.hotbar_per_character"));
    CHECK(!liveUpdate(noUi(""), noUi("hotbar_per_character = 0\n")).next.hotbarPerCharacter);
}

// 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): [interface] pad_hints - the controller's
// buttons on the interface while played with it; on, a box of «Інтерфейс», at once
TEST(config_pad_hints) {
    CHECK(noUi("").padHints);
    CHECK(!noUi("pad_hints = 0\n").padHints);
    const settings::IniSetting* s = settings::findSetting("interface.pad_hints");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.pad_hints"));
    CHECK(!liveUpdate(noUi(""), noUi("pad_hints = 0\n")).next.padHints);
}

// 3.0.11-beta.87 (stage 3): [interface] target_ring = off | pad | always - the target's ring on the ground; pad
TEST(config_target_ring) {
    CHECK(noUi("").targetRing == TargetRing::Pad);
    CHECK(noUi("target_ring = always\n").targetRing == TargetRing::Always);
    CHECK(noUi("target_ring = off\n").targetRing == TargetRing::Off);
    const settings::IniSetting* s = settings::findSetting("interface.target_ring");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK(appliesNow("interface.target_ring"));
    CHECK(liveUpdate(noUi(""), noUi("target_ring = off\n")).next.targetRing ==
          TargetRing::Off);
}

// 3.0.11-beta.88 (stage 4): [interface] pad_walk = mouse | arrow - the left stick without the mouse; mouse till checked
TEST(config_pad_walk) {
    // 3.0.11-beta.102 (the owner: «проработал передвижение с контролером»): the stick walks by the map's cells and the
    // arrow by default; the cursor held out as before when asked for
    CHECK(noUi("").padWalk == PadWalk::Arrow);
    CHECK(noUi("pad_walk = mouse\n").padWalk == PadWalk::Mouse);
    CHECK(noUi("pad_walk = arrow\n").padWalk == PadWalk::Arrow);
    const settings::IniSetting* s = settings::findSetting("interface.pad_walk");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK(appliesNow("interface.pad_walk"));
    CHECK(liveUpdate(noUi(""), noUi("pad_walk = arrow\n")).next.padWalk == PadWalk::Arrow);
}

// 3.0.11-beta.90 (stage 6): [interface] pad_tiles - Start opens the windows' tile; on
TEST(config_pad_tiles) {
    CHECK(noUi("").padTiles);
    CHECK(!noUi("pad_tiles = 0\n").padTiles);
    const settings::IniSetting* s = settings::findSetting("interface.pad_tiles");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.pad_tiles"));
    CHECK(!liveUpdate(noUi(""), noUi("pad_tiles = 0\n")).next.padTiles);
}

// 3.0.11-beta.33 (the owner: «полоска с маршрутом может лучше бы была выполнена такими типа следами типа шажков»):
// [interface] minimap_way - the way on the ground as footprints (steps), the strip (line) or none; a choice of
// «Інтерфейс», applied at once
TEST(config_reads_the_way_on_the_ground) {
    CHECK(noUi("").minimapWay == WayOnGround::Steps);
    CHECK(noUi("minimap_way = line\n").minimapWay == WayOnGround::Line);
    CHECK(noUi("minimap_way = off\n").minimapWay == WayOnGround::Off);
    CHECK(noUi("minimap_way = dots\n").minimapWay == WayOnGround::Steps);
    const settings::IniSetting* s = settings::findSetting("interface.minimap_way");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, noUi("minimap_way = line\n")), std::string("line"));
    CHECK_EQ(settings::tokenOf(*s, noUi("")), std::string("steps"));
    CHECK(appliesNow("interface.minimap_way"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("minimap_way = off\n"));
    CHECK(u.next.minimapWay == WayOnGround::Off);
}

// 3.0.11-beta.35 (stage 7 of the new interface, the owner's approved buffs column, the row a choice): [interface] buffs
// - the game's own icons (off), the driver's column, its row; a choice of «Інтерфейс», applied at once
TEST(config_reads_the_buffs_look) {
    CHECK(noUi("").buffs == BuffsLook::Off);
    CHECK(noUi("buffs = column\n").buffs == BuffsLook::Column);
    CHECK(noUi("buffs = row\n").buffs == BuffsLook::Row);
    CHECK(noUi("buffs = grid\n").buffs == BuffsLook::Off);
    const settings::IniSetting* s = settings::findSetting("interface.buffs");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, noUi("buffs = row\n")), std::string("row"));
    CHECK_EQ(settings::tokenOf(*s, noUi("")), std::string("off"));
    CHECK(appliesNow("interface.buffs"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("buffs = column\n"));
    CHECK(u.next.buffs == BuffsLook::Column);
}

// 3.0.11-beta.63 (the concept's question 17: variant 1 «ще N» by default, variant 2 - a second column - a choice):
// [interface] buffs_overflow - what the buffs' column does not hold: under «ще N» or in a second column on its left
// (the row: a second row); a choice of «Інтерфейс», applied at once
TEST(config_reads_the_buffs_overflow) {
    CHECK(noUi("").buffsOverflow == BuffsOverflow::More);
    CHECK(noUi("buffs_overflow = column\n").buffsOverflow == BuffsOverflow::Column);
    CHECK(noUi("buffs_overflow = more\n").buffsOverflow == BuffsOverflow::More);
    CHECK(noUi("buffs_overflow = scroll\n").buffsOverflow == BuffsOverflow::More);
    const settings::IniSetting* s = settings::findSetting("interface.buffs_overflow");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, noUi("buffs_overflow = column\n")), std::string("column"));
    CHECK_EQ(settings::tokenOf(*s, noUi("")), std::string("more"));
    CHECK(appliesNow("interface.buffs_overflow"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("buffs_overflow = column\n"));
    CHECK(u.next.buffsOverflow == BuffsOverflow::Column);
}

// 3.0.11-beta.36 (the owner: «кольца или полоски на выбор. приступай»): [interface] character - the game's own Basic
// Info window (off), the character block with bars, with rings; a choice of «Інтерфейс», applied at once
TEST(config_reads_the_character_block) {
    CHECK(noUi("").character == CharacterLook::Off);
    CHECK(noUi("character = bars\n").character == CharacterLook::Bars);
    CHECK(noUi("character = rings\n").character == CharacterLook::Rings);
    CHECK(noUi("character = boxes\n").character == CharacterLook::Off);
    const settings::IniSetting* s = settings::findSetting("interface.character");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, noUi("character = rings\n")), std::string("rings"));
    CHECK_EQ(settings::tokenOf(*s, noUi("")), std::string("off"));
    CHECK(appliesNow("interface.character"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("character = bars\n"));
    CHECK(u.next.character == CharacterLook::Bars);
}

// 3.0.11-beta.37 (R-TWU5W8: «не появились ни касты ни новый блок персонажа» - both new parts off, and the new minimap
// over the game's buff icons hid them): with the new minimap on, the buffs' column and the character block come with it
// unless the file says otherwise
TEST(config_new_interface_parts_come_with_the_minimap) {
    const Config on = noUi("minimap = 1\n");
    CHECK(on.buffs == BuffsLook::Column && on.character == CharacterLook::Bars);
    const Config chosen = noUi("minimap = 1\nbuffs = off\ncharacter = rings\n");
    CHECK(chosen.buffs == BuffsLook::Off && chosen.character == CharacterLook::Rings);
    const Config off = noUi("minimap = 0\n");
    CHECK(off.buffs == BuffsLook::Off && off.character == CharacterLook::Off);
    const settings::IniSetting* s = settings::findSetting("interface.buffs");
    CHECK(s != nullptr && settings::tokenOf(*s, on) == "column");  // the app shows what is in effect
    const LiveUpdate u = liveUpdate(noUi(""), on);  // the minimap switched on: they come at once
    CHECK(u.next.buffs == BuffsLook::Column && u.next.character == CharacterLook::Bars);
}

// 3.0.11-beta.38 (the owner: «делай карточку цели»): [interface] target - the target's card at the top middle; it
// comes with the new minimap as the other parts do, unless the file names it
TEST(config_reads_the_target_card) {
    CHECK(!noUi("").targetCard);
    CHECK(noUi("target = 1\n").targetCard);
    CHECK(noUi("minimap = 1\n").targetCard);
    CHECK(!noUi("minimap = 1\ntarget = 0\n").targetCard);
    const settings::IniSetting* s = settings::findSetting("interface.target");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK_EQ(settings::tokenOf(*s, noUi("minimap = 1\n")), std::string("1"));
    CHECK(appliesNow("interface.target"));
    CHECK(liveUpdate(noUi(""), noUi("target = 1\n")).next.targetCard);
}

// 3.0.11-beta.39 (the owner: «делай» - the party): [interface] party - the party's panel at the top left; it comes with
// the new minimap as the other parts do, unless the file names it
TEST(config_reads_the_party_panel) {
    CHECK(!noUi("").partyPanel);
    CHECK(noUi("party = 1\n").partyPanel);
    CHECK(noUi("minimap = 1\n").partyPanel);
    CHECK(!noUi("minimap = 1\nparty = 0\n").partyPanel);
    const settings::IniSetting* s = settings::findSetting("interface.party");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.party"));
    CHECK(liveUpdate(noUi(""), noUi("party = 1\n")).next.partyPanel);
}

// 3.0.11-beta.70 (the concept's tracker, «держать перед глазами»): [interface] tracker - the pinned quests, way and
// items left of the buffs; it comes with the new minimap as the other parts do, unless the file names it
TEST(config_reads_the_tracker) {
    CHECK(!noUi("").tracker);
    CHECK(noUi("tracker = 1\n").tracker);
    CHECK(noUi("minimap = 1\n").tracker);
    CHECK(!noUi("minimap = 1\ntracker = 0\n").tracker);
    const settings::IniSetting* s = settings::findSetting("interface.tracker");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.tracker"));
    CHECK(liveUpdate(noUi(""), noUi("tracker = 1\n")).next.tracker);
}

// 3.0.11-beta.40 (the owner: «делай дальше» - stage 8, the hotbar first): [interface] hotbar - the hotkeys' bar
// redrawn at the bottom middle; it comes with the new minimap as the other parts do, unless the file names it
TEST(config_reads_the_hotbar) {
    CHECK(!noUi("").hotbar);
    CHECK(noUi("hotbar = 1\n").hotbar);
    CHECK(noUi("minimap = 1\n").hotbar);
    CHECK(!noUi("minimap = 1\nhotbar = 0\n").hotbar);
    const settings::IniSetting* s = settings::findSetting("interface.hotbar");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.hotbar"));
    CHECK(liveUpdate(noUi(""), noUi("hotbar = 1\n")).next.hotbar);
}

// 3.0.11-beta.41 (the owner: «делай чат»): [interface] chat - the chat redrawn at the bottom left; it comes with the
// new minimap as the other parts do, unless the file names it
TEST(config_reads_the_chat) {
    CHECK(!noUi("").chat);
    CHECK(noUi("chat = 1\n").chat);
    CHECK(noUi("minimap = 1\n").chat);
    CHECK(!noUi("minimap = 1\nchat = 0\n").chat);
    const settings::IniSetting* s = settings::findSetting("interface.chat");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.chat"));
    CHECK(liveUpdate(noUi(""), noUi("chat = 1\n")).next.chat);
}

// 3.0.11-beta.186 (the tester: «тут дублюючий інтерфейс, треба десь вимикач зробити»): [interface] item_toast - «+N …»
// of a thing picked up over the bag, the game's own box at the top hidden for it; off: the game's box alone
TEST(config_reads_the_item_toast) {
    CHECK(noUi("").itemToast);
    CHECK(!noUi("item_toast = 0\n").itemToast);
    const settings::IniSetting* s = settings::findSetting("interface.item_toast");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.item_toast"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("item_toast = 0\n"));
    CHECK(!u.next.itemToast && u.applied.size() == 1 && u.waiting.empty());
    CHECK_EQ(settings::tokenOf(*s, noUi("item_toast = 0\n")), std::string("0"));
}

// 3.0.11-beta.42 (the owner: «делай окна»): [interface] items and status - «Речі» and «Характеристики» redrawn; they
// come with the new minimap as the other parts do, unless the file names them
TEST(config_reads_the_windows) {
    CHECK(!noUi("").itemsWindow && !noUi("").statusWindow);
    CHECK(noUi("items = 1\n").itemsWindow && noUi("status = 1\n").statusWindow);
    const Config c = noUi("minimap = 1\nstatus = 0\n");
    CHECK(c.itemsWindow && !c.statusWindow);
    for (const char* key : {"interface.items", "interface.status"}) {
        const settings::IniSetting* s = settings::findSetting(key);
        CHECK(s != nullptr);
        if (!s) continue;
        CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
        CHECK(appliesNow(key));
    }
    CHECK(liveUpdate(noUi(""), noUi("items = 1\n")).next.itemsWindow);
}

// 3.0.11-beta.46 (the concept's motion): [interface] animations - on unless the file says 0, at once
TEST(config_reads_the_animations) {
    CHECK(noUi("").animations && noUi("animations = 1\n").animations);
    CHECK(!noUi("animations = 0\n").animations);
    const settings::IniSetting* s = settings::findSetting("interface.animations");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.animations"));
    CHECK(!liveUpdate(noUi(""), noUi("animations = 0\n")).next.animations);
}

// 3.0.11-beta.47 (the concept's «Непрозрачность»): [interface] panel_opacity 50-100 (80), window_opacity 80-100 (90)
TEST(config_reads_the_opacities) {
    const Config d = noUi("");
    CHECK(d.hudOpacity == 80 && d.windowOpacity == 90);
    const Config c = noUi("panel_opacity = 65\nwindow_opacity = 95\n");
    CHECK(c.hudOpacity == 65 && c.windowOpacity == 95);
    const Config low = noUi("panel_opacity = 10\nwindow_opacity = 40\n");
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
    CHECK(!noUi("").castLabel && noUi("cast = 1\n").castLabel);
    CHECK(noUi("minimap = 1\n").castLabel);
    CHECK(!noUi("minimap = 1\ncast = 0\n").castLabel);
    const settings::IniSetting* s = settings::findSetting("interface.cast");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.cast"));
    CHECK(liveUpdate(noUi(""), noUi("cast = 1\n")).next.castLabel);
}

// 3.0.18-beta.67 (the owner: «вообще в этой игре оно не надо»): the world's dimming under the windows gone - an old
// file's dim_world read and dropped, no switch in the app
TEST(config_drops_the_world_dim) {
    CHECK(settings::findSetting("interface.dim_world") == nullptr);
    CHECK(noUi("dim_world = 1\n").uaui == noUi("").uaui);  // read, nothing kept
}

// 3.0.11-beta.50 (the concept's low HP): [interface] low_hp_edge 0-100 - none unless asked for
TEST(config_reads_the_low_hp_edge) {
    CHECK(noUi("").lowHpEdge == 0 && noUi("minimap = 1\n").lowHpEdge == 0);
    CHECK(noUi("low_hp_edge = 60\n").lowHpEdge == 60);
    CHECK(noUi("low_hp_edge = 250\n").lowHpEdge == 100);
    const settings::IniSetting* s = settings::findSetting("interface.low_hp_edge");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Slider);
    CHECK(appliesNow("interface.low_hp_edge"));
}

// 3.0.11-beta.67 (the concept: «Масштаб текста — отдельная настройка 80–150 %»): [interface] text_size - the
// windows one reads in, per cent; a choice of «Інтерфейс», applied at once
TEST(config_reads_the_text_size) {
    CHECK(noUi("").textSize == 100);
    CHECK(noUi("text_size = 130\n").textSize == 130);
    CHECK(noUi("text_size = 40\n").textSize == 80);
    CHECK(noUi("text_size = 900\n").textSize == 150);
    CHECK(noUi("text_size = big\n").textSize == 100);
    const settings::IniSetting* s = settings::findSetting("interface.text_size");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, noUi("text_size = 115\n")), std::string("115"));
    CHECK_EQ(settings::tokenOf(*s, noUi("")), std::string("100"));
    CHECK(appliesNow("interface.text_size"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("text_size = 150\n"));
    CHECK(u.next.textSize == 150);
}

// 3.0.11-beta.54 (R-JK5455): [interface] size - auto unless a per cent is named
TEST(config_reads_the_interface_size) {
    CHECK(noUi("").uiSize == 0 && noUi("size = auto\n").uiSize == 0);
    CHECK(noUi("size = 85\n").uiSize == 85);
    CHECK(noUi("size = 400\n").uiSize == 150);
    const settings::IniSetting* s = settings::findSetting("interface.size");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK_EQ(settings::tokenOf(*s, noUi("size = 115\n")), std::string("115"));
    CHECK_EQ(settings::tokenOf(*s, noUi("")), std::string("auto"));
    CHECK(appliesNow("interface.size"));
}

// 3.0.11-beta.95 (the owner: «Полоски HP мобов - всем показывай»): [interface] mob_hp = off | target | wounded | all -
// the monsters' HP bars; the target and the wounded by default, a choice of «Інтерфейс», at once
TEST(config_mob_hp) {
    // 3.0.11-beta.100 (the owner: «а можно чтобы над мобами всегда хп висело?»): every monster's by default
    CHECK(noUi("").mobHp == MobHp::All);
    CHECK(noUi("mob_hp = wounded\n").mobHp == MobHp::Wounded);
    CHECK(noUi("mob_hp = all\n").mobHp == MobHp::All);
    CHECK(noUi("mob_hp = target\n").mobHp == MobHp::Target);
    CHECK(noUi("mob_hp = off\n").mobHp == MobHp::Off);
    const settings::IniSetting* s = settings::findSetting("interface.mob_hp");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK(appliesNow("interface.mob_hp"));
    CHECK(liveUpdate(noUi(""), noUi("mob_hp = off\n")).next.mobHp == MobHp::Off);
}

// 3.0.11-beta.100 (the owner: «доделаем окна интерфейсов»; the concept's «Магазин»): [interface] shop - the NPCs' shops
// in the new look; on with the new minimap unless the file says, a box of «Інтерфейс», at once
TEST(config_interface_shop) {
    CHECK(!noUi("").shopWindow);
    CHECK(noUi("minimap = 1\n").shopWindow);
    CHECK(!noUi("minimap = 1\nshop = 0\n").shopWindow);
    CHECK(noUi("shop = 1\n").shopWindow);
    const settings::IniSetting* s = settings::findSetting("interface.shop");
    CHECK(s != nullptr);
    if (s) CHECK_EQ(settings::tokenOf(*s, noUi("shop = 1\n")), std::string("1"));
    CHECK(appliesNow("interface.shop"));
}

// 3.0.11-beta.115 (the windows plan's stage 6): [interface] storage - the Kafra storage in the new look («Склад» beside
// «Речі»); on with the new minimap unless the file says, a box of «Інтерфейс», at once
TEST(config_interface_storage) {
    CHECK(!noUi("").storageWindow);
    CHECK(noUi("minimap = 1\n").storageWindow);
    CHECK(!noUi("minimap = 1\nstorage = 0\n").storageWindow);
    CHECK(noUi("storage = 1\n").storageWindow);
    const settings::IniSetting* s = settings::findSetting("interface.storage");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    if (s) CHECK_EQ(settings::tokenOf(*s, noUi("storage = 1\n")), std::string("1"));
    CHECK(appliesNow("interface.storage"));
    CHECK(liveUpdate(noUi(""), noUi("storage = 1\n")).next.storageWindow);
}

// 3.0.11-beta.116 (the windows plan's stage 7): [interface] trade - the trade in the new look («Обмін» beside «Речі»);
// on with the new minimap unless the file says, a box of «Інтерфейс», at once
TEST(config_interface_trade) {
    CHECK(!noUi("").tradeWindow);
    CHECK(noUi("minimap = 1\n").tradeWindow);
    CHECK(!noUi("minimap = 1\ntrade = 0\n").tradeWindow);
    CHECK(noUi("trade = 1\n").tradeWindow);
    const settings::IniSetting* s = settings::findSetting("interface.trade");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    if (s) CHECK_EQ(settings::tokenOf(*s, noUi("trade = 1\n")), std::string("1"));
    CHECK(appliesNow("interface.trade"));
    CHECK(liveUpdate(noUi(""), noUi("trade = 1\n")).next.tradeWindow);
}

// 3.0.11-beta.187 (the tester took the minimap's fan for the character's sight): [interface] minimap_view - the
// camera's fan round the arrow, on; off: the arrow alone
TEST(config_reads_the_minimap_view) {
    CHECK(noUi("").minimapView);
    CHECK(!noUi("minimap_view = 0\n").minimapView);
    const settings::IniSetting* s = settings::findSetting("interface.minimap_view");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.minimap_view"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("minimap_view = 0\n"));
    CHECK(!u.next.minimapView && u.applied.size() == 1 && u.waiting.empty());
    CHECK_EQ(settings::tokenOf(*s, noUi("minimap_view = 0\n")), std::string("0"));
}

// 3.0.11-beta.119 (R-5Z6J2S, the owner «делать»): [interface] minimap_kafra and minimap_party - the Kafras and the
// party on the maps, each its own layer (on at first), switched on the maps themselves too; boxes of «Інтерфейс»,
// at once
TEST(config_interface_minimap_layers) {
    const Config c = noUi("");
    CHECK(c.minimapKafra && c.minimapParty);
    CHECK(!noUi("minimap_kafra = 0\n").minimapKafra);
    CHECK(!noUi("minimap_party = 0\n").minimapParty);
    for (const char* key : {"interface.minimap_kafra", "interface.minimap_party"}) {
        const settings::IniSetting* s = settings::findSetting(key);
        CHECK(s != nullptr && s->page == settings::Page::Interface);
        CHECK(appliesNow(key));
    }
    const LiveUpdate u =
        liveUpdate(noUi(""), noUi("minimap_kafra = 0\nminimap_party = 0\n"));
    CHECK(!u.next.minimapKafra && !u.next.minimapParty);
}

// 3.0.11-beta.120 (the owner: «карта ... какаято не красивая, может другую попробовать или сделать переключалку слоев
// разных?»): [interface] minimap_style - the maps' picture: the game's own (picture) or «Схема» from the walkable cells
// (scheme); a choice of «Інтерфейс», at once, switched on the maps themselves too
TEST(config_interface_minimap_style) {
    CHECK(noUi("").minimapStyle == MapStyle::Picture);
    CHECK(noUi("minimap_style = scheme\n").minimapStyle == MapStyle::Scheme);
    CHECK(noUi("minimap_style = other\n").minimapStyle == MapStyle::Picture);
    const settings::IniSetting* s = settings::findSetting("interface.minimap_style");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    if (s) CHECK_EQ(settings::tokenOf(*s, noUi("minimap_style = scheme\n")), std::string("scheme"));
    CHECK(appliesNow("interface.minimap_style"));
    CHECK(liveUpdate(noUi(""), noUi("minimap_style = scheme\n")).next.minimapStyle ==
          MapStyle::Scheme);
}

// 3.0.11-beta.122 (the network ideas, 7): [interface] net_badge - the round trip to the map server on the minimap's bar
// (a coloured dot, a stall in red); on at first, a box of «Інтерфейс», at once
TEST(config_interface_net_badge) {
    CHECK(noUi("").netBadge);
    CHECK(!noUi("net_badge = 0\n").netBadge);
    const settings::IniSetting* s = settings::findSetting("interface.net_badge");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.net_badge"));
    CHECK(!liveUpdate(noUi(""), noUi("net_badge = 0\n")).next.netBadge);
}

// 3.0.11-beta.123 (the network plan's step 3): [interface] smooth_fixes - the server's position fixes of 1-3 cells
// glided to, not jumped; on at first, a box of «Інтерфейс», at once
TEST(config_interface_smooth_fixes) {
    CHECK(noUi("").smoothFixes);
    CHECK(!noUi("smooth_fixes = 0\n").smoothFixes);
    const settings::IniSetting* s = settings::findSetting("interface.smooth_fixes");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.smooth_fixes"));
    CHECK(!liveUpdate(noUi(""), noUi("smooth_fixes = 0\n")).next.smoothFixes);
}

// 3.0.11-beta.125 (the owner: «когда играю арчером ... не видно сколько осталось»): [interface] hotbar_ammo - the ammo
// worn and how many left on a pill over the hotbar; on at first, a box of «Інтерфейс», at once
TEST(config_interface_hotbar_ammo) {
    CHECK(noUi("").hotbarAmmo);
    CHECK(!noUi("hotbar_ammo = 0\n").hotbarAmmo);
    const settings::IniSetting* s = settings::findSetting("interface.hotbar_ammo");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.hotbar_ammo"));
    CHECK(!liveUpdate(noUi(""), noUi("hotbar_ammo = 0\n")).next.hotbarAmmo);
}

// 3.0.16-beta.8 (the owner: «для магов и пристов ... гемов, а для монков ... сферы»): [interface] hotbar_supplies -
// off at first, a box of «Інтерфейс», at once
TEST(config_interface_hotbar_supplies) {
    CHECK(!noUi("").hotbarSupplies);
    CHECK(noUi("hotbar_supplies = 1\n").hotbarSupplies);
    const settings::IniSetting* s = settings::findSetting("interface.hotbar_supplies");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.hotbar_supplies"));
    CHECK(liveUpdate(noUi(""), noUi("hotbar_supplies = 1\n")).next.hotbarSupplies);
}

// 3.0.11-beta.128 (the network plan's step 5): [debug] quick_acks - the game's socket acknowledges each of the server's
// packets at once (Windows waits up to 200 ms for a lone one): the server sees a lost packet sooner; on, the
// developer's page, at once
TEST(config_debug_quick_acks) {
    CHECK(noUi("").quickAcks);
    CHECK(!noUi("[debug]\nquick_acks = 0\n").quickAcks);
    const settings::IniSetting* s = settings::findSetting("debug.quick_acks");
    CHECK(s != nullptr && s->page == settings::Page::Debug);
    CHECK(appliesNow("debug.quick_acks"));
    CHECK(!liveUpdate(noUi(""), noUi("[debug]\nquick_acks = 0\n")).next.quickAcks);
}

// 3.0.11-beta.129 (the owner «делай»): [interface] ammo_sound - a short soft chime when 10 arrows are left and when none
// are; on at first, a box of «Інтерфейс», at once
TEST(config_interface_ammo_sound) {
    CHECK(noUi("").ammoSound);
    CHECK(!noUi("ammo_sound = 0\n").ammoSound);
    const settings::IniSetting* s = settings::findSetting("interface.ammo_sound");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.ammo_sound"));
    CHECK(!liveUpdate(noUi(""), noUi("ammo_sound = 0\n")).next.ammoSound);
}

// 3.0.11-beta.130 (the network plan's step 5; the owner «делаем»): [interface] predict_walk - the character walks at the
// click, not a round trip later (an experiment); OFF at first, a box of «Інтерфейс», at once
TEST(config_interface_predict_walk) {
    CHECK(!noUi("").predictWalk);
    CHECK(noUi("predict_walk = 1\n").predictWalk);
    const settings::IniSetting* s = settings::findSetting("interface.predict_walk");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.predict_walk"));
    CHECK(liveUpdate(noUi(""), noUi("predict_walk = 1\n")).next.predictWalk);
}

// 3.0.11-beta.140 (the owner «давай»): [interface] menu - the game's menu and death screen redrawn; on with the new
// interface (its minimap) unless the file names it, as the other windows; a box of «Інтерфейс», at once
TEST(config_interface_menu_window) {
    CHECK(!noUi("").menuWindow);
    CHECK(noUi("minimap = 1\n").menuWindow);
    CHECK(!noUi("minimap = 1\nmenu = 0\n").menuWindow);
    CHECK(noUi("menu = 1\n").menuWindow);
    const settings::IniSetting* s = settings::findSetting("interface.menu");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.menu"));
    CHECK(liveUpdate(noUi(""), noUi("menu = 1\n")).next.menuWindow);
}

// 3.0.11-beta.149 (the owner: «сделай все без остановки»): [interface] cards - the item card and the skill card in the
// new look, on with the new minimap, at once
TEST(config_interface_cards) {
    CHECK(!noUi("").cardWindows);
    CHECK(noUi("minimap = 1\n").cardWindows);
    CHECK(!noUi("minimap = 1\ncards = 0\n").cardWindows);
    CHECK(noUi("cards = 1\n").cardWindows);
    const settings::IniSetting* s = settings::findSetting("interface.cards");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.cards"));
    CHECK(liveUpdate(noUi(""), noUi("cards = 1\n")).next.cardWindows);
}

// 3.0.11-beta.150: [interface] asks - the requests and the NPC's inputs in the new look, on with the new minimap,
// at once
TEST(config_interface_asks) {
    CHECK(!noUi("").askWindows);
    CHECK(noUi("minimap = 1\n").askWindows);
    CHECK(!noUi("minimap = 1\nasks = 0\n").askWindows);
    CHECK(noUi("asks = 1\n").askWindows);
    const settings::IniSetting* s = settings::findSetting("interface.asks");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.asks"));
    CHECK(liveUpdate(noUi(""), noUi("asks = 1\n")).next.askWindows);
}

// 3.0.11-beta.151: [interface] messages - the game's message boxes in the new look, on with the new minimap, at once
TEST(config_interface_messages) {
    CHECK(!noUi("").messageBoxes);
    CHECK(noUi("minimap = 1\n").messageBoxes);
    CHECK(!noUi("minimap = 1\nmessages = 0\n").messageBoxes);
    CHECK(noUi("messages = 1\n").messageBoxes);
    const settings::IniSetting* s = settings::findSetting("interface.messages");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.messages"));
    CHECK(liveUpdate(noUi(""), noUi("messages = 1\n")).next.messageBoxes);
}

// 3.0.11-beta.151: [interface] login - the game's login window in the new look, on with the new minimap, at once
TEST(config_interface_login) {
    CHECK(!noUi("").loginWindow);
    CHECK(noUi("minimap = 1\n").loginWindow);
    CHECK(!noUi("minimap = 1\nlogin = 0\n").loginWindow);
    CHECK(noUi("login = 1\n").loginWindow);
    const settings::IniSetting* s = settings::findSetting("interface.login");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.login"));
    CHECK(liveUpdate(noUi(""), noUi("login = 1\n")).next.loginWindow);
}

// 3.0.11-beta.152: [interface] char_select - the game's character select in the new look, on with the new minimap, at
// once
TEST(config_interface_char_select) {
    CHECK(!noUi("").charSelectWindow);
    CHECK(noUi("minimap = 1\n").charSelectWindow);
    CHECK(!noUi("minimap = 1\nchar_select = 0\n").charSelectWindow);
    CHECK(noUi("char_select = 1\n").charSelectWindow);
    const settings::IniSetting* s = settings::findSetting("interface.char_select");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.char_select"));
    CHECK(liveUpdate(noUi(""), noUi("char_select = 1\n")).next.charSelectWindow);
}

// 3.0.11-beta.153: [interface] char_make - the game's character make in the new look, on with the new minimap, at once
TEST(config_interface_char_make) {
    CHECK(!noUi("").charMakeWindow);
    CHECK(noUi("minimap = 1\n").charMakeWindow);
    CHECK(!noUi("minimap = 1\nchar_make = 0\n").charMakeWindow);
    CHECK(noUi("char_make = 1\n").charMakeWindow);
    const settings::IniSetting* s = settings::findSetting("interface.char_make");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.char_make"));
    CHECK(liveUpdate(noUi(""), noUi("char_make = 1\n")).next.charMakeWindow);
}

// 3.0.11-beta.154: [interface] cart - the merchant's cart as «Візок», on with the new minimap, at once
TEST(config_interface_cart) {
    CHECK(!noUi("").cartWindow);
    CHECK(noUi("minimap = 1\n").cartWindow);
    CHECK(!noUi("minimap = 1\ncart = 0\n").cartWindow);
    CHECK(noUi("cart = 1\n").cartWindow);
    const settings::IniSetting* s = settings::findSetting("interface.cart");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.cart"));
    CHECK(liveUpdate(noUi(""), noUi("cart = 1\n")).next.cartWindow);
}

// 3.0.11-beta.155: [interface] counts - the game's «how many» window as a card, on with the new minimap, at once
TEST(config_interface_counts) {
    CHECK(!noUi("").countWindow);
    CHECK(noUi("minimap = 1\n").countWindow);
    CHECK(!noUi("minimap = 1\ncounts = 0\n").countWindow);
    CHECK(noUi("counts = 1\n").countWindow);
    const settings::IniSetting* s = settings::findSetting("interface.counts");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.counts"));
    CHECK(liveUpdate(noUi(""), noUi("counts = 1\n")).next.countWindow);
}

// 3.0.11-beta.156: [interface] vending - another's shop as «Лавка», on with the new minimap, at once
TEST(config_interface_vending) {
    CHECK(!noUi("").vendWindow);
    CHECK(noUi("minimap = 1\n").vendWindow);
    CHECK(!noUi("minimap = 1\nvending = 0\n").vendWindow);
    CHECK(noUi("vending = 1\n").vendWindow);
    const settings::IniSetting* s = settings::findSetting("interface.vending");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.vending"));
    CHECK(liveUpdate(noUi(""), noUi("vending = 1\n")).next.vendWindow);
}

// 3.0.11-beta.157: [interface] picks - the magnifier's and a card's choice of things, on with the new minimap, at once
TEST(config_interface_picks) {
    CHECK(!noUi("").pickWindows);
    CHECK(noUi("minimap = 1\n").pickWindows);
    CHECK(!noUi("minimap = 1\npicks = 0\n").pickWindows);
    CHECK(noUi("picks = 1\n").pickWindows);
    const settings::IniSetting* s = settings::findSetting("interface.picks");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.picks"));
    CHECK(liveUpdate(noUi(""), noUi("picks = 1\n")).next.pickWindows);
}

// 3.0.11-beta.158: [interface] party_setup - the party's settings as a card, on with the new minimap, at once
TEST(config_interface_party_setup) {
    CHECK(!noUi("").partySetupWindow);
    CHECK(noUi("minimap = 1\n").partySetupWindow);
    CHECK(!noUi("minimap = 1\nparty_setup = 0\n").partySetupWindow);
    CHECK(noUi("party_setup = 1\n").partySetupWindow);
    const settings::IniSetting* s = settings::findSetting("interface.party_setup");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.party_setup"));
    CHECK(liveUpdate(noUi(""), noUi("party_setup = 1\n")).next.partySetupWindow);
}

// 3.0.11-beta.159: [interface] pet - the pet's window as a card, on with the new minimap, at once
TEST(config_interface_pet) {
    CHECK(!noUi("").petWindow);
    CHECK(noUi("minimap = 1\n").petWindow);
    CHECK(!noUi("minimap = 1\npet = 0\n").petWindow);
    CHECK(noUi("pet = 1\n").petWindow);
    const settings::IniSetting* s = settings::findSetting("interface.pet");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.pet"));
    CHECK(liveUpdate(noUi(""), noUi("pet = 1\n")).next.petWindow);
}

// 3.0.11-beta.160: [interface] homun - the homunculus' window as a card, on with the new minimap, at once
TEST(config_interface_homun) {
    CHECK(!noUi("").homunWindow);
    CHECK(noUi("minimap = 1\n").homunWindow);
    CHECK(!noUi("minimap = 1\nhomun = 0\n").homunWindow);
    CHECK(noUi("homun = 1\n").homunWindow);
    const settings::IniSetting* s = settings::findSetting("interface.homun");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.homun"));
    CHECK(liveUpdate(noUi(""), noUi("homun = 1\n")).next.homunWindow);
}

// 3.0.11-beta.161: [interface] merc - the mercenary's window as a card, on with the new minimap, at once
TEST(config_interface_merc) {
    CHECK(!noUi("").mercWindow);
    CHECK(noUi("minimap = 1\n").mercWindow);
    CHECK(!noUi("minimap = 1\nmerc = 0\n").mercWindow);
    CHECK(noUi("merc = 1\n").mercWindow);
    const settings::IniSetting* s = settings::findSetting("interface.merc");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.merc"));
    CHECK(liveUpdate(noUi(""), noUi("merc = 1\n")).next.mercWindow);
}

// 3.0.11-beta.162: [interface] options - the game's options window as a card, on with the new minimap, at once
TEST(config_interface_options) {
    CHECK(!noUi("").optionsWindow);
    CHECK(noUi("minimap = 1\n").optionsWindow);
    CHECK(!noUi("minimap = 1\noptions = 0\n").optionsWindow);
    CHECK(noUi("options = 1\n").optionsWindow);
    const settings::IniSetting* s = settings::findSetting("interface.options");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.options"));
    CHECK(liveUpdate(noUi(""), noUi("options = 1\n")).next.optionsWindow);
}

// 3.0.11-beta.163: [interface] social - the friends and party window as a card, on with the new minimap, at once
TEST(config_interface_social) {
    CHECK(!noUi("").socialWindow);
    CHECK(noUi("minimap = 1\n").socialWindow);
    CHECK(!noUi("minimap = 1\nsocial = 0\n").socialWindow);
    CHECK(noUi("social = 1\n").socialWindow);
    const settings::IniSetting* s = settings::findSetting("interface.social");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.social"));
    CHECK(liveUpdate(noUi(""), noUi("social = 1\n")).next.socialWindow);
}

// 3.0.11-beta.164: [interface] servers - the game's server select as a card, on with the new minimap, at once
TEST(config_interface_servers) {
    CHECK(!noUi("").serverWindow);
    CHECK(noUi("minimap = 1\n").serverWindow);
    CHECK(!noUi("minimap = 1\nservers = 0\n").serverWindow);
    CHECK(noUi("servers = 1\n").serverWindow);
    const settings::IniSetting* s = settings::findSetting("interface.servers");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.servers"));
    CHECK(liveUpdate(noUi(""), noUi("servers = 1\n")).next.serverWindow);
}

// 3.0.11-beta.165: [interface] guild - the guild's window as a card, on with the new minimap, at once
TEST(config_interface_guild) {
    CHECK(!noUi("").guildWindow);
    CHECK(noUi("minimap = 1\n").guildWindow);
    CHECK(!noUi("minimap = 1\nguild = 0\n").guildWindow);
    CHECK(noUi("guild = 1\n").guildWindow);
    const settings::IniSetting* s = settings::findSetting("interface.guild");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.guild"));
    CHECK(liveUpdate(noUi(""), noUi("guild = 1\n")).next.guildWindow);
}

// 3.0.11-beta.166: [interface] my_shop - one's own open shop as a card, on with the new minimap, at once
TEST(config_interface_my_shop) {
    CHECK(!noUi("").myShopWindow);
    CHECK(noUi("minimap = 1\n").myShopWindow);
    CHECK(!noUi("minimap = 1\nmy_shop = 0\n").myShopWindow);
    CHECK(noUi("my_shop = 1\n").myShopWindow);
    const settings::IniSetting* s = settings::findSetting("interface.my_shop");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.my_shop"));
    CHECK(liveUpdate(noUi(""), noUi("my_shop = 1\n")).next.myShopWindow);
}

// 3.0.11-beta.168: [interface] whisper_setup - the whisper's settings as a card, on with the new minimap, at once
TEST(config_interface_whisper_setup) {
    CHECK(!noUi("").whisperSetupWindow);
    CHECK(noUi("minimap = 1\n").whisperSetupWindow);
    CHECK(!noUi("minimap = 1\nwhisper_setup = 0\n").whisperSetupWindow);
    CHECK(noUi("whisper_setup = 1\n").whisperSetupWindow);
    const settings::IniSetting* s = settings::findSetting("interface.whisper_setup");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.whisper_setup"));
    CHECK(liveUpdate(noUi(""), noUi("whisper_setup = 1\n")).next.whisperSetupWindow);
}

// 3.0.11-beta.169: [interface] macros - the chat's macros as a card, on with the new minimap, at once
TEST(config_interface_macros) {
    CHECK(!noUi("").macroWindow);
    CHECK(noUi("minimap = 1\n").macroWindow);
    CHECK(!noUi("minimap = 1\nmacros = 0\n").macroWindow);
    CHECK(noUi("macros = 1\n").macroWindow);
    const settings::IniSetting* s = settings::findSetting("interface.macros");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.macros"));
    CHECK(liveUpdate(noUi(""), noUi("macros = 1\n")).next.macroWindow);
}

// 3.0.11-beta.204: [interface] tips - the game's «Tip of the Day» as a card, on with the new minimap, at once
TEST(config_interface_tips) {
    CHECK(!noUi("").tipWindow);
    CHECK(noUi("minimap = 1\n").tipWindow);
    CHECK(!noUi("minimap = 1\ntips = 0\n").tipWindow);
    CHECK(noUi("tips = 1\n").tipWindow);
    const settings::IniSetting* s = settings::findSetting("interface.tips");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.tips"));
    CHECK(liveUpdate(noUi(""), noUi("tips = 1\n")).next.tipWindow);
    CHECK(settings::tokenOf(*s, noUi("tips = 1\n")) == "1");
}

// 3.0.11-beta.205: [interface] emotes - the game's emotion list (Alt+L) as a card, on with the new minimap, at once
TEST(config_interface_emotes) {
    CHECK(!noUi("").emoteWindow);
    CHECK(noUi("minimap = 1\n").emoteWindow);
    CHECK(!noUi("minimap = 1\nemotes = 0\n").emoteWindow);
    CHECK(noUi("emotes = 1\n").emoteWindow);
    const settings::IniSetting* s = settings::findSetting("interface.emotes");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.emotes"));
    CHECK(liveUpdate(noUi(""), noUi("emotes = 1\n")).next.emoteWindow);
    CHECK(settings::tokenOf(*s, noUi("emotes = 1\n")) == "1");
}

// 3.0.11-beta.206: [interface] commands - the game's command list (Alt+Y) as a card, on with the new minimap, at once
TEST(config_interface_commands) {
    CHECK(!noUi("").commandWindow);
    CHECK(noUi("minimap = 1\n").commandWindow);
    CHECK(!noUi("minimap = 1\ncommands = 0\n").commandWindow);
    CHECK(noUi("commands = 1\n").commandWindow);
    const settings::IniSetting* s = settings::findSetting("interface.commands");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.commands"));
    CHECK(liveUpdate(noUi(""), noUi("commands = 1\n")).next.commandWindow);
    CHECK(settings::tokenOf(*s, noUi("commands = 1\n")) == "1");
}

// 3.0.11-beta.207: [interface] chat_rooms - the game's chat room windows (Alt+C) as cards, on with the new minimap
TEST(config_interface_chat_rooms) {
    CHECK(!noUi("").chatRoomWindows);
    CHECK(noUi("minimap = 1\n").chatRoomWindows);
    CHECK(!noUi("minimap = 1\nchat_rooms = 0\n").chatRoomWindows);
    CHECK(noUi("chat_rooms = 1\n").chatRoomWindows);
    const settings::IniSetting* s = settings::findSetting("interface.chat_rooms");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.chat_rooms"));
    CHECK(liveUpdate(noUi(""), noUi("chat_rooms = 1\n")).next.chatRoomWindows);
    CHECK(settings::tokenOf(*s, noUi("chat_rooms = 1\n")) == "1");
}

// 3.0.11-beta.209: [interface] detail_console - Alt+D opens the driver's console in place of the game's «Arrange
// Detail» window, on with the new minimap, at once
TEST(config_interface_detail_console) {
    CHECK(!noUi("").detailConsole);
    CHECK(noUi("minimap = 1\n").detailConsole);
    CHECK(!noUi("minimap = 1\ndetail_console = 0\n").detailConsole);
    CHECK(noUi("detail_console = 1\n").detailConsole);
    const settings::IniSetting* s = settings::findSetting("interface.detail_console");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.detail_console"));
    CHECK(liveUpdate(noUi(""), noUi("detail_console = 1\n")).next.detailConsole);
    CHECK(settings::tokenOf(*s, noUi("detail_console = 1\n")) == "1");
}

// 3.0.11-beta.211: [interface] popups - the game's popup menus (a player's, a room member's, a guild member's) as
// cards, on with the new minimap, at once
TEST(config_interface_popups) {
    CHECK(!noUi("").popupMenus);
    CHECK(noUi("minimap = 1\n").popupMenus);
    CHECK(!noUi("minimap = 1\npopups = 0\n").popupMenus);
    CHECK(noUi("popups = 1\n").popupMenus);
    const settings::IniSetting* s = settings::findSetting("interface.popups");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.popups"));
    CHECK(liveUpdate(noUi(""), noUi("popups = 1\n")).next.popupMenus);
    CHECK(settings::tokenOf(*s, noUi("popups = 1\n")) == "1");
}

// 3.0.11-beta.212: [interface] whispers - the game's 1:1 whisper windows as cards, on with the new minimap, at once
TEST(config_interface_whispers) {
    CHECK(!noUi("").whisperWindows);
    CHECK(noUi("minimap = 1\n").whisperWindows);
    CHECK(!noUi("minimap = 1\nwhispers = 0\n").whisperWindows);
    CHECK(noUi("whispers = 1\n").whisperWindows);
    const settings::IniSetting* s = settings::findSetting("interface.whispers");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.whispers"));
    CHECK(liveUpdate(noUi(""), noUi("whispers = 1\n")).next.whisperWindows);
    CHECK(settings::tokenOf(*s, noUi("whispers = 1\n")) == "1");
}

// 3.0.11-beta.103 (the owner: «можешь сделать настройку, чтобы можно было выбрать эту стрелку в конфигураторе»):
// [interface] walk_arrow - the look of the stick's walking arrow, the doc's variants; the chevrons by default (its
// advice), a choice of «Інтерфейс» applied at once
TEST(config_walk_arrow) {
    CHECK(noUi("").walkArrow == WalkLook::Chevrons);
    const char* tokens[] = {"chevrons", "slim", "concept", "ring", "dots", "cell", "off"};
    const WalkLook looks[] = {WalkLook::Chevrons, WalkLook::Slim, WalkLook::Concept, WalkLook::Ring,
                              WalkLook::Dots,     WalkLook::Cell, WalkLook::Off};
    for (size_t i = 0; i < 7; ++i) {
        CHECK(parseConfig(std::string("[interface]\nwalk_arrow = ") + tokens[i] + "\n").walkArrow == looks[i]);
        CHECK(std::string(walkLookToken(looks[i])) == tokens[i]);
    }
    const settings::IniSetting* s = settings::findSetting("interface.walk_arrow");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK(appliesNow("interface.walk_arrow"));
    CHECK(liveUpdate(noUi(""), noUi("walk_arrow = ring\n")).next.walkArrow ==
          WalkLook::Ring);
}

// 3.0.11-beta.105 (the owner: «а можно клетку куда идти над стрелкой не рисовать?»): [interface] walk_mark - the mark
// at the arrow's end where the character goes (the diamond, the cell's corners or frame, the small ring); off by
// default
TEST(config_walk_mark) {
    CHECK(!noUi("").walkMark);
    CHECK(noUi("walk_mark = 1\n").walkMark);
    const settings::IniSetting* s = settings::findSetting("interface.walk_mark");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Check);
    CHECK(appliesNow("interface.walk_mark"));
    CHECK(liveUpdate(noUi(""), noUi("walk_mark = 1\n")).next.walkMark);
}


// 3.0.11-beta.142 (the owner's concept «маг, прист и хант на геймпаде», answer 9 «дать»): [interface] skill_cells - a
// ground skill's cells shown on the ground (core/skill_area: the server's own) after a cast, for the pad and the mouse;
// on unless the file says 0, a box of «Інтерфейс», at once
TEST(config_interface_skill_cells) {
    CHECK(noUi("").skillCells);
    CHECK(!noUi("skill_cells = 0\n").skillCells);
    const settings::IniSetting* s = settings::findSetting("interface.skill_cells");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.skill_cells"));
    CHECK(!liveUpdate(noUi(""), noUi("skill_cells = 0\n")).next.skillCells);
}

// 3.0.11-beta.143 (the concept «маг, прист и хант на геймпаде»): [interface] ground_counts - on a ground skill's slot
// of the hotbar its walls standing («2/3») or a trap's Traps in the bag («×37»), its edge flashing when a wall burnt
// out; on unless the file says 0, a box of «Інтерфейс», at once
TEST(config_interface_ground_counts) {
    CHECK(noUi("").groundCounts);
    CHECK(!noUi("ground_counts = 0\n").groundCounts);
    const settings::IniSetting* s = settings::findSetting("interface.ground_counts");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.ground_counts"));
    CHECK(!liveUpdate(noUi(""), noUi("ground_counts = 0\n")).next.groundCounts);
}

// 3.0.11-beta.173 (the login concept): [interface] login_status - the server's status line under the login window
// (it answers, the players online, the ping; not answering: one line and a quiet retry); on at first, a box of
// «Інтерфейс», at once
TEST(config_interface_login_status) {
    CHECK(noUi("").loginStatus);
    CHECK(!noUi("login_status = 0\n").loginStatus);
    const settings::IniSetting* s = settings::findSetting("interface.login_status");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.login_status"));
    CHECK(!liveUpdate(noUi(""), noUi("login_status = 0\n")).next.loginStatus);
}

// 3.0.11-beta.177 (the login concept's views): [interface] login_view - «Полиця» at first, «Класичний» or
// «Продовжити» chosen (the login screen's «Вигляд» or «Інтерфейс»), any other word the first; a choice applied at once
TEST(config_interface_login_view) {
    CHECK(noUi("").loginView == loginview::View::Shelf);
    CHECK(noUi("login_view = classic\n").loginView == loginview::View::Classic);
    CHECK(noUi("login_view = continue\n").loginView == loginview::View::Continue);
    CHECK(noUi("login_view = wall\n").loginView == loginview::View::Shelf);
    const settings::IniSetting* s = settings::findSetting("interface.login_view");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK(appliesNow("interface.login_view"));
    CHECK(liveUpdate(noUi(""), noUi("login_view = continue\n")).next.loginView ==
          loginview::View::Continue);
}

// 3.0.11-beta.174 (the login concept's news line): [interface] login_news - the site's event or news now above the
// status line on the login screen (a click opens it on uaro.kiev.ua); on at first, a box of «Інтерфейс», at once
TEST(config_interface_login_news) {
    CHECK(noUi("").loginNews);
    CHECK(!noUi("login_news = 0\n").loginNews);
    const settings::IniSetting* s = settings::findSetting("interface.login_news");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.login_news"));
    CHECK(!liveUpdate(noUi(""), noUi("login_news = 0\n")).next.loginNews);
}

// 3.0.11-beta.222 (the tester: «togle в налаштуваннях старий \ новий стиль»): [interface] minimap_classic - the
// minimap's marks as round dots in the game's colours; off at first, at once
TEST(config_reads_the_minimap_classic_marks) {
    CHECK(!noUi("").minimapClassic);
    CHECK(noUi("minimap_classic = 1\n").minimapClassic);
    const settings::IniSetting* s = settings::findSetting("interface.minimap_classic");
    CHECK(s != nullptr);
    CHECK(appliesNow("interface.minimap_classic"));
    const LiveUpdate u = liveUpdate(noUi(""), noUi("minimap_classic = 1\n"));
    CHECK(u.next.minimapClassic && u.applied.size() == 1 && u.waiting.empty());
    if (s) CHECK_EQ(settings::tokenOf(*s, noUi("minimap_classic = 1\n")), std::string("1"));
}

// 3.0.11-beta.236 (the concept «Блок персонажа»: the experience at the screen's bottom - «включить»): [interface]
// exp_bars, on by default
TEST(config_reads_the_experience_bars) {
    CHECK(noUi("").expBars);
    CHECK(!noUi("exp_bars = 0\n").expBars);
}

// 3.0.11-beta.237: [interface] exp_gain - a kill's experience over the monster, on by default
TEST(config_reads_the_experience_gains) {
    CHECK(noUi("").expGain);
    CHECK(!noUi("exp_gain = 0\n").expGain);
}

// 3.0.11-beta.240 (the chat concept, stage 1): [interface] chat_fade - 12 by default, 30, 0 never; others 12
TEST(config_reads_the_chat_fade) {
    CHECK(noUi("").chatFade == 12);
    CHECK(noUi("chat_fade = 30\n").chatFade == 30);
    CHECK(noUi("chat_fade = 0\n").chatFade == 0);
    CHECK(noUi("chat_fade = 7\n").chatFade == 12);
}

// 3.0.16-beta.33: [interface] vend_setup - opening one's shop as one card, at once. 3.0.18-beta.103 (R-S377Y8: «vending
// работает со старым интерфейсом» - the switch never in the new interface's choice, the file without it): not in the
// file, as [interface] my_shop (one's own open shop - on with the new minimap)
TEST(config_interface_vend_setup) {
    CHECK(!noUi("").vendSetupWindow);
    CHECK(noUi("minimap = 1\n").vendSetupWindow);
    CHECK(noUi("my_shop = 1\n").vendSetupWindow);
    CHECK(!noUi("minimap = 1\nmy_shop = 0\n").vendSetupWindow);
    CHECK(!noUi("my_shop = 1\nvend_setup = 0\n").vendSetupWindow);
    CHECK(noUi("vend_setup = 1\n").vendSetupWindow);
    CHECK(settings::findSetting("interface.vend_setup") != nullptr);
    CHECK(appliesNow("interface.vend_setup"));
    CHECK(liveUpdate(noUi(""), noUi("vend_setup = 1\n")).next.vendSetupWindow);
}

// 3.0.18-beta.68 (stage 6, the owner's word): the new interface library on unless the file turns it off
TEST(config_has_the_new_interface_library_on_by_default) {
    CHECK(noUi("").uaui);
    CHECK(!noUi("uaui = 0\n").uaui);
    // 3.0.18-beta.75 (stage 6): no switch in the app - uaUI draws the new interface always
    CHECK(settings::findSetting("interface.uaui") == nullptr);
}

// 3.0.18-beta.125 (the concept «Ігровий інтерфейс», item 2: «Клавіші»): [interface]
// key_hunt, key_mvp, key_tracker, key_party, key_recap - Ctrl and a letter the game leaves free, or off; rows of the
// schema, applied at once
TEST(config_reads_our_windows_keys) {
    const Config d = noUi("");
    CHECK(d.ownKeys[size_t(closedwin::Own::Hunt)] == 'H' && d.ownKeys[size_t(closedwin::Own::Recap)] == 'K');
    const Config c = noUi("key_hunt = u\nkey_recap = off\n");
    CHECK(c.ownKeys[size_t(closedwin::Own::Hunt)] == 'U' && c.ownKeys[size_t(closedwin::Own::Recap)] == 0);
    CHECK(noUi("key_hunt = a\n").ownKeys[size_t(closedwin::Own::Hunt)] == 'H');  // the game's Ctrl+A: not taken
    const settings::IniSetting* s = settings::findSetting("interface.key_hunt");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->control == settings::Control::Choice && s->tokens.size() == s->choices.size());
    CHECK_EQ(settings::tokenOf(*s, c), std::string("u"));
    CHECK(appliesNow("interface.key_party"));
    CHECK(liveUpdate(noUi(""), c).next.ownKeys[size_t(closedwin::Own::Hunt)] == 'U');
}

// 3.0.18-beta.144 (the owner: element things in their colour): [interface] element_colours - a thing with an
// element its colour (its name, a line in its cell); on at first, a box of «Інтерфейс», at once
TEST(config_interface_element_colours) {
    CHECK(noUi("").elementColours);
    CHECK(!noUi("element_colours = 0\n").elementColours);
    const settings::IniSetting* s = settings::findSetting("interface.element_colours");
    CHECK(s != nullptr && s->page == settings::Page::Interface);
    CHECK(appliesNow("interface.element_colours"));
    CHECK(!liveUpdate(noUi(""), noUi("element_colours = 0\n")).next.elementColours);
}
