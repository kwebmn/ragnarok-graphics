#include "core/daylight.hpp"
#include "core/live_config.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(live_config_applies_what_can_change_in_a_running_game) {
    const Config running = parseConfig("[quality]\nsharpen = 50\n");
    const LiveUpdate u = liveUpdate(
        running, parseConfig("[quality]\nsharpen = 80\nrender_scale = 150\n[display]\nui_filter = smooth\n"
                             "crisp_text = 0\nhigh_resolution = 0\n[frames]\nvsync = 0\nfps_limit = 45\n"
                             "[debug]\nlog = verbose\n"));
    CHECK_EQ(u.next.sharpen, 80);
    CHECK_EQ(u.next.renderScale, 150);
    CHECK(u.next.uiFilter == UiFilter::Smooth);
    CHECK(!u.next.crispText);
    CHECK(!u.next.highResolution);
    CHECK(!u.next.vsync);
    CHECK_EQ(u.next.fpsLimit, 45);
    CHECK(u.next.logLevel == LogLevel::Debug);
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(8));
    CHECK(u.applied[0] == "high_resolution 1 -> 0");
    CHECK(u.applied[1] == "ui_filter sharp -> smooth");
    CHECK(u.applied[3] == "render_scale 100 -> 150");
    CHECK(u.applied[4] == "sharpen 50 -> 80");
    CHECK(u.applied[7] == "log normal -> verbose");
}

TEST(live_config_keeps_start_up_keys_until_the_next_start) {
    const Config running = parseConfig("");
    const LiveUpdate u = liveUpdate(
        running, parseConfig("[display]\nmode = window\nmonitor = 1\n"
                             "window_width = 1600\nwindow_height = 900\n[frames]\nallow_tearing = 1\n"));
    CHECK(u.applied.empty());
    CHECK(u.next.mode == running.mode);
    CHECK_EQ(u.next.monitor, 0);
    CHECK_EQ(u.next.windowWidth, 0);
    CHECK(!u.next.allowTearing);
    CHECK_EQ(u.waiting.size(), size_t(5));
    CHECK(u.waiting[0] == "mode");
    CHECK(u.waiting[4] == "allow_tearing");
}

TEST(live_config_an_unchanged_file_changes_nothing) {
    const Config c = parseConfig("[quality]\nsharpen = 70\n[display]\nmode = window\n");
    const LiveUpdate u = liveUpdate(c, c);
    CHECK(u.applied.empty());
    CHECK(u.waiting.empty());
    CHECK_EQ(u.next.sharpen, 70);
}

TEST(live_config_window_art_applies_at_once) {
    // 0.41: switched in a running game, to compare - the windows are redrawn with it.
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[display]\nwindow_art = ai\n"));
    CHECK(u.next.windowArt == WindowArt::Ai);
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(1));
    CHECK(u.applied[0] == "window_art xbr -> ai");
}

TEST(live_config_names_the_keys_that_apply_now) {
    for (const char* k : {"display.high_resolution", "display.ui_filter", "display.crisp_text", "quality.render_scale",
                          "quality.sharpen", "frames.vsync", "frames.fps_limit", "debug.log", "display.window_art"})
        CHECK(appliesNow(k));
    for (const char* k : {"renderer.backend", "display.mode", "display.monitor", "display.window_width",
                          "display.window_height", "frames.allow_tearing", "utility.language", "nothing"})
        CHECK(!appliesNow(k));
}

TEST(live_config_ai_waits_for_the_next_start) {
    // [ai]: the networks and their models start with the 64-bit renderer.
    const Config running = parseConfig("");
    const LiveUpdate u =
        liveUpdate(running, parseConfig("[ai]\nenabled = 1\nmonsters = 1\nmodel = anime\nscale = 2\nbudget = 4\n"));
    CHECK(u.applied.empty());
    CHECK(u.next.ai == running.ai);
    CHECK_EQ(u.waiting.size(), size_t(5));
    CHECK(u.waiting[0] == "ai.enabled");
    CHECK(u.waiting[1] == "ai.monsters");
    CHECK(u.waiting[2] == "ai.model");
    CHECK(u.waiting[4] == "ai.budget");
}

TEST(live_config_effects_apply_now) {
    // 0.18: [effects] apply in a running game, the driver's log naming each change.
    const Config running = parseConfig("");
    const LiveUpdate u =
        liveUpdate(running, parseConfig("[effects]\nglow = 1\nglow_strength = 140\nglow_size = 30\n"));
    CHECK(u.waiting.empty());
    CHECK(u.next.effects.glow);
    CHECK_EQ(u.next.effects.glowStrength, 140);
    CHECK_EQ(u.next.effects.glowSize, 30);
    CHECK_EQ(u.applied.size(), size_t(3));
    CHECK(u.applied[0] == "glow 0 -> 1");
    CHECK(u.applied[1] == "glow_strength 100 -> 140");
    CHECK(u.applied[2] == "glow_size 50 -> 30");
    for (const char* k : {"effects.glow", "effects.glow_strength", "effects.glow_size"}) CHECK(appliesNow(k));
}

TEST(live_config_water_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nwater = 1\nwater_clarity = 80\n"));
    CHECK(u.waiting.empty());
    CHECK(u.next.effects.water);
    CHECK_EQ(u.next.effects.waterClarity, 80);
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "water 0 -> 1");
    CHECK(u.applied[1] == "water_clarity 50 -> 80");
    for (const char* k : {"effects.water", "effects.water_waves", "effects.water_reflections", "effects.water_clarity"})
        CHECK(appliesNow(k));
}

TEST(live_config_shadows_apply_now) {
    const LiveUpdate u =
        liveUpdate(parseConfig(""), parseConfig("[effects]\nao = 1\nao_size = 70\nglow_light = 20\n"));
    CHECK(u.waiting.empty());
    CHECK(u.next.effects.ao);
    CHECK_EQ(u.applied.size(), size_t(3));
    CHECK(u.applied[0] == "ao 0 -> 1");  // in the settings' order (1.9.6)
    CHECK(u.applied[1] == "ao_size 50 -> 70");
    CHECK(u.applied[2] == "glow_light 50 -> 20");
    for (const char* k : {"effects.ao", "effects.ao_strength", "effects.ao_size", "effects.glow_light"})
        CHECK(appliesNow(k));
}

TEST(live_config_a_newer_effect_alone_applies_now) {  // 1.9.6: the console switched grass, nothing happened
    const LiveUpdate u = liveUpdate(parseConfig("[effects]\ngrass = 1\n"), parseConfig("[effects]\ngrass = 0\n"));
    CHECK(!u.next.effects.grass);
    CHECK_EQ(u.applied.size(), size_t(1));
    CHECK(u.applied[0] == "grass 1 -> 0");
}

TEST(live_config_colour_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\ngrade = 1\ngrade_warmth = 70\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "grade 0 -> 1");
    CHECK(u.applied[1] == "grade_warmth 50 -> 70");
    CHECK(appliesNow("effects.grade_vignette"));
}

TEST(live_config_torches_apply_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\ntorch = 0\ntorch_light = 150\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "torch 1 -> 0");
    CHECK(u.applied[1] == "torch_light 100 -> 150");
    CHECK(appliesNow("effects.torch_glow"));
    const LiveUpdate v =
        liveUpdate(parseConfig(""), parseConfig("[effects]\ntorch_flame = 0\ntorch_brightness = 70\n"));
    CHECK_EQ(v.applied.size(), size_t(2));
    CHECK(v.applied[0] == "torch_flame 1 -> 0");
    CHECK(v.applied[1] == "torch_brightness 100 -> 70");
}

TEST(live_config_fire_light_applies_now) {
    const LiveUpdate u =
        liveUpdate(parseConfig(""), parseConfig("[effects]\nfire_light = 0\nfire_light_strength = 150\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "fire_light 1 -> 0");
    CHECK(u.applied[1] == "fire_light_strength 100 -> 150");
}

TEST(live_config_aura_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\naura = 0\naura_brightness = 20\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "aura 1 -> 0");
    CHECK(u.applied[1] == "aura_brightness 50 -> 20");
}

TEST(live_config_fireflies_apply_now) {
    const LiveUpdate u =
        liveUpdate(parseConfig(""), parseConfig("[effects]\nfireflies = 0\nfireflies_brightness = 60\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "fireflies 1 -> 0");
    CHECK(u.applied[1] == "fireflies_brightness 100 -> 60");
}

TEST(live_config_map_glows_apply_now) {
    const LiveUpdate u =
        liveUpdate(parseConfig(""), parseConfig("[effects]\nmap_glows = 0\nmap_glows_brightness = 150\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "map_glows 1 -> 0");
    CHECK(u.applied[1] == "map_glows_brightness 100 -> 150");
}

TEST(live_config_rays_apply_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nrays = 0\nrays_brightness = 50\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "rays 1 -> 0");
    CHECK(u.applied[1] == "rays_brightness 100 -> 50");
}

TEST(live_config_lava_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nlava = 0\nlava_brightness = 120\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "lava 1 -> 0");
    CHECK(u.applied[1] == "lava_brightness 100 -> 120");
}

TEST(live_config_actor_shadows_apply_now) {
    const LiveUpdate u =
        liveUpdate(parseConfig(""), parseConfig("[effects]\nactor_shadows = 0\nactor_shadows_strength = 30\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "actor_shadows 1 -> 0");
    CHECK(u.applied[1] == "actor_shadows_strength 55 -> 30");
}

TEST(live_config_heat_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nheat = 0\nheat_strength = 40\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "heat 1 -> 0");
    CHECK(u.applied[1] == "heat_strength 100 -> 40");
}

TEST(live_config_skill_colours_apply_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nskill_colours = 0\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(1));
    CHECK(u.applied[0] == "skill_colours 1 -> 0");
}

TEST(live_config_skills_soft_apply_now) {
    const LiveUpdate u =
        liveUpdate(parseConfig(""), parseConfig("[effects]\nskills_soft = 0\nskill_brightness = 60\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "skills_soft 1 -> 0");
    CHECK(u.applied[1] == "skill_brightness 80 -> 60");
}

TEST(live_config_portals_apply_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nportals = 0\nportals_strength = 70\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "portals 1 -> 0");
    CHECK(u.applied[1] == "portals_strength 100 -> 70");
}

TEST(live_config_weather_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nweather = 0\nweather_amount = 150\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "weather 1 -> 0");
    CHECK(u.applied[1] == "weather_amount 100 -> 150");
}

TEST(live_config_lamps_apply_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nlamps = 0\nlamps_light = 100\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "lamps 1 -> 0");
    CHECK(u.applied[1] == "lamps_light 60 -> 100");
}

TEST(live_config_chimney_smoke_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nsmoke = 0\nsmoke_density = 150\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "smoke 1 -> 0");
    CHECK(u.applied[1] == "smoke_density 100 -> 150");
}

TEST(live_config_smooth_map_light_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nsmooth_light = 0\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(1));
    CHECK(u.applied[0] == "smooth_light 1 -> 0");
}


TEST(live_config_ai_sprite_kinds_hide_each_on_its_own) {
    // 2.2.2: monsters, players, NPCs or items turned off in a running game are hidden at once, each by the cells of
    // the sprite cache that hold it (2.2.1 waited for the next start unless all four were off: one copy holds them)
    const char* started = "[ai]\nenabled = 1\nmonsters = 1\nplayers = 1\nnpcs = 1\nitems = 1\n";
    const Config running = parseConfig(started);
    const char* noMonsters = "[ai]\nenabled = 1\nmonsters = 0\nplayers = 1\nnpcs = 1\nitems = 1\n";
    LiveUpdate u = liveUpdate(running, parseConfig(noMonsters));
    CHECK(u.waiting.empty());
    CHECK(u.applied.size() == 1 && u.applied[0] == "ai.monsters 1 -> 0");
    CHECK(!u.next.aiNow.monsters && u.next.ai.monsters);  // hidden now, its pictures kept
    u = liveUpdate(u.next, parseConfig(started));
    CHECK(u.waiting.empty() && u.applied.size() == 1 && u.applied[0] == "ai.monsters 0 -> 1");
    // started without the players' pictures: on waits for the next start, as any kind's
    const Config without = parseConfig("[ai]\nenabled = 1\nmonsters = 1\nplayers = 0\n");
    u = liveUpdate(without, parseConfig("[ai]\nenabled = 1\nmonsters = 1\nplayers = 1\n"));
    CHECK(u.applied.empty() && u.waiting.size() == 1 && u.waiting[0] == "ai.players");
}

TEST(live_config_ai_sprite_cells_follow_their_kinds) {
    // 2.2.2: a cell of the sprite cache is drawn from its AI copy while its own kind is shown - monsters, players,
    // NPCs and items by theirs (aiSpritesShown, bits by sprite_exe::Source), an effect's frame by the effects', the
    // cursor by the icons' (aiKindsShown), a frame of no known file while any sprite kind is
    AiSettings a;
    a.enabled = true;
    a.monsters = a.npcs = true;
    CHECK_EQ(int(aiSpritesShown(a)), (1 << 1) | (1 << 3));
    a.enabled = false;
    CHECK_EQ(int(aiSpritesShown(a)), 0);
    const uint8_t kinds = 1 | 2 | 4 | 16;  // ground, objects, sprites, icons - not the effects
    const uint8_t npcs = 1 << 3;
    CHECK(!aiCellShown(1, kinds, npcs));  // a monster's frame: the game's own
    CHECK(aiCellShown(3, kinds, npcs));   // an NPC's: the AI's
    CHECK(!aiCellShown(5, kinds, npcs));  // an effect's frame
    CHECK(aiCellShown(6, kinds, npcs));   // the cursor
    CHECK(aiCellShown(0, kinds, npcs));   // no known file
    CHECK(!aiCellShown(0, kinds & ~4, 0));
}

TEST(live_config_takes_the_xray_at_once) {
    // 2.3: uaRO Studio's X-ray from the console or the settings app shows at once
    Config running, file;
    file.xray = true;
    const LiveUpdate u = liveUpdate(running, file);
    CHECK(u.next.xray);
}

TEST(live_config_the_chosen_clock_applies_now) {
    // 2.6.11: the time of day picked in the console or the settings app shows at once
    CHECK(appliesNow("effects.clock"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\nclock = night\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(1));
    CHECK(u.applied.size() == 1 && u.applied[0] == "clock auto -> night");
    CHECK_EQ(u.next.effects.clock, int(daylight::kClockNight));
}
