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
