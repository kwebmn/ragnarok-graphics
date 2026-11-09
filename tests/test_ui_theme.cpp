#include <string>

#include "core/ui_theme.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
float lum(uint32_t c) {
    return (0.2126f * float((c >> 16) & 0xFF) + 0.7152f * float((c >> 8) & 0xFF) + 0.0722f * float(c & 0xFF)) / 255;
}
}  // namespace

// 3.0.18-beta.13 (the concept's themes; docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 6): the parts'
// colours and corners by the theme chosen
TEST(ui_theme_standard_keeps_everything) {
    const uitheme::Theme t = uitheme::named("standard");
    CHECK(uitheme::colour(t, 0x10141C) == 0x10141Cu && uitheme::colour(t, 0x5BD36C) == 0x5BD36Cu);
    CHECK(t.radius == 1.0f && t.edge == 1.0f);
}

TEST(ui_theme_classic_warms_the_greys_and_keeps_the_colours) {
    const uitheme::Theme t = uitheme::named("classic");
    const uint32_t ground = uitheme::colour(t, 0x10141C);  // the cards' ground: brown now, as dark
    CHECK(((ground >> 16) & 0xFF) > (ground & 0xFF));
    CHECK(std::abs(lum(ground) - lum(0x10141C)) < 0.04f);
    const uint32_t text = uitheme::colour(t, 0xECEFF5);  // the words: cream
    CHECK(((text >> 16) & 0xFF) >= (text & 0xFF) && lum(text) > 0.8f);
    CHECK(uitheme::colour(t, 0x5BD36C) == 0x5BD36Cu);  // HP's green stays HP's
    CHECK(t.radius < 0.5f);
}

TEST(ui_theme_contrast_spreads_the_greys) {
    const uitheme::Theme t = uitheme::named("contrast");
    CHECK(lum(uitheme::colour(t, 0x10141C)) < lum(0x10141C));
    CHECK(lum(uitheme::colour(t, 0x969EAE)) > lum(0x969EAE));
    CHECK(t.edge > 1.0f);
}

TEST(ui_theme_reads_a_palette_over_the_theme) {
    uitheme::Theme t = uitheme::named("standard");
    std::string bad;
    uitheme::read("# my colours\n#10141C = #202020\nradius = 0.5\nedge = 2\nnonsense\n", t, bad);
    CHECK(uitheme::colour(t, 0x10141C) == 0x202020u);
    CHECK(t.radius == 0.5f && t.edge == 2.0f);
    CHECK_EQ(bad, std::string("nonsense"));
}
