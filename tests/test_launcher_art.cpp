// 3.0.18-beta.193 (docs/superpowers/plans/2026-11-12-launcher.md Task 4): the launcher's art - uaRO HD's shots of the
// fountain in the package, read, laid over the window as a cover, the next one faded in
#include <cstdint>
#include <string>
#include <vector>

#include "core/launcher_art.hpp"
#include "core/webp_image.hpp"
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

// 3.0.18-beta.203 (the owner: «лого на картинке с заставки самого клиента взять»): the logo - a WebP with its alpha
// (cut out of the client's login picture), made the logo box's size and laid over the art
TEST(launcher_art_logo_keeps_its_alpha) {
    CHECK_EQ(launcherart::decodeLogo("").w, 0);
    CHECK_EQ(launcherart::decodeLogo("not a picture").w, 0);
    const uint32_t px[2] = {0xFFFF0000u, 0x40FFFFFFu};  // opaque red, a quarter white
    const std::vector<uint8_t> bytes = webp::encode(px, 2, 1, 100);
    const launcherart::Image logo = launcherart::decodeLogo(std::string(bytes.begin(), bytes.end()));
    CHECK_EQ(logo.w, 2);
    CHECK_EQ(logo.h, 1);
    CHECK_EQ(logo.argb.size(), size_t(2));
    CHECK_EQ(logo.argb[0], 0xFFFF0000u);
    CHECK_EQ(logo.argb[1] >> 24, 0x40u);
}

TEST(launcher_art_logo_fitted_premultiplied) {
    launcherart::Image src;  // 4 x 2: the left half white over nothing (a green colour under its clear pixels), red
    src.w = 4;
    src.h = 2;
    src.argb = {0xFFFFFFFFu, 0x0000FF00u, 0xFFFF0000u, 0xFFFF0000u,  //
                0x0000FF00u, 0xFFFFFFFFu, 0xFFFF0000u, 0xFFFF0000u};
    std::vector<uint32_t> out;
    launcherart::fitted(src, 2, 1, out);  // smaller: the mean of what each covers, the colours times alpha
    CHECK_EQ(out.size(), size_t(2));
    const uint32_t a = out[0] >> 24, r = (out[0] >> 16) & 255u, g = (out[0] >> 8) & 255u, b = out[0] & 255u;
    CHECK(a == 0x7Fu || a == 0x80u);
    CHECK_EQ(r, a);  // half white, premultiplied: no green from under the clear pixels
    CHECK_EQ(g, a);
    CHECK_EQ(b, a);
    CHECK_EQ(out[1], 0xFFFF0000u);
    launcherart::fitted(src, 4, 2, out);  // its own size: the same, premultiplied
    CHECK_EQ(out[0], 0xFFFFFFFFu);
    CHECK_EQ(out[1], 0u);
    launcherart::Image ramp;  // bigger: smooth between its pixels
    ramp.w = 2;
    ramp.h = 1;
    ramp.argb = {0xFF000000u, 0xFFFFFFFFu};
    launcherart::fitted(ramp, 4, 1, out);
    CHECK_EQ(out.size(), size_t(4));
    for (int i = 0; i < 4; ++i) CHECK_EQ(out[size_t(i)] >> 24, 0xFFu);
    for (int i = 1; i < 4; ++i) CHECK((out[size_t(i)] & 255u) >= (out[size_t(i - 1)] & 255u));
    CHECK((out[0] & 255u) < 0x40u);
    CHECK((out[3] & 255u) > 0xC0u);
    launcherart::fitted(launcherart::Image{}, 3, 3, out);  // no logo: nothing to lay
    CHECK_EQ(out.size(), size_t(9));
    CHECK_EQ(out[4], 0u);
}

TEST(launcher_art_logo_over_the_art) {
    std::vector<uint32_t> dst(3, 0xFF0000FFu);                 // blue, 3 x 1
    const std::vector<uint32_t> logo{0xFFFF0000u, 0x80800000u};  // red, half red (premultiplied)
    launcherart::over(dst, 3, 1, logo, 2, 1, 1, 0);
    CHECK_EQ(dst[0], 0xFF0000FFu);
    CHECK_EQ(dst[1], 0xFFFF0000u);
    CHECK_EQ(dst[2] >> 24, 0xFFu);
    CHECK_EQ((dst[2] >> 16) & 255u, 0x80u);
    const uint32_t blue = dst[2] & 255u;
    CHECK(blue == 0x7Fu || blue == 0x80u);
    std::vector<uint32_t> edge(3, 0xFF0000FFu);
    launcherart::over(edge, 3, 1, logo, 2, 1, -1, 0);  // cut at the left: its second pixel on the first
    CHECK_EQ((edge[0] >> 16) & 255u, 0x80u);
    CHECK_EQ(edge[1], 0xFF0000FFu);
    launcherart::over(edge, 3, 1, logo, 2, 1, 2, 0);  // cut at the right: its first pixel on the last
    CHECK_EQ(edge[2], 0xFFFF0000u);
    launcherart::over(edge, 3, 1, logo, 2, 1, 5, 7);  // all outside: nothing
    CHECK_EQ(edge[1], 0xFF0000FFu);
}
