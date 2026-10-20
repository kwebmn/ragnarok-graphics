#include <string>

#include "core/config.hpp"
#include "core/effect_groups.hpp"
#include "core/settings/effects_layout.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

namespace {

// The box a check sits under on the Effects page (and in the game's console), or "" under none.
std::string boxOf(const std::string& check) {
    const EffectLayout* fx = effectLayout();
    for (int e = 0; e < effectLayoutCount(); ++e)
        for (int k = 0; k < kLayoutChecks && fx[e].checks[k]; ++k)
            if (check == fx[e].checks[k]) return fx[e].box;
    return "";
}

}  // namespace

TEST(effect_parts_have_their_own_boxes_under_the_effect) {
    // 2.4: what an effect draws besides its main picture has a box of its own under it - on, as it was drawn before
    static const char* const kParts[][2] = {
        {"effects.water", "effects.water_foam"},       {"effects.water", "effects.water_glints"},
        {"effects.water", "effects.caustics"},         {"effects.water", "effects.waterfalls"},
        {"effects.footprints", "effects.step_dust"},
        {"effects.seasons", "effects.season_snow"},    {"effects.seasons", "effects.season_grass"},
        {"effects.motes", "effects.pollen"},           {"effects.motes", "effects.indoor_dust"},
        {"effects.weather", "effects.rain_splashes"},  {"effects.sky", "effects.stars"},
        {"effects.heat", "effects.desert_heat"},
        {"effects.shafts", "effects.sky_rays"},        {"effects.tree_tints", "effects.tree_seasons"},
        {"effects.butterflies", "effects.dragonflies"}};
    const Config defaults = parseConfig("");
    for (const auto& p : kParts) {
        if (boxOf(p[1]) != p[0]) std::printf("  not under %s: %s\n", p[0], p[1]);
        CHECK(boxOf(p[1]) == p[0]);
        const IniSetting* s = findSetting(p[1]);
        CHECK(s != nullptr);
        if (!s) continue;
        CHECK(s->control == Control::Check);
        CHECK(tokenOf(*s, defaults) == "1");
        CHECK(effect_groups::memberOf(p[1]) == nullptr);  // the effect's, not a switch of a group
        Config off = defaults;
        off.effects = effectsOff();
        CHECK(tokenOf(*s, off) == "0");  // Pause shows the game's own picture
    }
}

TEST(effect_pebbles_are_gone) {
    // 2.4: the pebbles were specks a few pixels across over every paved square - gone; their old line is read and left
    CHECK(findSetting("effects.ground_clutter") == nullptr);
    CHECK(effect_groups::memberOf("effects.ground_clutter") == nullptr);
    CHECK(boxOf("effects.ground_clutter").empty());
    CHECK(parseConfig("[effects]\nground_clutter = 1\n").effects == parseConfig("").effects);
}

TEST(effect_settings_compare_every_field) {
    // 2.4: the comparison left out every field since 1.9 - a change of one of them alone never reached the 64-bit
    // renderer (Remote::setEffects sends only what differs): the seasons off in the console did nothing under DLSS
    int fields = 0;
    for (const IniSetting& s : iniSettings()) {
        if (std::string(s.section) != "effects" || (!s.flag && !s.number)) continue;
        EffectSettings a, b;
        if (s.flag) b.*s.flag = !(a.*s.flag);
        if (s.number) b.*s.number = a.*s.number == s.min ? s.max : s.min;
        ++fields;
        if (a == b) std::printf("  not compared: effects.%s\n", s.key);
        CHECK(a != b);
    }
    CHECK(fields >= 100);
}

TEST(effect_new_trees_are_gone) {
    // 2.4.1: the new trees (2.0) and their anime style are gone - the player keeps the game's own trees (their
    // volume, light, tints and seasons stay); the old lines in the settings are read and left alone
    for (const char* k : {"effects.new_trees", "effects.anime_trees"}) {
        CHECK(findSetting(k) == nullptr);
        CHECK(effect_groups::memberOf(k) == nullptr);
        CHECK(boxOf(k).empty());
    }
    CHECK(parseConfig("[effects]\nnew_trees = 1\nanime_trees = 1\n").effects == parseConfig("").effects);
}
