#include <cstring>

#include "core/config.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(config_defaults_without_a_file) {
    const Config c = parseConfig("");
    CHECK(c.backend == Backend::DgVoodoo);
    CHECK(c.mode == DisplayMode::Borderless);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(!c.allowTearing);
    CHECK(c.logLevel == LogLevel::Info);
}

TEST(config_reads_every_key) {
    const Config c = parseConfig(
        "[renderer]\nbackend = uaro\n"
        "[display]\nmode = window\nmonitor = 1\nwindow_width = 1600\nwindow_height = 900\n"
        "[frames]\nvsync = 0\nfps_limit = 144\nallow_tearing = 1\n"
        "[debug]\nlog = verbose\n");
    CHECK(c.backend == Backend::Uaro);
    CHECK(c.mode == DisplayMode::Window);
    CHECK_EQ(c.monitor, 1);
    CHECK_EQ(c.windowWidth, 1600);
    CHECK_EQ(c.windowHeight, 900);
    CHECK(!c.vsync);
    CHECK_EQ(c.fpsLimit, 144);
    CHECK(c.allowTearing);
    CHECK(c.logLevel == LogLevel::Debug);
}

TEST(config_invalid_values_keep_defaults) {
    const Config c = parseConfig(
        "[renderer]\nbackend = vulkan\n"
        "[display]\nmode = fullscreen\nmonitor = -1\nwindow_width = 100\nwindow_height = abc\n"
        "[frames]\nvsync = maybe\nfps_limit = -5\n"
        "[debug]\nlog = loud\n");
    CHECK(c.backend == Backend::DgVoodoo);
    CHECK(c.mode == DisplayMode::Borderless);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(c.logLevel == LogLevel::Info);
}

TEST(config_notepad_file_with_bom_crlf_comments_and_capitals) {
    const Config c = parseConfig("\xEF\xBB\xBF; uaRO Graphics\r\n[Renderer]\r\nBackend = UARO ; the new one\r\n");
    CHECK(c.backend == Backend::Uaro);
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
}
