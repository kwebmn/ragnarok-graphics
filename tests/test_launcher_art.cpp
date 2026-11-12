// 3.0.18-beta.193 (docs/superpowers/plans/2026-11-12-launcher.md Task 4): the launcher's art - uaRO HD's shots of the
// fountain in the package, read, laid over the window as a cover, the next one faded in
#include <cstdint>
#include <string>
#include <vector>

#include "core/launcher_art.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(launcher_art_missing_file_is_empty) {
    CHECK_EQ(launcherart::decode("").w, 0);
    CHECK_EQ(launcherart::decode("not a picture").w, 0);
    std::vector<uint32_t> out;
    launcherart::cover(launcherart::Image{}, 4, 4, out);  // nothing to lay: the plain ground
    CHECK_EQ(out.size(), size_t(16));
    CHECK_EQ(out[0], launcherart::kGround);
}

TEST(launcher_art_cover_and_blend) {
    launcherart::Image redBlue;
    redBlue.w = 2;
    redBlue.h = 1;
    redBlue.argb = {0xFFFF0000u, 0xFF0000FFu};
    std::vector<uint32_t> out;
    launcherart::cover(redBlue, 4, 4, out);  // twice as tall as wide asked: cut at the sides, the middle kept
    CHECK_EQ(out.size(), size_t(16));
    CHECK_EQ(out[0], 0xFFFF0000u);
    CHECK_EQ(out[1], 0xFFFF0000u);
    CHECK_EQ(out[2], 0xFF0000FFu);
    CHECK_EQ(out[15], 0xFF0000FFu);
    std::vector<uint32_t> black(4, 0xFF000000u);
    const std::vector<uint32_t> white(4, 0xFFFFFFFFu);
    launcherart::blend(black, white, 0.5f);
    const uint32_t g = black[0] & 0xFFu;
    CHECK(g == 0x7Fu || g == 0x80u);
    CHECK_EQ(black[0] >> 24, 0xFFu);
    launcherart::blend(black, white, 1.0f);
    CHECK_EQ(black[3], 0xFFFFFFFFu);
}
