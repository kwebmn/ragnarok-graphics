#include <cmath>
#include <set>
#include <string>

#include "core/horizon.hpp"
#include "core/horizon_terrain.hpp"
#include "test.hpp"

using namespace uagfx;
using horizon::Land;

// 3.0.22-beta.11 (the owner 19.11: «я хочу панорамы горизонта в клиенте красивые», «я хочу чтобы ты нарисовал это на
// видеокарте, не генерировал с помощью ИИ готовое»): the 3D view's horizon drawn by the card - a climate per region,
// the 360° strip's geometry, what a bake depends on and how it reaches the shader

TEST(horizon_climates_by_region) {
    CHECK(horizon::climateOf("Prontera").land == Land::Hills);
    CHECK(horizon::climateOf("Lutie").land == Land::Alps);
    CHECK(horizon::climateOf("Morroc").land == Land::Desert);
    CHECK(horizon::climateOf("Alberta").land == Land::Sea);
    CHECK(horizon::climateOf("Comodo").land == Land::Sea);
    CHECK(horizon::climateOf("Louyang").land == Land::Karst);
    CHECK(horizon::climateOf("Veins").land == Land::Canyon);
    CHECK(horizon::climateOf("Rachel").land == Land::Canyon);
    CHECK(horizon::climateOf("Niflheim").land == Land::Waste);
    CHECK(horizon::climateOf("Glast Heim").land == Land::Waste);
    const horizon::Climate a = horizon::climateOf(""), b = horizon::climateOf("Prontera");
    CHECK(a.land == b.land && a.seed == b.seed && a.mountAmp == b.mountAmp && a.grass.g == b.grass.g);
    const char* const regions[] = {"Alberta", "Amatsu",  "Ayothaya", "Comodo",   "Einbroch", "Geffen",   "Glast Heim",
                                   "Hugel",   "Juno",    "Kunlun",   "Lighthalzen", "Louyang", "Lutie",  "Morroc",
                                   "Niflheim", "Payon",  "Prontera", "Rachel",   "Umbala",   "Veins"};
    std::set<uint32_t> seeds;
    for (const char* r : regions) seeds.insert(horizon::climateOf(r).seed);
    CHECK_EQ(seeds.size(), size_t(20));
}

// 3.0.22-beta.13 (R-VDN9YH «очень размыто»: 4096 round the circle, a texel some 5 pixels wide on the owner's 4K
// screen): the strip by the card (effect_groups::cardLevel) - 8192 x 768 from 8 GB on Direct3D 12, 4096 x 384 from
// 4 GB, 2048 x 192 else; whole rows a frame, a few - the bake a second or two, a frame's share of it small
TEST(horizon_strip_geometry) {
    const horizon::Level sharp = horizon::levelForCard(3), full = horizon::levelForCard(2),
                         weak = horizon::levelForCard(1);
    CHECK(sharp.width == 8192 && sharp.height == 768 && sharp.steps == 128 && sharp.shadowSteps == 24);
    CHECK(sharp.slices == 96);
    CHECK(full.width == 4096 && full.height == 384 && full.steps == 128 && full.shadowSteps == 24 && full.slices == 64);
    CHECK(weak.width == 2048 && weak.height == 192 && weak.steps == 64 && weak.shadowSteps == 0 && weak.slices == 24);
    CHECK(horizon::levelForCard(0).width == 2048 && horizon::levelForCard(4).width == 8192);
    for (const horizon::Level& l : {sharp, full, weak}) {
        CHECK(l.height % l.slices == 0);
        CHECK(l.width * (l.height / l.slices) <= 65536);  // texels a frame
    }
    const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-5f; };
    CHECK(near(horizon::uOf(1, 0), 0.0f));
    CHECK(near(horizon::uOf(0, 1), 0.25f));
    CHECK(near(horizon::uOf(-1, 0), 0.5f));
    CHECK(near(horizon::uOf(0, -1), 0.75f));
    CHECK(near(horizon::rowOfRise(horizon::kRiseHigh), 0.0f));
    CHECK(near(horizon::rowOfRise(horizon::kRiseLow), 1.0f));
    CHECK(near(horizon::riseOfRow(0.5f), 0.15f));
}

TEST(horizon_wanted) {
    CHECK(horizon::wanted(true, true, true, false));
    CHECK(!horizon::wanted(false, true, true, false));
    CHECK(!horizon::wanted(true, false, true, false));
    CHECK(!horizon::wanted(true, true, false, false));
    CHECK(!horizon::wanted(true, true, true, true));
}

TEST(horizon_key_and_pack) {
    horizon::Key a{"Prontera", {0.5f, -0.7f, 0.5f}, false, 1200.0f};
    horizon::Key b = a;
    b.sun[1] += 0.0005f;
    CHECK(a == b);
    b.sun[1] += 0.01f;
    CHECK(!(a == b));
    horizon::Key c = a;
    c.region = "Payon";
    CHECK(!(a == c));
    horizon::Key d = a;
    d.weak = true;
    CHECK(!(a == d));
    float out[horizon::kPackedFloats] = {};
    const horizon::Climate lutie = horizon::climateOf("Lutie");
    const horizon::Level l = horizon::levelForCard(2);
    horizon::pack(lutie, 1200.0f, a.sun, l, 48, 96, out);
    CHECK(out[0] == float(int(Land::Alps)));
    CHECK(out[1] == float(lutie.seed));
    CHECK(out[2] == 1200.0f);
    CHECK(out[18] == 128.0f && out[19] == 24.0f);
    CHECK(out[28] == 0.5f && out[29] == -0.7f && out[30] == 0.5f);
    CHECK(out[32] == 4096.0f && out[33] == 384.0f);
    CHECK(out[36] == 48.0f && out[37] == 96.0f);
    CHECK(out[38] == lutie.sand.r && out[40] == lutie.sand.b);
}

// The CPU twin of the bake shader's land (core/horizon_terrain; shaders.cpp hz... the same math): its shape, its
// materials and the ray's march - the tests and the preview tool see what the card draws

namespace {

const char* const kRegions[] = {"Alberta", "Amatsu",  "Ayothaya", "Comodo",      "Einbroch", "Geffen", "Glast Heim",
                                "Hugel",   "Juno",    "Kunlun",   "Lighthalzen", "Louyang",  "Lutie",  "Morroc",
                                "Niflheim", "Payon",  "Prontera", "Rachel",      "Umbala",   "Veins"};

float meanOnCircle(const horizon::Climate& c, float radius) {
    float sum = 0.0f;
    for (int k = 0; k < 64; ++k) {
        const float a = 6.2831853f * float(k) / 64.0f;
        sum += horizon::heightAt(c, 1500.0f, radius * std::cos(a), radius * std::sin(a));
    }
    return sum / 64.0f;
}

bool within(const horizon::Rgb& a, const horizon::Rgb& b, float d) {
    return std::fabs(a.r - b.r) <= d && std::fabs(a.g - b.g) <= d && std::fabs(a.b - b.b) <= d;
}

}  // namespace

TEST(horizon_terrain_shape) {
    const horizon::Climate hills = horizon::climateOf("Prontera"), alps = horizon::climateOf("Lutie");
    CHECK(horizon::heightAt(hills, 1500.0f, 100.0f, 200.0f) == 0.0f);  // the map itself
    CHECK(horizon::heightAt(hills, 1500.0f, 5000.0f, -2500.0f) == horizon::heightAt(hills, 1500.0f, 5000.0f, -2500.0f));
    CHECK(meanOnCircle(hills, 10000.0f) > meanOnCircle(hills, 3500.0f));
    CHECK(meanOnCircle(alps, 10000.0f) > meanOnCircle(alps, 3500.0f));
    const horizon::Climate sea = horizon::climateOf("Alberta"), desert = horizon::climateOf("Morroc");
    bool below = false, under = true;
    for (int k = 0; k < 256; ++k) {
        const float a = 6.2831853f * float(k) / 256.0f;
        below = below || horizon::heightAt(sea, 1500.0f, 8000.0f * std::cos(a), 8000.0f * std::sin(a)) < 0.0f;
        under = under && horizon::heightAt(desert, 1500.0f, 8000.0f * std::cos(a), 8000.0f * std::sin(a)) <
                             desert.mountAmp + desert.hillAmp;
    }
    CHECK(below);
    CHECK(under);
}

TEST(horizon_terrain_materials) {
    const horizon::Climate alps = horizon::climateOf("Lutie");
    CHECK(within(horizon::albedoAt(alps, 7000.0f, 300.0f, alps.snowLine + 400.0f, 0.9f), alps.snow, 0.1f));
    CHECK(within(horizon::albedoAt(alps, 7000.0f, 300.0f, alps.snowLine + 400.0f, 0.4f), alps.rock, 0.15f));
    const horizon::Climate sea = horizon::climateOf("Alberta");
    const horizon::Rgb water = horizon::albedoAt(sea, 7000.0f, 300.0f, -30.0f, 1.0f);
    CHECK(water.b > water.r);  // the sea is blue
}

TEST(horizon_terrain_march) {
    const horizon::Climate hills = horizon::climateOf("Prontera");
    const float pixel = 6.2831853f / 4096.0f;
    const horizon::Hit ground = horizon::march(hills, 1500.0f, horizon::kEyeHeight, 1.0f, -0.02f, 0.0f, 128, pixel);
    CHECK(ground.hit && ground.coverage == 1.0f && ground.distance > 1500.0f);
    // the strip's top row is sky in every climate all round: no land cut flat by it
    for (const char* region : kRegions) {
        const horizon::Climate c = horizon::climateOf(region);
        for (int k = 0; k < 32; ++k) {
            const float a = 6.2831853f * float(k) / 32.0f;
            const float flat = std::sqrt(1.0f - horizon::kRiseHigh * horizon::kRiseHigh);
            const horizon::Hit top = horizon::march(c, 1500.0f, horizon::kEyeHeight, flat * std::cos(a),
                                                    horizon::kRiseHigh, flat * std::sin(a), 128, pixel);
            if (top.hit || top.coverage > 0.0f) std::printf("  %s: land at the top row, way %d\n", region, k);
            CHECK(!top.hit && top.coverage == 0.0f);
        }
    }
}

// The panorama's light by the time (the sky pass's twin): the night darker, the dusk warmer than the day; and the
// land's colour into the screen's (ACES, gamma) the same way in the shader and the preview
TEST(horizon_light_by_the_night) {
    const horizon::LightSet day = horizon::lightAt(0.0f), dusk = horizon::lightAt(0.5f), night = horizon::lightAt(1.0f);
    CHECK(night.sun.r + night.sun.g + night.sun.b < 0.25f * (day.sun.r + day.sun.g + day.sun.b));
    CHECK(night.ambient.b < day.ambient.b);
    CHECK(dusk.sun.r / dusk.sun.b > day.sun.r / day.sun.b);
    const horizon::Rgb black = horizon::toDisplay({0, 0, 0}), bright = horizon::toDisplay({4, 4, 4});
    CHECK(black.r == 0.0f && black.g == 0.0f && black.b == 0.0f);
    CHECK(bright.r > 0.95f && bright.r <= 1.0f);
    const horizon::Rgb mid = horizon::toDisplay({0.18f, 0.18f, 0.18f});
    CHECK(mid.r > 0.3f && mid.r < 0.6f);
}

// The bake's progress, the renderers' twin state (core horizon::Bake): a slice of rows a frame, ready after the last,
// faded in over a second; a new key (another region's map mid-bake, another level) starts it anew
TEST(horizon_bake_by_slices) {
    horizon::Bake b;
    const horizon::Key k{"Prontera", {0.5f, -0.7f, 0.5f}, false, 1000.0f};
    const horizon::Level l = horizon::levelForCard(2);  // 384 rows over 64 frames: 6 a frame
    horizon::Slice s = horizon::step(b, k, l, 1000);
    CHECK(s.restart && s.row0 == 0 && s.row1 == 6 && !s.last);
    CHECK(!b.ready && horizon::fadeOf(b, 1000) == 0.0f);
    for (int i = 1; i < 64; ++i) s = horizon::step(b, k, l, 1000 + uint64_t(i) * 16);
    CHECK(s.last && s.row0 == 378 && s.row1 == 384 && !s.restart);
    CHECK(b.ready && b.frames == 64);
    const uint64_t at = 1000 + 63 * 16;
    s = horizon::step(b, k, l, at + 100);
    CHECK(s.row1 <= s.row0 && !s.restart && !s.last);  // nothing more to draw
    CHECK(horizon::fadeOf(b, at + 500) > 0.45f && horizon::fadeOf(b, at + 500) < 0.55f);
    CHECK(horizon::fadeOf(b, at + 2000) == 1.0f);
}

TEST(horizon_bake_restarts_on_a_new_key) {
    horizon::Bake b;
    const horizon::Level l = horizon::levelForCard(2);
    const horizon::Key k{"Prontera", {0.5f, -0.7f, 0.5f}, false, 1000.0f};
    horizon::step(b, k, l, 0);
    horizon::step(b, k, l, 16);
    horizon::Key m = k;
    m.region = "Payon";
    const horizon::Slice s = horizon::step(b, m, l, 32);  // a map of another region mid-bake
    CHECK(s.restart && s.row0 == 0 && !b.ready && horizon::fadeOf(b, 32) == 0.0f);
    horizon::Bake w;
    for (int i = 0; i < 64; ++i) horizon::step(w, k, l, uint64_t(i));
    CHECK(w.ready);
    const horizon::Slice r = horizon::step(w, k, horizon::levelForCard(1), 100);  // the weak level's textures
    CHECK(r.restart && r.row0 == 0 && r.row1 == 8 && !w.ready);  // 192 rows over 24 frames
}

// 3.0.22-beta.13 (the owner's R-VDN9YH «над горизонтом ... очень размыто», cmd_fild02): no wall right past the map's
// edge - Comodo's islands stood at their whole height a step past the beach, up to the strip's roof; within 1500 of
// the edge the land in every climate stays under 0.15 of its distance, its mountains and islands rising farther off
TEST(horizon_no_wall_past_the_edge) {
    for (const char* region : kRegions) {
        const horizon::Climate c = horizon::climateOf(region);
        for (const float radius : {800.0f, 1500.0f, 2500.0f}) {
            float worst = 0.0f;
            for (int k = 0; k < 256; ++k) {
                const float a = 6.2831853f * float(k) / 256.0f;
                for (float d = radius; d <= radius + 1500.0f; d += 50.0f)
                    worst = std::max(worst, horizon::heightAt(c, radius, d * std::cos(a), d * std::sin(a)) / d);
            }
            if (worst >= 0.15f)
                std::printf("  %s, the map %.0f: the land %.2f of its distance\n", region, radius, worst);
            CHECK(worst < 0.15f);
        }
    }
}

// 3.0.22-beta.13: the far sea pale toward the horizon (the sky in it at a grazing look, as the world's own water's) -
// the strip's sea was dark blue to the horizon, a dark band past the sight's end where the world's water turns into it
TEST(horizon_sea_shines_toward_the_horizon) {
    CHECK(horizon::seaShine(0.0f) == 1.0f);
    CHECK(horizon::seaShine(-0.005f) > 0.8f);
    CHECK(horizon::seaShine(horizon::kRiseLow) < 0.2f);
    CHECK(horizon::seaShine(-0.01f) > horizon::seaShine(-0.03f));
    CHECK(horizon::seaShine(0.02f) == horizon::seaShine(-0.02f));
}

// 3.0.22-beta.15 (the owner 19.11 «можно» - the sun moving by the clock): the sun moved on more than a degree - the
// strip's shadows drawn again slice by slice while it stays shown (no fade, no new textures); less than a degree, or
// while they are being drawn, nothing new till they are done; the weak level draws no shadows, the sun nothing there
TEST(horizon_bake_relights_as_the_sun_moves) {
    horizon::Bake b;
    const horizon::Level l = horizon::levelForCard(2);  // 64 frames of 6 rows
    const horizon::Key k{"Prontera", {0.5f, -0.7f, 0.5f}, false, 1000.0f};
    for (int i = 0; i < 64; ++i) horizon::step(b, k, l, uint64_t(i));
    CHECK(b.ready);
    horizon::Key small = k;
    small.sun[0] += 0.01f;  // half a degree
    const horizon::Slice none = horizon::step(b, small, l, 4000);
    CHECK(!none.restart && !none.relight && none.row1 <= none.row0);
    horizon::Key m = k;
    m.sun[0] = 0.6f;
    const horizon::Slice s = horizon::step(b, m, l, 5000);
    CHECK(!s.restart && s.relight && s.row0 == 0 && s.row1 == 6 && !s.last);
    CHECK(b.ready && horizon::fadeOf(b, 5000) == 1.0f);
    horizon::Key again = m;
    again.sun[2] = 0.3f;  // on again while its shadows are drawn: those finished first
    horizon::Slice t = horizon::step(b, again, l, 5001);
    CHECK(t.relight && t.row0 == 6 && !t.restart);
    for (int i = 2; i < 64; ++i) t = horizon::step(b, again, l, 5000 + uint64_t(i));
    CHECK(t.last && t.relight && b.ready && t.row1 == 384 && b.frames == 64);  // the first bake's frames kept
    const horizon::Slice next = horizon::step(b, again, l, 6000);  // then for the sun as it stands now
    CHECK(next.relight && next.row0 == 0);
    horizon::Bake w;
    const horizon::Level weak = horizon::levelForCard(1);
    for (int i = 0; i < 24; ++i) horizon::step(w, k, weak, uint64_t(i));
    CHECK(w.ready);
    const horizon::Slice n = horizon::step(w, m, weak, 100);
    CHECK(!n.restart && !n.relight && n.row1 <= n.row0);
}

// 3.0.22-beta.15: the panorama's light by the height of what lights it (the sky pass's twin): as before high up; a
// low sun's warmer; at the horizon and under it none of its own, the ambient left
TEST(horizon_light_by_the_height_of_its_source) {
    const horizon::LightSet day = horizon::lightAt(0.0f), high = horizon::lightAt(0.0f, 0.7f);
    CHECK(high.sun.r == day.sun.r && high.sun.g == day.sun.g && high.sun.b == day.sun.b);
    CHECK(high.ambient.r == day.ambient.r && high.ambient.b == day.ambient.b);
    const horizon::LightSet low = horizon::lightAt(0.0f, 0.12f);
    CHECK(low.sun.r / low.sun.b > day.sun.r / day.sun.b);  // golden
    const horizon::LightSet edge = horizon::lightAt(0.0f, -0.02f), under = horizon::lightAt(1.0f, -0.3f);
    CHECK(edge.sun.r == 0.0f && edge.sun.g == 0.0f && edge.sun.b == 0.0f);
    CHECK(under.sun.r == 0.0f && under.ambient.b > 0.0f);
    const horizon::LightSet moon = horizon::lightAt(1.0f, 0.6f), night = horizon::lightAt(1.0f);
    CHECK(moon.sun.b == night.sun.b);  // a high moon's light the night's as it was
}
