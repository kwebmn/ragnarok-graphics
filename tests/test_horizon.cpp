#include <cmath>
#include <set>
#include <string>

#include "core/horizon.hpp"
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

TEST(horizon_strip_geometry) {
    const horizon::Level full = horizon::levelFor(false), weak = horizon::levelFor(true);
    CHECK(full.width == 4096 && full.height == 384 && full.steps == 128 && full.shadowSteps == 24 && full.slices == 8);
    CHECK(weak.width == 2048 && weak.height == 192 && weak.steps == 64 && weak.shadowSteps == 0 && weak.slices == 16);
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
    const horizon::Level l = horizon::levelFor(false);
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
