// 3.0.24-beta.37 (the laptop's run, AMD Radeon 610M): effects.ao = 1 through the MCP's «setting» did nothing - written
// straight, the effects' set «auto» went on deciding the switches; a change made as Alt+O makes it: the set made the
// player's own first, the value checked, a value already so said
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/setting_change.hpp"
#include "test.hpp"

using namespace uagfx;
namespace sc = uagfx::settingchange;

TEST(setting_change_an_effect_under_a_set_makes_the_set_own) {
    Config c;
    c.effects.preset = EffectPreset::Auto;
    std::vector<settings::IniEdit> e;
    CHECK(sc::edits(c, 1, "effects.ao", "1", e) == sc::Outcome::Edits);
    bool custom = false;
    int ao = 0;
    for (const settings::IniEdit& x : e) {
        custom = custom || (x.section == "effects" && x.key == "preset" && x.value == "custom");
        ao += x.key == "ao";
    }
    CHECK(custom);
    CHECK_EQ(ao, 1);  // written once, last
    CHECK(!e.empty() && e.back().section == "effects" && e.back().key == "ao" && e.back().value == "1");
    // off under the weak set: so already, nothing written
    e.clear();
    CHECK(sc::edits(c, 1, "effects.ao", "0", e) == sc::Outcome::Same);
    CHECK(e.empty());
    // not an effect: its line alone
    c.uiTheme = "dark";
    e.clear();
    CHECK(sc::edits(c, 1, "interface.theme", "light", e) == sc::Outcome::Edits);
    CHECK_EQ(e.size(), size_t(1));
    // a value it does not take, a key it does not know
    e.clear();
    CHECK(sc::edits(c, 1, "effects.ao", "7", e) == sc::Outcome::Invalid);
    CHECK(sc::edits(c, 1, "effects.no_such_one", "1", e) == sc::Outcome::Unknown);
    CHECK(e.empty());
    // custom already: the switch alone
    c.effects.preset = EffectPreset::Custom;
    e.clear();
    CHECK(sc::edits(c, 1, "effects.ao", "1", e) == sc::Outcome::Edits);
    CHECK_EQ(e.size(), size_t(1));
}
