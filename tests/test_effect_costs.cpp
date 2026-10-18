#include "core/effect_costs.hpp"
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


TEST(effect_costs_keep_fps_blames_the_card_only_when_it_is_the_card) {
    // 2.2.2: a window under 90 % of the frames wanted is the card's fault only when its own work fills the frame
    // (85 % of the frame's time at that rate) - a game slow for the processor's part keeps its effects
    CHECK(effect_costs::slowForCard(45.0, 60, 15.5));   // 15.5 ms of 16.7: the card
    CHECK(!effect_costs::slowForCard(45.0, 60, 7.0));   // the card idles half the frame: the processor
    CHECK(!effect_costs::slowForCard(58.0, 60, 16.5));  // 58 is not slow (54 is)
    CHECK(effect_costs::slowForCard(45.0, 60, -1.0));   // nothing measured: by the frames alone, as before
    CHECK(!effect_costs::slowForCard(70.0, 60, -1.0));
}
