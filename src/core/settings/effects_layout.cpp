#include "core/settings/effects_layout.hpp"

namespace uagfx::settings {
namespace {

const EffectLayout kEffects[] = {
    {"effects.smooth_light", {}, {}, {}, 0},
    {"effects.ao", {}, {"effects.ao_strength", "effects.ao_size"}, {}, 0},
    {"effects.sun_shadows", {}, {"effects.sun_shadows_strength"}, {}, 0},  // 0.77
    {"effects.bloom", {}, {"effects.bloom_strength"}, {}, 0},  // 0.78
    {"effects.filmic", {}, {}, {}, 0},                           // 0.78
    {"effects.region_looks", {}, {"effects.region_looks_strength"}, {}, 0},  // 0.79
    {"effects.glow", {}, {"effects.glow_strength", "effects.glow_size", "effects.glow_light"}, {}, 0},
    {"effects.fire_light", {"effects.skill_colours"}, {"effects.fire_light_strength"}, {}, 0},
    {"effects.actor_shadows", {}, {"effects.actor_shadows_strength"}, {}, 0},
    {"effects.clouds", {}, {"effects.clouds_strength"}, {}, 0},
    {"effects.haze", {}, {"effects.haze_strength"}, {}, 0},
    {"effects.heat", {}, {"effects.heat_strength"}, {}, 0},
    {"effects.weather", {}, {"effects.weather_amount"}, {}, 0},
    {"effects.footprints", {}, {}, {}, 0},  // 0.80
    {"effects.storms", {}, {}, {}, 0},      // 0.80
    {"effects.grass", {}, {"effects.grass_amount"}, {}, 0},  // 0.81
    {"effects.skill_sparks", {}, {"effects.skill_sparks_amount"}, {}, 0},  // 0.82
    {"effects.film_grain", {}, {"effects.film_grain_amount"}, {}, 0},  // 0.83
    {"effects.map_fade", {}, {}, {}, 0},                                // 0.83
    {"effects.round_light", {}, {}, {}, 0},  // 0.84
    {"effects.appear", {}, {}, {}, 0},  // 0.93
    {"effects.ripples", {}, {}, {}, 0},  // 0.94
    {"effects.glint", {}, {}, {}, 0},  // 0.95
    {"effects.hit_flash", {}, {}, {}, 0},  // 0.96
    {"effects.wet_ground", {}, {}, {}, 0},  // 0.97
    {"effects.flare", {}, {}, {}, 0},  // 0.98
    {"effects.aura", {}, {"effects.aura_brightness"}, {}, 0},
    {"effects.skills_soft", {}, {"effects.skill_brightness"}, {}, 0},
    {"effects.water", {}, {"effects.water_waves", "effects.water_reflections", "effects.water_clarity"}, {}, 0},
    {"effects.grade",
     {},
     {"effects.grade_saturation", "effects.grade_contrast", "effects.grade_brightness", "effects.grade_warmth",
      "effects.grade_vignette"},
     {},
     0},
    {"effects.night", {"effects.night_clock"}, {"effects.night_strength"}, {}, 0},  // 0.63
    {"effects.foliage", {}, {"effects.foliage_strength"}, {}, 0},                    // 0.64
    {"effects.soft_particles", {}, {}, {}, 0},  // 0.67
    {"effects.rim_light", {}, {"effects.rim_strength"}, {}, 0},  // 0.68
    {"effects.sky", {}, {"effects.sky_clouds", "effects.sky_strength"}, {}, 0},  // 0.69
    {"effects.motes", {}, {"effects.motes_amount"}, {}, 0},  // 0.70
    {"effects.relief", {}, {"effects.relief_strength"}, {}, 0},  // 0.71
    {"effects.floor_reflections", {}, {"effects.floor_reflect_strength"}, {}, 0},  // 0.72
    {"effects.shafts", {}, {"effects.shafts_strength"}, {}, 0},  // 0.73
    {"effects.day_cycle", {}, {}, {}, 0},  // 0.74
    {"effects.dof", {}, {"effects.dof_strength", "effects.dof_range"}, {}, 0},
    {"effects.torch",
     {"effects.torch_flame"},
     {"effects.torch_brightness", "effects.torch_glow", "effects.torch_light", "effects.torch_flicker"},
     {"effects.torch_flame"},
     1},
    {"effects.smoke", {}, {"effects.smoke_density"}, {}, 1},
    {"effects.lamps", {}, {"effects.lamps_light"}, {}, 1},
    {"effects.fireflies", {}, {"effects.fireflies_brightness"}, {}, 1},
    {"effects.map_glows", {}, {"effects.map_glows_brightness"}, {}, 1},
    {"effects.rays", {}, {"effects.rays_brightness"}, {}, 1},
    {"effects.lava", {}, {"effects.lava_brightness"}, {}, 1},
    {"effects.portals", {}, {"effects.portals_strength"}, {}, 1},
    {"effects.tree_volume", {}, {}, {}, 2},  // 1.7
    {"effects.leaf_light", {}, {}, {}, 2},  // 1.7
    {"effects.tree_tints", {}, {}, {}, 2},  // 1.7
    {"effects.gusts", {}, {}, {}, 2},  // 1.7
    {"effects.dapple", {}, {}, {}, 2},  // 1.7
    {"effects.flowers", {}, {}, {}, 2},  // 1.7.1
    {"effects.dew", {}, {}, {}, 2},  // 1.7.1
    {"effects.morning_fog", {}, {}, {}, 2},  // 1.7.1
    {"effects.sand_drift", {}, {}, {}, 2},  // 1.7.1
    {"effects.snow_sparkle", {}, {}, {}, 2},  // 1.7.1
    {"effects.wet_walls", {}, {}, {}, 2},  // 1.7.1
    {"effects.shore_foam", {}, {}, {}, 2},  // 1.7.2
    {"effects.fish", {}, {}, {}, 2},  // 1.7.2
    {"effects.swamp", {}, {}, {}, 2},  // 1.7.2
    {"effects.butterflies", {}, {}, {}, 2},  // 1.7.3
    {"effects.birds", {}, {}, {}, 2},  // 1.7.3
    {"effects.leaf_fall", {}, {}, {}, 2},  // 1.7.3
    {"effects.embers", {}, {}, {}, 2},  // 1.7.3
    {"effects.breath", {}, {}, {}, 2},  // 1.7.3
    {"effects.splashes", {}, {}, {}, 2},  // 1.7.3
    {"effects.cave_drips", {}, {}, {}, 2},  // 1.7.3
    {"effects.underwater", {}, {}, {}, 2},  // 1.7.3
    {"effects.materials", {}, {"effects.materials_strength"}, {}, 3},  // 1.8
    {"effects.ground_seams", {}, {}, {}, 3},  // 1.9
    {"effects.full_crowns", {}, {}, {}, 3},  // 1.9.7
    {"effects.ground_clutter", {}, {}, {}, 3},  // 1.9.8
    {"effects.new_trees", {}, {}, {}, 3},  // 1.10
    {"effects.sky_light", {}, {}, {}, 3},  // 1.11
    {"effects.bounce_light", {}, {}, {}, 3},  // 1.11
    {"effects.real_shadows", {}, {}, {}, 3},  // 1.12
    {"effects.volumetric_fog", {}, {"effects.volumetric_fog_density"}, {}, 3},  // 1.13
    {"effects.smooth_hills", {}, {"effects.smooth_hills_strength"}, {}, 3},  // 1.15
    {"effects.world_reflections", {}, {}, {}, 3},  // 1.16
};

}  // namespace

const EffectLayout* effectLayout() { return kEffects; }
int effectLayoutCount() { return int(sizeof kEffects / sizeof kEffects[0]); }

}  // namespace uagfx::settings
