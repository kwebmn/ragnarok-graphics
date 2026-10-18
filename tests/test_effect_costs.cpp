#include <cstdio>
#include <string>

#include "core/effect_costs.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(effect_costs_read_back_what_was_written) {
    effect_costs::Costs c{{"ao", 0.42}, {"floor", 1.25}};
    const effect_costs::Costs back = effect_costs::parse(effect_costs::format(c) + "junk\nsky -3\n");
    CHECK(back.size() == 2 && back.at("floor") > 1.24 && back.at("floor") < 1.26);
    CHECK(effect_costs::costOf(back, "effects.ao") > 0.41);
    CHECK(effect_costs::costOf(back, "sky") < 0);       // not measured
    CHECK(effect_costs::costOf(back, "torch_flame") < 0);  // not one of them
}

TEST(effect_costs_drop_the_costliest_first) {
    // 0.76, [frames] keep_fps: the costliest measured; without measures the usual order; a dropped one once.
    EffectSettings e;
    e.ao = e.dof = true;
    const effect_costs::Costs c{{"ao", 0.9}, {"dof", 0.3}, {"floor", 0.6}, {"haze", 0.01}};
    CHECK(effect_costs::pickToDrop(e, c, {}) == "ao");
    CHECK(effect_costs::pickToDrop(e, c, {"ao"}) == "floor_reflections");
    CHECK(effect_costs::pickToDrop(e, {}, {}) == "floor_reflections");
    effect_costs::drop(e, "floor_reflections");
    CHECK(!e.floorReflections);
    CHECK(effect_costs::pickToDrop(e, {}, {}) == "dof");
    EffectSettings none = effectsOff();
    CHECK(effect_costs::pickToDrop(none, c, {}).empty());
}

TEST(effect_costs_keep_fps_drops_the_light_from_the_sky) {
    // 2.5.14: the light from the sky and from around (one pass, "sky_light") was the costliest of an HD 6570's frame
    // (R-7NUW85: 29 ms) and keep_fps never turned it off - none of its switches was known to it
    EffectSettings e = effectsOff();
    e.skyLight = e.bounceLight = e.ao = e.grass = true;
    const effect_costs::Costs c{{"sky_light", 29.47}, {"ao", 4.57}, {"grass", 2.42}};
    CHECK(effect_costs::pickToDrop(e, c, {}) == "sky_light");
    effect_costs::drop(e, "sky_light");
    CHECK(!e.skyLight && e.bounceLight);
    CHECK(effect_costs::pickToDrop(e, c, {"sky_light"}) == "bounce_light");  // the pass runs while either is on
    effect_costs::drop(e, "effects.bounce_light");
    CHECK(!e.bounceLight);
    CHECK(effect_costs::passOf("effects.sky_light") == "sky_light");
    CHECK(effect_costs::costOf(c, "effects.bounce_light") > 29.0);
}

TEST(effect_costs_every_timed_pass_has_a_switch) {
    // keep_fps can turn off whatever the renderer times: a pass with no switch of its own stays however costly
    for (const effect_costs::TimedPass& p : effect_costs::kTimedPasses) {
        bool found = false;
        for (const settings::IniSetting& s : settings::iniSettings())
            if (std::string(s.section) == "effects" && effect_costs::passOf(s.key) == p.key) found = true;
        if (!found) std::printf("  no switch for the pass %s\n", p.key);
        CHECK(found);
    }
}

TEST(effect_costs_keep_fps_blames_the_card_only_when_it_is_the_card) {
    // 2.2.2: a window under 90 % of the frames wanted is the card's fault only when its own work fills the frame
    // (85 % of the frame's time at that rate) - a game slow for the processor's part keeps its effects
    CHECK(effect_costs::slowForCard(45.0, 60, 15.5));   // 15.5 ms of 16.7: the card
    CHECK(!effect_costs::slowForCard(45.0, 60, 7.0));   // the card idles half the frame: the processor
    CHECK(!effect_costs::slowForCard(58.0, 60, 16.5));  // 58 is not slow (54 is)
    CHECK(effect_costs::slowForCard(45.0, 60, -1.0));   // nothing measured: by the frames alone, as before
    CHECK(!effect_costs::slowForCard(70.0, 60, -1.0));
}
