#include "core/live_config.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(live_config_applies_what_can_change_in_a_running_game) {
    const Config running = parseConfig("[quality]\nsharpen = 50\n");
    const LiveUpdate u = liveUpdate(
        running, parseConfig("[quality]\nsharpen = 80\nrender_scale = 150\n[display]\nui_filter = smooth\n"
                             "crisp_text = 0\nhigh_resolution = 0\n[frames]\nvsync = 0\nfps_limit = 60\n"
                             "[debug]\nlog = verbose\n"));
    CHECK_EQ(u.next.sharpen, 80);
    CHECK_EQ(u.next.renderScale, 150);
    CHECK(u.next.uiFilter == UiFilter::Smooth);
    CHECK(!u.next.crispText);
    CHECK(!u.next.highResolution);
    CHECK(!u.next.vsync);
    CHECK_EQ(u.next.fpsLimit, 60);
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
        running, parseConfig("[renderer]\nbackend = uaro\n[display]\nmode = window\nmonitor = 1\n"
                             "window_width = 1600\nwindow_height = 900\n[frames]\nallow_tearing = 1\n"));
    CHECK(u.applied.empty());
    CHECK(u.next.backend == running.backend);
    CHECK(u.next.mode == running.mode);
    CHECK_EQ(u.next.monitor, 0);
    CHECK_EQ(u.next.windowWidth, 0);
    CHECK(!u.next.allowTearing);
    CHECK_EQ(u.waiting.size(), size_t(6));
    CHECK(u.waiting[0] == "backend");
    CHECK(u.waiting[1] == "mode");
    CHECK(u.waiting[5] == "allow_tearing");
}

TEST(live_config_an_unchanged_file_changes_nothing) {
    const Config c = parseConfig("[quality]\nsharpen = 70\n[display]\nmode = window\n");
    const LiveUpdate u = liveUpdate(c, c);
    CHECK(u.applied.empty());
    CHECK(u.waiting.empty());
    CHECK_EQ(u.next.sharpen, 70);
}

TEST(live_config_names_the_keys_that_apply_now) {
    for (const char* k : {"display.high_resolution", "display.ui_filter", "display.crisp_text", "quality.render_scale",
                          "quality.sharpen", "frames.vsync", "frames.fps_limit", "debug.log"})
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
    CHECK(u.applied[0] == "ao 0 -> 1");
    CHECK(u.applied[1] == "glow_light 50 -> 20");
    CHECK(u.applied[2] == "ao_size 50 -> 70");
    for (const char* k : {"effects.ao", "effects.ao_strength", "effects.ao_size", "effects.glow_light"})
        CHECK(appliesNow(k));
}

TEST(live_config_colour_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\ngrade = 1\ngrade_warmth = 70\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "grade 0 -> 1");
    CHECK(u.applied[1] == "grade_warmth 50 -> 70");
    CHECK(appliesNow("effects.grade_vignette"));
}

TEST(live_config_depth_of_field_applies_now) {
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[effects]\ndof = 1\ndof_range = 80\n"));
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(2));
    CHECK(u.applied[0] == "dof 0 -> 1");
    CHECK(u.applied[1] == "dof_range 40 -> 80");
    CHECK(appliesNow("effects.dof_strength"));
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

