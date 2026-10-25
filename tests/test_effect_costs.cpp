#include <algorithm>
#include <cstdio>
#include <string>

#include "core/effect_costs.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(effect_costs_read_back_what_was_written) {
    effect_costs::Costs c{{"ao", 0.42}, {"floor", 1.25}};
    const effect_costs::Costs back = effect_costs::parse(effect_costs::format(c) + "junk\nshafts -3\n");
    CHECK(back.size() == 2 && back.at("floor") > 1.24 && back.at("floor") < 1.26);
    CHECK(effect_costs::costOf(back, "effects.ao") > 0.41);
    CHECK(effect_costs::costOf(back, "shafts") < 0);    // not measured
    CHECK(effect_costs::costOf(back, "torch_flame") < 0);  // not one of them
}

TEST(effect_costs_drop_the_costliest_first) {
    // 0.76, [frames] keep_fps: the costliest measured; without measures the usual order; a dropped one once.
    EffectSettings e;
    e.ao = true;
    const effect_costs::Costs c{{"ao", 0.9}, {"floor", 0.6}, {"mist", 0.01}};
    CHECK(effect_costs::pickToDrop(e, c, {}) == "ao");
    CHECK(effect_costs::pickToDrop(e, c, {"ao"}) == "floor_reflections");
    CHECK(effect_costs::pickToDrop(e, {}, {}) == "floor_reflections");
    effect_costs::drop(e, "floor_reflections");
    CHECK(!e.floorReflections);
    CHECK(effect_costs::pickToDrop(e, {}, {}) == "ao");  // the usual order's next
    EffectSettings none = effectsOff();
    CHECK(effect_costs::pickToDrop(none, c, {}).empty());
}

TEST(effect_costs_keep_fps_drops_the_worlds_extras) {
    // 2.5.16: the materials, the relief, the characters' light... are drawn inside the world's draws - no pass of their
    // own to time, so keep_fps never turned them off (R-DUMAMX, an HD 6570: the world 40-50 ms a frame, the passes a
    // few). With the world timed: a pass worth a millisecond first; then they, the usually costliest first; then the
    // cheap passes. Without (Direct3D 12): the passes as before, then they.
    EffectSettings e = effectsOff();
    e.materials = e.relief = e.spriteLight = e.ao = e.glow = true;
    const effect_costs::Costs cheap{{"ao", 0.45}, {"glow", 0.3}, {"world", 41.0}};
    CHECK(effect_costs::pickToDrop(e, cheap, {}) == "materials");
    CHECK(effect_costs::pickToDrop(e, cheap, {"materials"}) == "relief");
    CHECK(effect_costs::pickToDrop(e, cheap, {"materials", "relief"}) == "sprite_light");
    CHECK(effect_costs::pickToDrop(e, cheap, {"materials", "relief", "sprite_light"}) == "ao");  // then the passes
    const effect_costs::Costs dear{{"ao", 4.8}, {"glow", 0.3}, {"world", 41.0}};
    CHECK(effect_costs::pickToDrop(e, dear, {}) == "ao");  // a millisecond's pass before the extras
    const effect_costs::Costs untimed{{"ao", 0.45}, {"glow", 0.3}};  // Direct3D 12: no world's time
    CHECK(effect_costs::pickToDrop(e, untimed, {}) == "ao");
    CHECK(effect_costs::pickToDrop(e, untimed, {"ao", "glow"}) == "materials");  // the passes gone: the extras
    effect_costs::drop(e, "effects.materials");
    CHECK(!e.materials);
    CHECK(effect_costs::costOf(dear, "effects.relief") < 0);  // not measured on its own
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

TEST(effect_costs_keep_fps_leaves_what_the_player_turned_on) {
    // 3.0.4-beta.11 (R-EQDYNE «нету травы»: the grass turned on in the console at 23:00, off by keep_fps at 23:01): the
    // switches turned on by hand while the game runs - keep_fps passes them by
    EffectSettings was, now;
    was.grass = false;
    now.grass = true;
    was.ao = now.ao = true;           // on before and after: not the player's doing now
    was.bloom = true;
    now.bloom = false;                // turned off: nothing to keep
    was.materials = false;
    now.materials = true;             // the world's extras count too
    const std::vector<std::string> on = effect_costs::turnedOn(was, now);
    CHECK_EQ(on.size(), size_t(2));
    CHECK(std::find(on.begin(), on.end(), "grass") != on.end());
    CHECK(std::find(on.begin(), on.end(), "materials") != on.end());
    CHECK(effect_costs::turnedOn(now, now).empty());
}

TEST(effect_costs_keep_fps_thins_the_grass_first_and_turns_it_off_last) {
    // 3.0.4-beta.35 (the user: «у людей отключается трава на слабых видеокартах»; R-EQDYNE, a Radeon 780M: the grass
    // 4.94 ms, the costliest, went first): keep_fps thins the grass (its roots on every second row - half its work),
    // then turns the others off by their cost, the grass itself last
    EffectSettings e = effectsOff();
    e.grass = e.ao = e.glow = true;
    const effect_costs::Costs c{{"grass", 4.94}, {"ao", 1.71}, {"glow", 0.85}};
    CHECK(effect_costs::pickToDrop(e, c, {}) == "grass_thin");
    effect_costs::drop(e, "grass_thin");
    CHECK(e.grass && e.grassThin);
    CHECK(effect_costs::pickToDrop(e, c, {"grass_thin"}) == "ao");  // then the costliest of the others
    CHECK(effect_costs::pickToDrop(e, c, {"grass_thin", "ao"}) == "glow");
    CHECK(effect_costs::pickToDrop(e, c, {"grass_thin", "ao", "glow"}) == "grass");  // the grass last
    EffectSettings cheap = effectsOff();  // a little grass: the others first, it thinned once they are gone
    cheap.grass = cheap.ao = true;
    const effect_costs::Costs little{{"grass", 0.4}, {"ao", 1.2}};
    CHECK(effect_costs::pickToDrop(cheap, little, {}) == "ao");
    CHECK(effect_costs::pickToDrop(cheap, little, {"ao"}) == "grass_thin");
    CHECK(effect_costs::pickToDrop(e, {}, {}) == "ao");  // unmeasured: the usual order, the grass still last
    EffectSettings own = e;  // turned on by hand in this game: left whole
    own.grassThin = false;
    CHECK(effect_costs::pickToDrop(own, c, {"grass"}) == "ao");
    CHECK(effect_costs::pickToDrop(own, c, {"grass", "ao", "glow"}).empty());
}
