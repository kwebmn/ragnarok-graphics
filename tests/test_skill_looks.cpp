// 3.0.18-beta.183 (the owner: «а можем сделать чтото типа оригинал, улушеный орининал и перерисованый ... для всех
// скилов магов», «и других професий», «лучше одним пунктом чтобы не усложнять интерфейс»): [effects] skill_looks - one
// choice for every class's skills over what the set and the groups leave on: the game's own, the game's made sharp,
// redrawn (the classes' new looks), redrawn soft; the classes' and the elements' own boxes out of sight
#include <cstring>
#include <string>

#include "core/config.hpp"
#include "core/effect_groups.hpp"
#include "core/settings/effects_layout.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(skill_looks_read_and_its_old_switches) {
    CHECK(parseConfig("").effects.skillLooks == kSkillSoft);  // 3.0.18-beta.186: the owner's «redraw soft по умолчанию»
    CHECK(parseConfig("[effects]\nskill_looks = original\n").effects.skillLooks == kSkillOriginal);
    CHECK(parseConfig("[effects]\nskill_looks = improved\n").effects.skillLooks == kSkillImproved);
    CHECK(parseConfig("[effects]\nskill_looks = redrawn\n").effects.skillLooks == kSkillRedrawn);
    CHECK(parseConfig("[effects]\nskill_looks = soft\n").effects.skillLooks == kSkillSoft);
    // a file of before: its own boxes say which
    CHECK(parseConfig("[effects]\ncrisp_storm_gust = 1\n").effects.skillLooks == kSkillImproved);
    CHECK(parseConfig("[effects]\nmage_soft = 1\n").effects.skillLooks == kSkillSoft);
    CHECK(parseConfig("[effects]\nmage_soft = 1\nskill_looks = original\n").effects.skillLooks == kSkillOriginal);
    const settings::IniSetting* s = settings::findSetting("effects.skill_looks");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->control == settings::Control::Choice && s->tokens.size() == 4 && s->choices.size() == 4);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[effects]\nskill_looks = improved\n")), std::string("improved"));
    CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("soft"));
}

TEST(skill_looks_over_the_classes_looks) {
    EffectSettings e;
    e.mageEffects = e.swordEffects = e.hitEffects = e.monsterEffects = true;  // as a set leaves them
    e.skillLooks = kSkillOriginal;
    EffectSettings o = effect_groups::looksStyled(e);
    CHECK(!o.mageEffects && !o.swordEffects && !o.hitEffects && !o.monsterEffects && !o.crispStormGust);
    e.skillLooks = kSkillImproved;
    o = effect_groups::looksStyled(e);
    CHECK(!o.mageEffects && !o.swordEffects && o.crispStormGust && !o.mageSoft);
    e.skillLooks = kSkillRedrawn;
    o = effect_groups::looksStyled(e);
    CHECK(o.mageEffects && o.swordEffects && !o.crispStormGust && !o.mageSoft);
    e.skillLooks = kSkillSoft;
    o = effect_groups::looksStyled(e);
    CHECK(o.mageEffects && o.monsterEffects && o.mageSoft && !o.crispStormGust);
    EffectSettings none;  // the set or the group «Уміння» off: nothing to make sharp either
    none.skillLooks = kSkillImproved;
    CHECK(!effect_groups::looksStyled(none).crispStormGust && !effect_groups::looksStyled(none).sharpSpecks);
}

// 3.0.18-beta.184 (the owner: «давай без этого всего, сделай все без остановки»): every skill's sheets of specks drawn
// anew at real resolution (the snow, the sparkles) in every look but the original, while the skills show anew at all
TEST(skill_looks_every_skills_specks_sharp) {
    EffectSettings e;
    e.swordEffects = true;  // a set leaves a class's look on
    for (int look : {kSkillImproved, kSkillRedrawn, kSkillSoft}) {
        e.skillLooks = look;
        CHECK(effect_groups::looksStyled(e).sharpSpecks);
    }
    e.skillLooks = kSkillOriginal;
    CHECK(!effect_groups::looksStyled(e).sharpSpecks);
}

TEST(skill_looks_one_row_the_classes_boxes_out_of_sight) {
    CHECK(settings::hiddenEffect("effects.mage_effects") && settings::hiddenEffect("effects.sword_effects"));
    CHECK(settings::hiddenEffect("effects.mage_soft") && settings::hiddenEffect("effects.crisp_storm_gust"));
    CHECK(settings::hiddenEffect("effects.mage_water") && settings::hiddenEffect("effects.monster_effects"));
    CHECK(!settings::hiddenEffect("effects.skill_looks") && !settings::hiddenEffect("effects.skills_soft"));
    const settings::EffectLayout* fx = settings::effectLayout();
    for (int e = 0; e < settings::effectLayoutCount(); ++e) {
        CHECK(!settings::hiddenEffect(fx[e].box));
        for (int k = 0; k < settings::kLayoutChecks && fx[e].checks[k]; ++k)
            CHECK(!settings::hiddenEffect(fx[e].checks[k]));
    }
}
