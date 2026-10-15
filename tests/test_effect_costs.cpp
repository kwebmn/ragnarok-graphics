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

TEST(effect_costs_on_direct3d_11_only_what_it_draws) {
    EffectSettings e;
    e.ao = e.dof = true;
    CHECK(effect_costs::pickToDrop(e, {}, {}, true) == "actor_shadows");
}
