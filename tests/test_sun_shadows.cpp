// 3.0.20-beta.50 (the owner's «делай 1-9», 2 - a trial of real shadows in the 3D view): the houses', the trees' and
// the ground's shadows from the map's own models and ground, kept whole on the card - a map of depths from the sun's
// side (the renderers' sun_shadows.cpp), laid over the picture by each pixel's place in it. 2.1.2-2.1.4's «Тени от
// солнца» and «Настоящие тени» were made of what the frame showed («крутились и прыгали вместе с камерой») and lay
// over the map's own painted shadows («спорили с тенями, которые игра уже нарисовала»): these are of the whole map's
// geometry, and the painted ones are taken out of the ground's light where they are drawn
#include <cmath>
#include <vector>

#include <string>

#include "core/settings/catalogue.hpp"
#include "core/config.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "core/sun_shadows.hpp"
#include "core/terrain.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

}  // namespace

TEST(sun_shadows_the_lights_own_axes_stand_square) {
    const float ways[3][3] = {{0.5f, -0.7071f, 0.5f}, {0.0f, -1.0f, 0.0f}, {-0.8f, -0.2f, 0.5657f}};
    for (const float* way : {ways[0], ways[1], ways[2]}) {
        float right[3], up[3];
        sun_shadows::axesOf(way, right, up);
        CHECK(std::fabs(dot3(right, right) - 1.0f) < 1e-4f && std::fabs(dot3(up, up) - 1.0f) < 1e-4f);
        CHECK(std::fabs(dot3(right, way)) < 1e-4f && std::fabs(dot3(up, way)) < 1e-4f);
        CHECK(std::fabs(dot3(right, up)) < 1e-4f);
        CHECK(std::fabs(right[1]) < 1e-4f);  // its right along the ground
    }
}

TEST(sun_shadows_a_place_in_the_box) {
    const float toSun[3] = {0.5f, -0.7071f, 0.5f};
    const float focus[3] = {100.0f, -20.0f, 300.0f};
    const sun_shadows::Box b = sun_shadows::boxAt(focus, toSun, 320.0f, 1500.0f, 2048);
    float at[3];
    sun_shadows::placeIn(b, b.middle, at);
    CHECK(std::fabs(at[0] - 0.5f) < 1e-5f && std::fabs(at[1] - 0.5f) < 1e-5f && std::fabs(at[2] - 0.5f) < 1e-5f);
    const float aside[3] = {b.middle[0] + b.right[0] * 320.0f, b.middle[1] + b.right[1] * 320.0f,
                            b.middle[2] + b.right[2] * 320.0f};
    sun_shadows::placeIn(b, aside, at);
    CHECK(std::fabs(at[0] - 1.0f) < 1e-4f && std::fabs(at[1] - 0.5f) < 1e-4f);
    const float above[3] = {b.middle[0] + b.up[0] * 160.0f, b.middle[1] + b.up[1] * 160.0f,
                            b.middle[2] + b.up[2] * 160.0f};
    sun_shadows::placeIn(b, above, at);
    CHECK(std::fabs(at[1] - 0.25f) < 1e-4f);  // up in the light's view: the map's rows run down
    // nearer the sun a lesser depth: a crown thirty over a spot of the ground along the way to the sun lies at the
    // spot's own place in the map, before it
    const float spot[3] = {120.0f, -20.0f, 310.0f};
    const float crown[3] = {spot[0] + toSun[0] * 30.0f, spot[1] + toSun[1] * 30.0f, spot[2] + toSun[2] * 30.0f};
    float s[3], c[3];
    sun_shadows::placeIn(b, spot, s);
    sun_shadows::placeIn(b, crown, c);
    CHECK(std::fabs(s[0] - c[0]) < 1e-4f && std::fabs(s[1] - c[1]) < 1e-4f);
    CHECK(c[2] < s[2] && std::fabs((s[2] - c[2]) * 2.0f * 1500.0f - 30.0f) < 0.01f);
}

TEST(sun_shadows_the_box_moves_by_whole_texels) {
    const float toSun[3] = {0.5f, -0.7071f, 0.5f};
    const float texel = 2.0f * 320.0f / 2048.0f;
    float focus[3] = {100.0f, -20.0f, 300.0f};
    const sun_shadows::Box a = sun_shadows::boxAt(focus, toSun, 320.0f, 1500.0f, 2048);
    focus[0] += 0.05f, focus[2] -= 0.04f;  // a step shorter than a texel: the same box (no shimmer walking)
    const sun_shadows::Box near = sun_shadows::boxAt(focus, toSun, 320.0f, 1500.0f, 2048);
    float d[3] = {near.middle[0] - a.middle[0], near.middle[1] - a.middle[1], near.middle[2] - a.middle[2]};
    const float alongRight = dot3(d, a.right) / texel, alongUp = dot3(d, a.up) / texel;
    CHECK(std::fabs(alongRight - std::round(alongRight)) < 1e-3f && std::fabs(alongUp - std::round(alongUp)) < 1e-3f);
    CHECK(std::fabs(alongRight) <= 1.001f && std::fabs(alongUp) <= 1.001f);
    focus[0] += 50.0f;  // a long way: it follows, still on the texels
    const sun_shadows::Box off = sun_shadows::boxAt(focus, toSun, 320.0f, 1500.0f, 2048);
    for (int k = 0; k < 3; ++k) d[k] = off.middle[k] - a.middle[k];
    CHECK(std::fabs(dot3(d, a.right) / texel - std::round(dot3(d, a.right) / texel)) < 1e-2f);
    CHECK(std::fabs(dot3(d, a.right)) > 20.0f);
    // the middle never further from the focus than a texel along the light's axes
    for (int k = 0; k < 3; ++k) d[k] = off.middle[k] - focus[k];
    CHECK(std::fabs(dot3(d, off.right)) <= texel && std::fabs(dot3(d, off.up)) <= texel);
}

TEST(sun_shadows_the_box_ahead_of_the_eye) {
    const float eye[3] = {10.0f, -30.0f, 20.0f}, forward[3] = {0.6f, 0.3f, 0.7416f};
    float focus[3];
    sun_shadows::focusOf(eye, forward, 320.0f, focus);
    const float flat = std::sqrt(0.6f * 0.6f + 0.7416f * 0.7416f);
    CHECK(std::fabs(focus[0] - (10.0f + 0.6f / flat * 160.0f)) < 0.01f);
    CHECK(std::fabs(focus[2] - (20.0f + 0.7416f / flat * 160.0f)) < 0.01f && focus[1] == eye[1]);
    const float down[3] = {0.0f, 1.0f, 0.0f};  // looking straight down: round the eye
    sun_shadows::focusOf(eye, down, 320.0f, focus);
    CHECK(focus[0] == eye[0] && focus[2] == eye[2]);
}

namespace {

// 2 x 1 cells: the first's top of lightmap 0 (lit, 200), the second's of lightmap 1 (in a painted shadow, 90); a
// wall of lightmap 2 (60); eight more lit tops so that most of the ground is lit
gnd::Ground shaded() {
    gnd::Ground g;
    g.width = 10, g.height = 1;
    g.zoom = 10.0f;
    g.textures = {"a.bmp"};
    const uint8_t levels[3] = {200, 90, 60};
    for (int k = 0; k < 3; ++k) {
        gnd::Lightmap l{};
        for (int i = 0; i < 64; ++i) l.brightness[i] = levels[k];
        for (int i = 0; i < 192; ++i) l.rgb[i] = uint8_t(10 + k);
        g.lightmaps.push_back(l);
    }
    for (int k = 0; k < 3; ++k) {
        gnd::Surface s;
        s.texture = 0;
        s.lightmap = uint16_t(k);
        g.surfaces.push_back(s);
    }
    for (int x = 0; x < 10; ++x) {
        gnd::Cell c{};
        c.top = x == 1 ? 1 : 0;
        c.front = x == 1 ? 2 : -1;
        c.right = -1;
        g.cells.push_back(c);
    }
    return g;
}

}  // namespace

TEST(sun_shadows_the_painted_shadows_out_of_the_grounds_light) {
    const gnd::Ground g = shaded();
    const terrain::Atlas atlas = terrain::lightmapAtlas(g);
    const terrain::Atlas sunless = sun_shadows::sunlessAtlas(g, atlas);
    CHECK(sunless.width == atlas.width && sunless.height == atlas.height);
    const auto at = [](const terrain::Atlas& a, int lightmap) {  // a texel within its eight by eight
        return a.argb[size_t(lightmap) * 8 + size_t(a.width) * 3 + 3];
    };
    CHECK((at(atlas, 1) >> 24) == 90u);
    CHECK((at(sunless, 0) >> 24) == 200u);                     // the lit ground as it was
    CHECK((at(sunless, 1) >> 24) == 200u);                     // the shadow's cell as bright as the lit ground
    CHECK((at(sunless, 1) & 0xFFFFFFu) == (at(atlas, 1) & 0xFFFFFFu));  // its coloured light kept
    CHECK((at(sunless, 2) >> 24) == 60u);                      // a wall's own light kept
    CHECK((at(atlas, 3) >> 24) < 200u && (at(sunless, 3) >> 24) == 200u);  // the mean cell after them (the land's)
    // a renderer takes it a strip of rows at a time, from what the map's read left it: which lightmaps are tops',
    // the lit level
    const sun_shadows::Sunless kept = sun_shadows::sunlessOf(g);
    CHECK(kept.lit == 200u && kept.tops.size() == 4 && kept.tops[0] && kept.tops[1] && !kept.tops[2] && kept.tops[3]);
    std::vector<uint32_t> rows(size_t(atlas.width) * 3);
    sun_shadows::sunlessRows(atlas, kept, 2, 3, rows.data());
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 32; ++x)
            CHECK(rows[size_t(y) * size_t(atlas.width) + size_t(x)] ==
                  sunless.argb[size_t(y + 2) * size_t(atlas.width) + size_t(x)]);
    // how dark the map paints its shadows: the darkest tenth of its ground's light over its lit ground's
    gnd::Ground many = shaded();
    many.cells[2].top = 1;  // two cells of ten in the shadow
    const float shade = sun_shadows::shadeOf(many);
    CHECK(std::fabs(shade - 90.0f / 200.0f) < 0.02f);
    CHECK(sun_shadows::shadeOf(gnd::Ground{}) == sun_shadows::kShadeUsual);  // nothing known
    gnd::Ground flat = shaded();
    flat.cells[1].top = 0;  // no shadow painted at all: the usual one
    CHECK(std::fabs(sun_shadows::shadeOf(flat) - sun_shadows::kShadeUsual) < 1e-4f);
}

// its switch: [view3d] shadows - a trial, off at first (the owner 2026-10-15: «тени тоже мне кажется лишнее» -
// 2.1.4 took the old ones out); live. 3.0.21-beta.2: a trial of the catalogue - a row of the console's «Проби»
TEST(config_view3d_shadows_a_trial_off_by_default) {
    CHECK(!parseConfig("").effects.sunShadows3d);
    CHECK(parseConfig("[view3d]\nshadows = 1\n").effects.sunShadows3d);
    CHECK(!parseConfig("[effects]\nsun_shadows = 1\nreal_shadows = 1\n").effects.sunShadows3d);  // the old lines: none
    const settings::IniSetting* row = settings::findSetting("view3d.shadows");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
    CHECK(appliesNow("view3d.shadows"));
    const settings::catalogue::Row* trial = settings::catalogue::find("view3d.shadows");
    CHECK(trial && trial->status == settings::catalogue::Status::Trial);
}
