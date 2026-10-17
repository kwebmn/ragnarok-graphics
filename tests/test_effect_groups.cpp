#include <string>

#include "core/effect_groups.hpp"
#include "core/settings/auto_setup.hpp"
#include "core/settings/effects_layout.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::effect_groups;

TEST(effect_groups_hold_every_switch_of_the_page) {
    // 2.2: each box of the Effects page's list is in one group, each member is a box of it
    const settings::EffectLayout* layout = settings::effectLayout();
    for (int i = 0; i < settings::effectLayoutCount(); ++i) CHECK(memberOf(layout[i].box) != nullptr);
    for (int m = 0; m < memberCount(); ++m) {
        bool found = false;
        for (int i = 0; i < settings::effectLayoutCount(); ++i)
            found = found || std::string(layout[i].box) == std::string("effects.") + members()[m].key;
        CHECK(found);
        CHECK(members()[m].group >= 0 && members()[m].group < kEffectGroups);
    }
    CHECK(memberOf("effects.torch_flame") == nullptr);  // a setting of the torches, not a switch of its own
}

TEST(effect_groups_sets_grow_from_weak_to_strong) {
    EffectSettings e;
    e.preset = EffectPreset::Weak;
    const EffectSettings weak = effective(e, 3);
    e.preset = EffectPreset::Medium;
    const EffectSettings medium = effective(e, 3);
    e.preset = EffectPreset::Strong;
    const EffectSettings strong = effective(e, 3);
    e.preset = EffectPreset::Off;
    const EffectSettings off = effective(e, 3);
    for (int m = 0; m < memberCount(); ++m) {
        const Member& x = members()[m];
        CHECK(!(off.*x.on));
        CHECK(!(weak.*x.on) || (medium.*x.on));
        CHECK(!(medium.*x.on) || (strong.*x.on));
        CHECK((strong.*x.on) == (x.level > 0));
    }
    CHECK(weak.torch && !weak.grass && medium.grass && !medium.materials && strong.materials && strong.newTrees);
    CHECK(!strong.animeTrees && !strong.filmGrain);  // the player's own only
    CHECK(weak.tier == 1 && strong.tier == 2);
}

TEST(effect_groups_auto_follows_the_card) {
    CHECK_EQ(cardLevel(false, 16000), 1);  // Direct3D 11
    CHECK_EQ(cardLevel(true, 2048), 1);
    CHECK_EQ(cardLevel(true, 6144), 2);
    CHECK_EQ(cardLevel(true, 16311), 3);
    EffectSettings e;
    e.preset = EffectPreset::Auto;
    CHECK(!effective(e, 1).grass && effective(e, 2).grass && effective(e, 3).materials);
}

TEST(effect_groups_custom_keeps_the_switches_a_group_turns_its_off) {
    EffectSettings e;  // custom by default: as written
    e.grass = true;
    e.water = false;
    CHECK(effective(e, 1).grass && !effective(e, 3).water);
    e.groups[Nature] = false;
    CHECK(!effective(e, 3).grass && effective(e, 3).torch);
    e.preset = EffectPreset::Strong;
    CHECK(!effective(e, 3).newTrees && effective(e, 3).water);
}

TEST(effect_groups_strength_scales_its_effects) {
    EffectSettings e;
    e.groupStrength[Light] = 50;
    e.groupStrength[Shadows] = 200;
    const EffectSettings x = effective(e, 3);
    CHECK_EQ(x.torchLight, 50);
    CHECK_EQ(x.lampsLight, 30);
    CHECK_EQ(x.actorShadowsStrength, 100);  // 55 x 2, kept to its most
    CHECK_EQ(x.hazeStrength, e.hazeStrength);  // another group's as it was
    CHECK(!hasStrength(Life) && hasStrength(Water));
}

TEST(effect_groups_ini_words) {
    const Config c = parseConfig("[effects]\npreset = medium\ngroup_water = 0\ngroup_light_strength = 70\n");
    CHECK(c.effects.preset == EffectPreset::Medium);
    CHECK(!c.effects.groups[Water] && c.effects.groups[Light]);
    CHECK_EQ(c.effects.groupStrength[Light], 70);
    CHECK(parseConfig("").effects.preset == EffectPreset::Custom);  // an old ini keeps its switches
    EffectPreset p = EffectPreset::Custom;
    CHECK(presetFrom("strong", p) && p == EffectPreset::Strong && !presetFrom("ultra", p));
    CHECK(std::string(presetToken(EffectPreset::Auto)) == "auto");
}

TEST(effect_groups_auto_setup_picks_the_set) {
    settings::Machine m;
    m.windows10 = m.d3d12 = true;
    m.vendor = 0x10de;
    m.vramMb = 16311;
    auto word = [](const settings::AutoSetup& a) {
        for (const settings::IniEdit& e : a.edits)
            if (e.section == "effects" && e.key == "preset") return e.value;
        return std::string();
    };
    CHECK(word(settings::autoSetup(m)) == "strong");
    m.vramMb = 6144;
    CHECK(word(settings::autoSetup(m)) == "medium");
    m.vramMb = 2048;
    CHECK(word(settings::autoSetup(m)) == "weak");
    m.vendor = 0x1414;  // no driver
    CHECK(word(settings::autoSetup(m)) == "off");
}
