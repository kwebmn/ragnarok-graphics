#include "core/settings/effects_layout.hpp"

#include <cstring>

namespace uagfx::settings {
namespace {

const EffectLayout kEffects[] = {
    {"effects.smooth_light", {}, {}, {}, 0},
    {"effects.ao", {}, {"effects.ao_strength", "effects.ao_size"}, {}, 0},
    {"effects.bloom", {}, {"effects.bloom_strength"}, {}, 0},  // 0.78
    {"effects.hallucination_wave", {}, {}, {}, 0},  // 3.0.16-beta.37
    {"effects.region_looks", {}, {"effects.region_looks_strength"}, {}, 0},  // 0.79
    {"effects.glow", {}, {"effects.glow_strength", "effects.glow_size", "effects.glow_light"}, {}, 0},
    {"effects.fire_light", {"effects.skill_colours", "effects.light_orbs"}, {"effects.fire_light_strength"}, {}, 0},
    {"effects.actor_shadows", {"effects.actor_shadows_spot"}, {"effects.actor_shadows_strength"}, {}, 0},
    {"effects.heat", {"effects.desert_heat"}, {"effects.heat_strength"}, {}, 0},
    {"effects.weather", {"effects.rain_splashes"}, {"effects.weather_amount"}, {}, 0},
    {"effects.footprints", {"effects.step_dust"}, {}, {}, 0},  // 0.80
    {"effects.storms", {}, {}, {}, 0},      // 0.80
    {"effects.grass", {}, {"effects.grass_amount"}, {}, 0},  // 0.81
    {"effects.skill_sparks", {}, {"effects.skill_sparks_amount"}, {}, 0},  // 0.82
    {"effects.map_fade", {}, {}, {}, 0},                                // 0.83
    {"effects.no_game_fog", {}, {}, {}, 0},                             // 2.6.10
    {"effects.no_server_night", {}, {}, {}, 0},                         // 2.6.13
    {"effects.fix_negative", {}, {}, {}, 0},                            // 3.0.18-beta.229
    {"effects.moon_light", {"effects.moon_beams"}, {"effects.moon_strength"}, {}, 0},  // 2.7.18; 2.7.20 its beams
    {"effects.round_light", {}, {}, {}, 0},  // 0.84
    {"effects.ripples", {}, {}, {}, 0},  // 0.94
    {"effects.glint", {}, {}, {}, 0},  // 0.95
    {"effects.hit_flash", {}, {}, {}, 0},  // 0.96
    {"effects.death_flash", {}, {}, {}, 0},  // 3.0.16-beta.7
    {"effects.body_tints", {}, {}, {}, 0},  // 3.0.16-beta.13
    {"effects.body_stamp", {}, {}, {}, 0},  // 3.0.16-beta.25
    {"effects.sharp_sprites", {}, {}, {}, 0},  // 3.0.16-beta.29
    {"effects.wet_ground", {}, {}, {}, 0},  // 0.97
    {"effects.aura", {}, {"effects.aura_brightness"}, {}, 0},
    {"effects.aura_portals", {}, {}, {}, 0},  // 3.0.13-beta.15
    {"effects.silhouette", {}, {}, {}, 0},    // 3.0.13-beta.30
    {"effects.skills_soft", {}, {"effects.skill_brightness"}, {}, 0},
    {"effects.crowd_effects", {}, {}, {}, 0},  // 3.0.13-beta.20
    // 3.0.15-beta.11-3.0.18-beta.181: the classes' looks' own boxes - [effects] skill_looks in their place (.183)
    {"effects.water",
     {"effects.water_foam", "effects.water_glints", "effects.caustics", "effects.waterfalls"},
     {"effects.water_waves", "effects.water_reflections", "effects.water_clarity"},
     {},
     0},
    {"effects.grade",
     {},
     {"effects.grade_saturation", "effects.grade_contrast", "effects.grade_brightness", "effects.grade_warmth",
      "effects.grade_vignette"},
     {},
     0},
    {"effects.foliage", {}, {"effects.foliage_strength"}, {}, 0},                    // 0.64
    {"effects.soft_particles", {}, {}, {}, 0},  // 0.67
    {"effects.rim_light", {}, {"effects.rim_strength"}, {}, 0},  // 0.68
    {"effects.motes", {"effects.pollen", "effects.indoor_dust"}, {"effects.motes_amount"}, {}, 0},  // 0.70
    {"effects.relief", {}, {"effects.relief_strength"}, {}, 0},  // 0.71
    {"effects.floor_reflections", {}, {"effects.floor_reflect_strength"}, {}, 0},  // 0.72
    {"effects.shafts", {"effects.sky_rays", "effects.shafts_lamps"}, {"effects.shafts_strength"}, {}, 0},  // 0.73
    {"effects.day_cycle", {}, {}, {}, 0},  // 0.74
    {"effects.torch",
     {"effects.torch_flame"},
     {"effects.torch_brightness", "effects.torch_glow", "effects.torch_light", "effects.torch_flicker"},
     {"effects.torch_flame"},
     1},
    {"effects.torch_shadows", {"effects.torch_leaf_light"}, {"effects.torch_shadows_strength"}, {}, 1},  // .16, .30
    {"effects.smoke", {}, {"effects.smoke_density"}, {}, 1},
    {"effects.lamps", {}, {"effects.lamps_light"}, {}, 1},
    {"effects.fireflies", {"effects.firefly_dots"}, {"effects.fireflies_brightness"}, {}, 1},  // 3.0.7-beta.3 dots
    {"effects.map_glows", {"effects.map_glow_orbs"}, {"effects.map_glows_brightness"}, {}, 1},
    {"effects.rays", {}, {"effects.rays_brightness"}, {}, 1},
    {"effects.lava", {}, {"effects.lava_brightness"}, {}, 1},
    {"effects.portals", {"effects.portal_new"}, {"effects.portals_strength"}, {}, 1},  // 2.7.48 the new portal
    {"effects.tree_volume", {}, {}, {}, 2},  // 1.7
    {"effects.leaf_light", {}, {}, {}, 2},  // 1.7
    {"effects.tree_tints", {"effects.tree_seasons"}, {}, {}, 2},  // 1.7
    {"effects.flowers", {}, {}, {}, 2},  // 1.7.1
    {"effects.dew", {}, {}, {}, 2},  // 1.7.1
    {"effects.morning_fog", {}, {}, {}, 2},  // 1.7.1
    {"effects.sand_drift", {}, {}, {}, 2},  // 1.7.1
    {"effects.wet_walls", {}, {}, {}, 2},  // 1.7.1
    {"effects.swamp", {}, {}, {}, 2},  // 1.7.2
    {"effects.butterflies", {"effects.dragonflies"}, {}, {}, 2},  // 1.7.3
    {"effects.birds", {}, {}, {}, 2},  // 1.7.3
    {"effects.night_fireflies", {"effects.fireflies_light"}, {}, {}, 2},  // 2.6.11; 2.7.18 their light
    {"effects.leaf_fall", {}, {}, {}, 2},  // 1.7.3
    {"effects.embers", {}, {}, {}, 2},  // 1.7.3
    {"effects.splashes", {}, {}, {}, 2},  // 1.7.3
    {"effects.cave_drips", {}, {}, {}, 2},  // 1.7.3
    {"effects.underwater", {}, {}, {}, 2},  // 1.7.3
    {"effects.materials", {}, {"effects.materials_strength"}, {}, 3},  // 1.8
    {"effects.ground_seams", {}, {}, {}, 3},  // 1.9

    {"effects.smooth_hills", {}, {"effects.smooth_hills_strength"}, {}, 3},  // 1.15
    {"effects.own_ground", {"effects.own_far_models"}, {}, {}, 3},  // 3.0.4-beta.10, 3.0.4-beta.14 its far models
    {"effects.world_reflections", {}, {}, {}, 3},  // 1.16
    {"effects.sprite_light", {}, {"effects.sprite_light_strength"}, {}, 2},  // 1.17
    {"effects.seasons", {"effects.season_snow", "effects.season_grass"}, {}, {}, 2},  // 1.20
    {"effects.rain_wet", {}, {}, {}, 2},  // 1.20
    {"effects.night_windows", {}, {}, {}, 2},  // 1.21
    {"effects.shared_wind", {}, {}, {}, 2},  // 1.22
    {"effects.lit_dust", {}, {}, {}, 2},  // 1.23
};

}  // namespace

const EffectLayout* effectLayout() { return kEffects; }
int effectLayoutCount() { return int(sizeof kEffects / sizeof kEffects[0]); }

bool hiddenEffect(const char* sectionKey) {
    static const char* const kHidden[] = {
        "effects.mage_effects",    "effects.mage_casting",    "effects.mage_fire",       "effects.mage_water",
        "effects.mage_wind",       "effects.mage_earth",      "effects.mage_spirit",     "effects.mage_soft",
        "effects.crisp_storm_gust", "effects.sword_effects",  "effects.acolyte_effects", "effects.archer_effects",
        "effects.thief_effects",   "effects.merchant_effects", "effects.other_effects",  "effects.hit_effects",
        "effects.item_effects",    "effects.status_effects",  "effects.travel_effects",  "effects.monster_effects"};
    if (!sectionKey) return false;
    for (const char* k : kHidden)
        if (std::strcmp(k, sectionKey) == 0) return true;
    return false;
}

}  // namespace uagfx::settings
