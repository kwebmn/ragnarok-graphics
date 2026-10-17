#include "core/settings/schema.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace uagfx::settings {

Text pageName(Page p) {
    switch (p) {
        case Page::Render: return Text::PageRender;
        case Page::Screen: return Text::PageScreen;
        case Page::Quality: return Text::PageQuality;
        case Page::Smoothing: return Text::PageSmoothing;
        case Page::Effects: return Text::PageEffects;
        case Page::Ai: return Text::PageAi;
        case Page::AiCache: return Text::PageAiCache;
        case Page::Pack: return Text::PagePack;
        case Page::Frames: return Text::PageFrames;
        case Page::Sound: return Text::PageSound;
        case Page::Pad: return Text::PagePad;
        case Page::Game: return Text::PageGame;
        case Page::Chat: return Text::PageChat;
        case Page::Diagnostics: return Text::PageDiagnostics;
        case Page::Service: break;
    }
    return Text::PageService;
}

const std::vector<IniSetting>& iniSettings() {
    static const std::vector<IniSetting> list{
        {"renderer", "backend", Page::Render, Control::Choice, 0, 0, 0, {"uaro", "dgvoodoo"},
         {Text::RenderUaro, Text::RenderDgVoodoo}, Text::RenderHeading, Text::RenderUaroHint, ""},
        {"renderer", "api", Page::Render, Control::Choice, 0, 0, 0, {"d3d12", "d3d11", "auto"},
         {Text::RenderApiD3d12, Text::RenderApiD3d11, Text::RenderApiAuto}, Text::RenderApi, Text::RenderApiHint, ""},
        // 0.50.2: a card's name as the Renderer page lists them, or auto (a list of its own there, not a text field)
        {"renderer", "gpu", Page::Render, Control::Line, 0, 0, 0, {}, {}, Text::RenderGpu, Text::RenderGpuHint, ""},
        // 0.51: the Diagnostics page's own check box
        {"report", "auto", Page::Diagnostics, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ReportAuto,
         Text::ReportAutoHint, ""},
        {"renderer", "bridge", Page::Render, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderBridge,
         Text::RenderBridgeHint, ""},
        {"renderer", "remix", Page::Render, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderRemix,
         Text::RenderRemixHint, ""},
        {"display", "high_resolution", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityHighRes,
         Text::QualityHighResHint, ""},
        {"quality", "render_scale", Page::Quality, Control::Slider, 50, 200, 10, {}, {}, Text::QualityRenderScale,
         Text::QualityRenderScaleHint, " %"},
        {"quality", "sharpen", Page::Quality, Control::Slider, 0, 100, 5, {}, {}, Text::QualitySharpen,
         Text::QualitySharpenHint, ""},
        {"display", "ui_filter", Page::Quality, Control::Choice, 0, 0, 0, {"sharp", "smooth"},
         {Text::UiSharp, Text::UiSmooth}, Text::QualityUiFilter, Text::QualityUiFilterHint, ""},
        {"display", "hdr", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityHdr,
         Text::QualityHdrHint, ""},
        {"display", "ui_native", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityUiNative,
         Text::QualityUiNativeHint, ""},
        {"display", "crisp_text", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityCrispText,
         Text::QualityCrispTextHint, ""},
        {"display", "window_art", Page::Quality, Control::Choice, 0, 0, 0, {"xbr", "ai", "pixels"},
         {Text::WindowArtXbr, Text::WindowArtAi, Text::WindowArtPixels}, Text::QualityWindowArt,
         Text::QualityWindowArtHint, ""},
        {"quality", "dlss", Page::Smoothing, Control::Choice, 0, 0, 0,
         {"off", "dlaa", "quality", "balanced", "performance", "ultra"},
         {Text::SmoothOff, Text::DlssDlaa, Text::DlssQuality, Text::DlssBalanced, Text::DlssPerformance,
          Text::DlssUltra},
         Text::SmoothDlss, Text::SmoothDlssHint, ""},
        {"quality", "taa", Page::Smoothing, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::SmoothTaa,
         Text::SmoothTaaHint, ""},
        {"quality", "msaa", Page::Smoothing, Control::Choice, 0, 0, 0, {"0", "2", "4", "8"},
         {Text::SmoothOff, Text::Factor2, Text::Factor4, Text::Factor8}, Text::SmoothMsaa, Text::SmoothMsaaHint, ""},
        {"quality", "alpha_to_coverage", Page::Smoothing, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::SmoothA2c,
         Text::SmoothA2cHint, ""},
        {"quality", "anisotropy", Page::Smoothing, Control::Choice, 0, 0, 0, {"0", "2", "4", "8", "16"},
         {Text::SmoothOff, Text::Factor2, Text::Factor4, Text::Factor8, Text::Factor16}, Text::SmoothAniso,
         Text::SmoothAnisoHint, ""},
        {"effects", "smooth_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSmoothLight,
         Text::FxSmoothLightHint, ""},
        {"effects", "torch", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTorch, Text::FxTorchHint,
         ""},
        {"effects", "torch_flame", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTorchFlame,
         Text::FxTorchFlameHint, ""},
        {"effects", "torch_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxTorchBrightness,
         Text::FxTorchBrightnessHint, " %"},
        {"effects", "torch_glow", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxTorchGlow,
         Text::FxTorchGlowHint, " %"},
        {"effects", "torch_light", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxTorchLight,
         Text::FxTorchLightHint, " %"},
        {"effects", "torch_flicker", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxTorchFlicker,
         Text::FxTorchFlickerHint, " %"},
        {"effects", "smoke", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSmoke, Text::FxSmokeHint,
         ""},
        {"effects", "smoke_density", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxSmokeDensity,
         Text::FxSmokeDensityHint, " %"},
        {"effects", "lamps", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLamps, Text::FxLampsHint,
         ""},
        {"effects", "lamps_light", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxLampsLight,
         Text::FxLampsLightHint, " %"},
        {"effects", "ao", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxAo, Text::FxAoHint, ""},
        {"effects", "ao_strength", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxAoStrength,
         Text::FxAoStrengthHint, " %"},
        {"effects", "ao_size", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxAoSize, Text::FxAoSizeHint,
         " %"},
        {"effects", "glow", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGlow, Text::FxGlowHint,
         ""},
        {"effects", "glow_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxGlowStrength,
         Text::FxGlowStrengthHint, " %"},
        {"effects", "glow_size", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxGlowSize,
         Text::FxGlowSizeHint, " %"},
        {"effects", "glow_light", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxGlowLight,
         Text::FxGlowLightHint, " %"},
        {"effects", "fire_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFireLight,
         Text::FxFireLightHint, ""},
        {"effects", "fire_light_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxFireLightStrength, Text::FxFireLightStrengthHint, " %"},
        {"effects", "skill_colours", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkillColours,
         Text::FxSkillColoursHint, ""},
        {"effects", "aura", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxAura, Text::FxAuraHint,
         ""},
        {"effects", "aura_brightness", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxAuraBrightness,
         Text::FxAuraBrightnessHint, " %"},
        {"effects", "skills_soft", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkillsSoft,
         Text::FxSkillsSoftHint, ""},
        {"effects", "skill_brightness", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxSkillBrightness,
         Text::FxSkillBrightnessHint, " %"},
        {"effects", "fireflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFireflies,
         Text::FxFirefliesHint, ""},
        {"effects", "fireflies_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxFirefliesBrightness, Text::FxFirefliesBrightnessHint, " %"},
        {"effects", "map_glows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMapGlows,
         Text::FxMapGlowsHint, ""},
        {"effects", "map_glows_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxMapGlowsBrightness, Text::FxMapGlowsBrightnessHint, " %"},
        {"effects", "rays", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRays, Text::FxRaysHint,
         ""},
        {"effects", "rays_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxRaysBrightness,
         Text::FxRaysBrightnessHint, " %"},
        {"effects", "lava", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLava, Text::FxLavaHint,
         ""},
        {"effects", "lava_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxLavaBrightness,
         Text::FxLavaBrightnessHint, " %"},
        {"effects", "actor_shadows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxActorShadows,
         Text::FxActorShadowsHint, ""},
        {"effects", "actor_shadows_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {},
         Text::FxActorShadowsStrength, Text::FxActorShadowsStrengthHint, " %"},
        {"effects", "clouds", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxClouds,
         Text::FxCloudsHint, ""},
        {"effects", "clouds_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxCloudsStrength,
         Text::FxCloudsStrengthHint, " %"},
        {"effects", "haze", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxHaze, Text::FxHazeHint,
         ""},
        {"effects", "haze_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxHazeStrength,
         Text::FxHazeStrengthHint, " %"},
        {"effects", "heat", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxHeat, Text::FxHeatHint,
         ""},
        {"effects", "heat_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxHeatStrength,
         Text::FxHeatStrengthHint, " %"},
        {"effects", "portals", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxPortals,
         Text::FxPortalsHint, ""},
        {"effects", "portals_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxPortalsStrength,
         Text::FxPortalsStrengthHint, " %"},
        {"effects", "weather", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWeather,
         Text::FxWeatherHint, ""},
        {"effects", "weather_amount", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxWeatherAmount,
         Text::FxWeatherAmountHint, " %"},
        {"effects", "grade", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGrade, Text::FxGradeHint,
         ""},
        {"effects", "grade_saturation", Page::Effects, Control::Slider, 0, 200, 5, {}, {}, Text::FxGradeSaturation,
         Text::FxGradeSaturationHint, " %"},
        {"effects", "grade_contrast", Page::Effects, Control::Slider, 50, 150, 2, {}, {}, Text::FxGradeContrast,
         Text::FxGradeContrastHint, " %"},
        {"effects", "grade_brightness", Page::Effects, Control::Slider, 50, 150, 2, {}, {}, Text::FxGradeBrightness,
         Text::FxGradeBrightnessHint, " %"},
        {"effects", "grade_warmth", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxGradeWarmth,
         Text::FxGradeWarmthHint, " %"},
        {"effects", "grade_vignette", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxGradeVignette,
         Text::FxGradeVignetteHint, " %"},
        {"effects", "foliage", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFoliage,
         Text::FxFoliageHint, ""},
        {"effects", "foliage_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxFoliageStrength,
         Text::FxFoliageStrengthHint, " %"},
        {"effects", "soft_particles", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSoftParticles,
         Text::FxSoftParticlesHint, ""},
        {"effects", "rim_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRimLight,
         Text::FxRimLightHint, ""},
        {"effects", "rim_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxRimStrength,
         Text::FxRimStrengthHint, " %"},
        {"effects", "sky", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSky,
         Text::FxSkyHint, ""},
        {"effects", "sky_clouds", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxSkyClouds,
         Text::FxSkyCloudsHint, " %"},
        {"effects", "sky_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxSkyStrength,
         Text::FxSkyStrengthHint, " %"},
        {"effects", "motes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMotes,
         Text::FxMotesHint, ""},
        {"effects", "motes_amount", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxMotesAmount,
         Text::FxMotesAmountHint, " %"},
        {"effects", "relief", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRelief,
         Text::FxReliefHint, ""},
        {"effects", "relief_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxReliefStrength,
         Text::FxReliefStrengthHint, " %"},
        {"effects", "floor_reflections", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxFloorReflections, Text::FxFloorReflectionsHint, ""},
        {"effects", "floor_reflect_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {},
         Text::FxFloorReflectStrength, Text::FxFloorReflectStrengthHint, " %"},
        {"effects", "shafts", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxShafts,
         Text::FxShaftsHint, ""},
        {"effects", "shafts_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxShaftsStrength,
         Text::FxShaftsStrengthHint, " %"},
        {"effects", "day_cycle", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDayCycle,
         Text::FxDayCycleHint, ""},
        {"effects", "sun_shadows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSunShadows,
         Text::FxSunShadowsHint, ""},
        {"effects", "sun_shadows_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {},
         Text::FxSunShadowsStrength, Text::FxSunShadowsStrengthHint, " %"},
        {"effects", "bloom", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxBloom,
         Text::FxBloomHint, ""},
        {"effects", "bloom_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxBloomStrength,
         Text::FxBloomStrengthHint, " %"},
        {"effects", "filmic", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFilmic,
         Text::FxFilmicHint, ""},
        {"effects", "region_looks", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRegionLooks,
         Text::FxRegionLooksHint, ""},
        {"effects", "region_looks_strength", Page::Effects, Control::Slider, 0, 100, 10, {}, {},
         Text::FxRegionLooksStrength, Text::FxRegionLooksStrengthHint, " %"},
        {"effects", "footprints", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFootprints,
         Text::FxFootprintsHint, ""},
        {"effects", "storms", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxStorms,
         Text::FxStormsHint, ""},
        {"effects", "grass", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGrass,
         Text::FxGrassHint, ""},
        {"effects", "grass_amount", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxGrassAmount,
         Text::FxGrassAmountHint, " %"},
        {"effects", "skill_sparks", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkillSparks,
         Text::FxSkillSparksHint, ""},
        {"effects", "skill_sparks_amount", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxSkillSparksAmount, Text::FxSkillSparksAmountHint, " %"},
        {"effects", "film_grain", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFilmGrain,
         Text::FxFilmGrainHint, ""},
        {"effects", "film_grain_amount", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxFilmGrainAmount,
         Text::FxFilmGrainAmountHint, " %"},
        {"effects", "map_fade", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMapFade,
         Text::FxMapFadeHint, ""},
        {"effects", "round_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRoundLight,
         Text::FxRoundLightHint, ""},
        {"effects", "appear", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxAppear,
         Text::FxAppearHint, ""},
        {"effects", "ripples", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRipples,
         Text::FxRipplesHint, ""},
        {"effects", "glint", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGlint,
         Text::FxGlintHint, ""},
        {"effects", "hit_flash", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxHitFlash,
         Text::FxHitFlashHint, ""},
        {"effects", "wet_ground", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWetGround,
         Text::FxWetGroundHint, ""},
        {"effects", "flare", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFlare,
         Text::FxFlareHint, ""},
        {"effects", "tree_volume", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTreeVolume,
         Text::FxTreeVolumeHint, ""},
        {"effects", "leaf_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLeafLight,
         Text::FxLeafLightHint, ""},
        {"effects", "tree_tints", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTreeTints,
         Text::FxTreeTintsHint, ""},
        {"effects", "gusts", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGusts,
         Text::FxGustsHint, ""},
        {"effects", "dapple", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDapple,
         Text::FxDappleHint, ""},
        {"effects", "flowers", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFlowers,
         Text::FxFlowersHint, ""},
        {"effects", "dew", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDew,
         Text::FxDewHint, ""},
        {"effects", "morning_fog", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMorningFog,
         Text::FxMorningFogHint, ""},
        {"effects", "sand_drift", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSandDrift,
         Text::FxSandDriftHint, ""},
        {"effects", "snow_sparkle", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSnowSparkle,
         Text::FxSnowSparkleHint, ""},
        {"effects", "wet_walls", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWetWalls,
         Text::FxWetWallsHint, ""},
        {"effects", "shore_foam", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxShoreFoam,
         Text::FxShoreFoamHint, ""},
        {"effects", "fish", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFish,
         Text::FxFishHint, ""},
        {"effects", "swamp", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSwamp,
         Text::FxSwampHint, ""},
        {"effects", "butterflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxButterflies,
         Text::FxButterfliesHint, ""},
        {"effects", "birds", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxBirds,
         Text::FxBirdsHint, ""},
        {"effects", "leaf_fall", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLeafFall,
         Text::FxLeafFallHint, ""},
        {"effects", "embers", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxEmbers,
         Text::FxEmbersHint, ""},
        {"effects", "breath", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxBreath,
         Text::FxBreathHint, ""},
        {"effects", "splashes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSplashes,
         Text::FxSplashesHint, ""},
        {"effects", "cave_drips", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxCaveDrips,
         Text::FxCaveDripsHint, ""},
        {"effects", "underwater", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxUnderwater,
         Text::FxUnderwaterHint, ""},
        {"effects", "tier", Page::Effects, Control::Choice, 0, 0, 0, {"auto", "classic", "remaster"},
         {Text::FxTierAuto, Text::FxTierClassic, Text::FxTierRemaster}, Text::FxTier, Text::FxTierHint, ""},
        {"effects", "materials", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMaterials,
         Text::FxMaterialsHint, ""},
        {"effects", "materials_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxMaterialsStrength,
         Text::FxMaterialsStrengthHint, " %"},
        {"effects", "ground_seams", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroundSeams,
         Text::FxGroundSeamsHint, ""},
        {"effects", "full_crowns", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFullCrowns,
         Text::FxFullCrownsHint, ""},
        {"effects", "night", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxNight, Text::FxNightHint,
         ""},
        {"effects", "night_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxNightStrength,
         Text::FxNightStrengthHint, " %"},
        {"effects", "night_clock", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxNightClock,
         Text::FxNightClockHint, ""},
        {"effects", "dof", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDof, Text::FxDofHint, ""},
        {"effects", "dof_strength", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxDofStrength,
         Text::FxDofStrengthHint, " %"},
        {"effects", "dof_range", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxDofRange,
         Text::FxDofRangeHint, " %"},
        {"effects", "water", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWater, Text::FxWaterHint,
         ""},
        {"effects", "water_waves", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxWaterWaves,
         Text::FxWaterWavesHint, " %"},
        {"effects", "water_reflections", Page::Effects, Control::Slider, 0, 100, 10, {}, {},
         Text::FxWaterReflections, Text::FxWaterReflectionsHint, " %"},
        {"effects", "water_clarity", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxWaterClarity,
         Text::FxWaterClarityHint, " %"},
        {"ai", "enabled", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiEnable, Text::AiEnableHint, ""},
        {"ai", "ground", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiGround, Text::AiGroundHint, ""},
        {"ai", "objects", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiObjects, Text::AiObjectsHint,
         ""},
        {"ai", "monsters", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiSprites, Text::AiSpritesHint,
         ""},
        {"ai", "players", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiPlayers, Text::AiPlayersHint, ""},
        {"ai", "npcs", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiNpcs, Text::AiNpcsHint, ""},
        {"ai", "items", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiItems, Text::AiItemsHint, ""},
        {"ai", "effects", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiEffects, Text::AiEffectsHint,
         ""},
        {"ai", "icons", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiIcons, Text::AiIconsHint, ""},
        {"ai", "ui_pieces", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiUiPieces, Text::AiUiPiecesHint,
         ""},
        {"ai", "model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b},
         Text::AiModelWorld, Text::AiModelWorldHint, ""},
        {"ai", "sprite_model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b},
         Text::AiModelSprites, Text::AiModelSpritesHint, ""},
        // 1.6.3: small pictures by a model of their own
        {"ai", "small_pictures", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiSmall, Text::AiSmallHint,
         ""},
        {"ai", "small_side", Page::Ai, Control::Number, 16, 256, 1, {}, {}, Text::AiSmallSide, Text::AiSmallHint, ""},
        {"ai", "small_model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b}, Text::AiSmallModel, Text::AiSmallHint, ""},
        // 1.6: a model for each group of pictures
        {"ai", "model_world", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b}, Text::AiGroupWorld,
         Text::AiGroupModelHint, ""},
        {"ai", "model_monsters", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b}, Text::AiGroupMonsters,
         Text::AiGroupModelHint, ""},
        {"ai", "model_players", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b}, Text::AiGroupPlayers,
         Text::AiGroupModelHint, ""},
        {"ai", "model_npcs", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b}, Text::AiGroupNpcs,
         Text::AiGroupModelHint, ""},
        {"ai", "model_items", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b}, Text::AiGroupItems,
         Text::AiGroupModelHint, ""},
        {"ai", "model_effects", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b}, Text::AiGroupEffects,
         Text::AiGroupModelHint, ""},
        {"ai", "model_icons", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b}, Text::AiGroupIcons,
         Text::AiGroupModelHint, ""},
        {"ai", "model_screens", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b}, Text::AiGroupScreens,
         Text::AiGroupModelHint, ""},
        {"ai", "denoise", Page::Ai, Control::Slider, 0, 100, 5, {}, {}, Text::AiDenoise, Text::AiDenoiseHint, " %"},
        {"ai", "scale", Page::Ai, Control::Choice, 0, 0, 0, {"4", "2"}, {Text::AiScale4, Text::AiScale2},
         Text::AiScale, Text::AiScaleHint, ""},
        {"ai", "budget", Page::Ai, Control::Number, 1, 20, 1, {}, {}, Text::AiBudget, Text::AiBudgetHint, ""},
        {"ai", "min_size", Page::Ai, Control::Number, 16, 1024, 1, {}, {}, Text::AiSizes, Text::AiSizesHint, ""},
        {"ai", "max_size", Page::Ai, Control::Number, 16, 1024, 1, {}, {}, Text::AiSizes, Text::AiSizesHint, ""},
        {"ai", "compress", Page::Ai, Control::Choice, 0, 0, 0, {"auto", "on", "off"},
         {Text::AiCompressAuto, Text::AiCompressOn, Text::AiCompressOff}, Text::AiCompress, Text::AiCompressHint, ""},
        {"ai", "ahead", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiAhead, Text::AiAheadHint, ""},
        {"ai", "generate", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiGenerate, Text::AiGenerateHint,
         ""},
        {"frames", "vsync", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesVsync,
         Text::FramesVsyncHint, ""},
        {"frames", "frame_generation", Page::Frames, Control::Choice, 0, 0, 0, {"off", "2x", "3x", "4x"},
         {Text::SmoothOff, Text::Factor2, Text::Times3, Text::Factor4}, Text::FramesGeneration,
         Text::FramesGenerationHint, ""},
        {"frames", "fps_counter", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesFpsCounter,
         Text::FramesFpsCounterHint, ""},
        {"frames", "fps_limit", Page::Frames, Control::Number, 0, 1000, 1, {}, {}, Text::FramesFpsLimit,
         Text::FramesFpsLimitHint, ""},
        {"frames", "save_power", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesSavePower,
         Text::FramesSavePowerHint, ""},
        {"frames", "keep_fps", Page::Frames, Control::Number, 0, 240, 1, {}, {}, Text::FramesKeepFps,
         Text::FramesKeepFpsHint, ""},
        {"frames", "allow_tearing", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesTearing,
         Text::FramesTearingHint, ""},
        // 0.91: the game's sound through the driver
        {"sound", "rate", Page::Sound, Control::Choice, 0, 0, 0, {"0", "44100", "48000"},
         {Text::GfxSoundRateGame, Text::GfxSoundRate44, Text::GfxSoundRate48}, Text::GfxSoundRate,
         Text::GfxSoundRateHint, ""},
        {"sound", "voices", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundVoices,
         Text::GfxSoundVoicesHint, ""},
        {"sound", "background", Page::Sound, Control::Slider, 0, 100, 5, {}, {}, Text::GfxSoundBackground,
         Text::GfxSoundBackgroundHint, "%"},
        {"sound", "limiter", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundLimiter,
         Text::GfxSoundLimiterHint, ""},
        {"sound", "stereo", Page::Sound, Control::Slider, 0, 100, 5, {}, {}, Text::GfxSoundStereo,
         Text::GfxSoundStereoHint, "%"},  // 0.92
        {"sound", "steal", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundSteal,
         Text::GfxSoundStealHint, ""},
        {"sound", "crossfade", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundCrossfade,
         Text::GfxSoundCrossfadeHint, ""},
        {"sound", "engine", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundEngine,
         Text::GfxSoundEngineHint, ""},  // 1.1
        {"sound", "reverb", Page::Sound, Control::Slider, 0, 100, 5, {}, {}, Text::GfxSoundReverb,
         Text::GfxSoundReverbHint, "%"},
        {"sound", "headphones", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundHeadphones,
         Text::GfxSoundHeadphonesHint, ""},  // 1.2
        {"sound", "duck", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundDuck,
         Text::GfxSoundDuckHint, ""},  // 1.3
        {"debug", "profile", Page::Diagnostics, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DiagProfile,
         Text::DiagProfileHint, ""},
        {"debug", "log", Page::Diagnostics, Control::Check, 0, 1, 1, {"normal", "verbose"}, {}, Text::DiagVerboseLog,
         Text::DiagVerboseLogHint, ""},
    };
    return list;
}

const IniSetting* findSetting(const std::string& sectionKey) {
    for (const IniSetting& s : iniSettings())
        if (sectionKey == std::string(s.section) + "." + s.key) return &s;
    return nullptr;
}

std::string sectionKey(const IniSetting& s) { return std::string(s.section) + "." + s.key; }

std::string tokenOf(const IniSetting& s, const Config& c) {
    const std::string k = sectionKey(s);
    if (k == "renderer.backend") return backendToken(c.backend);
    if (k == "renderer.api") return apiToken(c.api);
    if (k == "renderer.gpu") return c.gpu.empty() ? "auto" : c.gpu;
    if (k == "report.auto") return c.reportAuto ? "1" : "0";
    if (k == "renderer.bridge") return c.bridge ? "1" : "0";
    if (k == "renderer.remix") return c.remix ? "1" : "0";
    if (k == "display.high_resolution") return c.highResolution ? "1" : "0";
    if (k == "quality.render_scale") return std::to_string(c.renderScale);
    if (k == "quality.sharpen") return std::to_string(c.sharpen);
    if (k == "display.ui_filter") return uiFilterToken(c.uiFilter);
    if (k == "display.window_art") return windowArtToken(c.windowArt);
    if (k == "display.crisp_text") return c.crispText ? "1" : "0";
    if (k == "quality.dlss") return c.dlss;
    if (k == "quality.msaa") return std::to_string(c.msaa);
    if (k == "quality.taa") return c.taa ? "1" : "0";
    if (k == "quality.alpha_to_coverage") return c.alphaToCoverage ? "1" : "0";
    const AiSettings& a = c.ai;
    if (k == "ai.enabled") return a.enabled ? "1" : "0";
    if (k == "ai.ground") return a.ground ? "1" : "0";
    if (k == "ai.objects") return a.objects ? "1" : "0";
    if (k == "ai.monsters") return a.monsters ? "1" : "0";
    if (k == "ai.players") return a.players ? "1" : "0";
    if (k == "ai.npcs") return a.npcs ? "1" : "0";
    if (k == "ai.items") return a.items ? "1" : "0";
    if (k == "ai.effects") return a.effects ? "1" : "0";
    if (k == "ai.icons") return a.icons ? "1" : "0";
    if (k == "ai.ui_pieces") return a.uiPieces ? "1" : "0";
    if (k == "ai.model") return a.model;
    if (k == "ai.sprite_model") return a.spriteModel;
    if (k == "ai.small_pictures") return a.smallPictures ? "1" : "0";  // 1.6.3
    if (k == "ai.small_side") return std::to_string(a.smallSide);
    if (k == "ai.small_model") return a.smallModel;
    if (k.rfind("ai.model_", 0) == 0) {  // 1.6: a group's, as it comes out (its own, or model / sprite_model)
        ai::Group g;
        if (ai::parseGroup(k.substr(9), g)) return a.modelFor(g);
    }
    if (k == "ai.denoise") return std::to_string(a.denoise);
    if (k == "ai.scale") return std::to_string(a.scale);
    if (k == "ai.budget") return std::to_string(a.budget);
    if (k == "ai.min_size") return std::to_string(a.minSize);
    if (k == "ai.max_size") return std::to_string(a.maxSize);
    if (k == "ai.ahead") return a.ahead ? "1" : "0";
    if (k == "ai.compress") return a.compress;
    if (k == "ai.generate") return a.generate ? "1" : "0";
    if (k == "quality.anisotropy") return std::to_string(c.anisotropy);
    if (k == "effects.smooth_light") return c.effects.smoothLight ? "1" : "0";
    if (k == "effects.torch") return c.effects.torch ? "1" : "0";
    if (k == "effects.torch_flame") return c.effects.torchFlame ? "1" : "0";
    if (k == "effects.torch_brightness") return std::to_string(c.effects.torchBrightness);
    if (k == "effects.torch_glow") return std::to_string(c.effects.torchGlow);
    if (k == "effects.torch_light") return std::to_string(c.effects.torchLight);
    if (k == "effects.torch_flicker") return std::to_string(c.effects.torchFlicker);
    if (k == "effects.smoke") return c.effects.smoke ? "1" : "0";
    if (k == "effects.smoke_density") return std::to_string(c.effects.smokeDensity);
    if (k == "effects.fire_light") return c.effects.fireLight ? "1" : "0";
    if (k == "effects.fire_light_strength") return std::to_string(c.effects.fireLightStrength);
    if (k == "effects.skill_colours") return c.effects.skillColours ? "1" : "0";
    if (k == "effects.aura") return c.effects.aura ? "1" : "0";
    if (k == "effects.aura_brightness") return std::to_string(c.effects.auraBrightness);
    if (k == "effects.skills_soft") return c.effects.skillsSoft ? "1" : "0";
    if (k == "effects.skill_brightness") return std::to_string(c.effects.skillBrightness);
    if (k == "effects.fireflies") return c.effects.fireflies ? "1" : "0";
    if (k == "effects.fireflies_brightness") return std::to_string(c.effects.firefliesBrightness);
    if (k == "effects.map_glows") return c.effects.mapGlows ? "1" : "0";
    if (k == "effects.map_glows_brightness") return std::to_string(c.effects.mapGlowsBrightness);
    if (k == "effects.rays") return c.effects.rays ? "1" : "0";
    if (k == "effects.rays_brightness") return std::to_string(c.effects.raysBrightness);
    if (k == "effects.lava") return c.effects.lava ? "1" : "0";
    if (k == "effects.lava_brightness") return std::to_string(c.effects.lavaBrightness);
    if (k == "effects.actor_shadows") return c.effects.actorShadows ? "1" : "0";
    if (k == "effects.actor_shadows_strength") return std::to_string(c.effects.actorShadowsStrength);
    if (k == "effects.clouds") return c.effects.clouds ? "1" : "0";
    if (k == "effects.clouds_strength") return std::to_string(c.effects.cloudsStrength);
    if (k == "effects.haze") return c.effects.haze ? "1" : "0";
    if (k == "effects.haze_strength") return std::to_string(c.effects.hazeStrength);
    if (k == "effects.heat") return c.effects.heat ? "1" : "0";
    if (k == "effects.heat_strength") return std::to_string(c.effects.heatStrength);
    if (k == "effects.portals") return c.effects.portals ? "1" : "0";
    if (k == "effects.portals_strength") return std::to_string(c.effects.portalsStrength);
    if (k == "effects.weather") return c.effects.weather ? "1" : "0";
    if (k == "effects.weather_amount") return std::to_string(c.effects.weatherAmount);
    if (k == "effects.lamps") return c.effects.lamps ? "1" : "0";
    if (k == "effects.lamps_light") return std::to_string(c.effects.lampsLight);
    if (k == "effects.glow") return c.effects.glow ? "1" : "0";
    if (k == "effects.glow_strength") return std::to_string(c.effects.glowStrength);
    if (k == "effects.glow_size") return std::to_string(c.effects.glowSize);
    if (k == "effects.glow_light") return std::to_string(c.effects.glowLight);
    if (k == "effects.ao") return c.effects.ao ? "1" : "0";
    if (k == "effects.ao_strength") return std::to_string(c.effects.aoStrength);
    if (k == "effects.ao_size") return std::to_string(c.effects.aoSize);
    if (k == "effects.grade") return c.effects.grade ? "1" : "0";
    if (k == "effects.grade_saturation") return std::to_string(c.effects.gradeSaturation);
    if (k == "effects.grade_contrast") return std::to_string(c.effects.gradeContrast);
    if (k == "effects.grade_brightness") return std::to_string(c.effects.gradeBrightness);
    if (k == "effects.grade_warmth") return std::to_string(c.effects.gradeWarmth);
    if (k == "effects.grade_vignette") return std::to_string(c.effects.gradeVignette);
    if (k == "effects.soft_particles") return c.effects.softParticles ? "1" : "0";
    if (k == "effects.rim_light") return c.effects.rimLight ? "1" : "0";
    if (k == "effects.rim_strength") return std::to_string(c.effects.rimStrength);
    if (k == "effects.sky") return c.effects.sky ? "1" : "0";
    if (k == "effects.sky_clouds") return std::to_string(c.effects.skyClouds);
    if (k == "effects.sky_strength") return std::to_string(c.effects.skyStrength);
    if (k == "effects.motes") return c.effects.motes ? "1" : "0";
    if (k == "effects.motes_amount") return std::to_string(c.effects.motesAmount);
    if (k == "effects.relief") return c.effects.relief ? "1" : "0";
    if (k == "effects.relief_strength") return std::to_string(c.effects.reliefStrength);
    if (k == "effects.floor_reflections") return c.effects.floorReflections ? "1" : "0";
    if (k == "effects.floor_reflect_strength") return std::to_string(c.effects.floorReflectStrength);
    if (k == "effects.shafts") return c.effects.shafts ? "1" : "0";
    if (k == "effects.shafts_strength") return std::to_string(c.effects.shaftsStrength);
    if (k == "effects.day_cycle") return c.effects.dayCycle ? "1" : "0";
    if (k == "effects.sun_shadows") return c.effects.sunShadows ? "1" : "0";
    if (k == "effects.sun_shadows_strength") return std::to_string(c.effects.sunShadowsStrength);
    if (k == "effects.bloom") return c.effects.bloom ? "1" : "0";
    if (k == "effects.bloom_strength") return std::to_string(c.effects.bloomStrength);
    if (k == "effects.filmic") return c.effects.filmic ? "1" : "0";
    if (k == "effects.region_looks") return c.effects.regionLooks ? "1" : "0";
    if (k == "effects.region_looks_strength") return std::to_string(c.effects.regionLooksStrength);
    if (k == "effects.footprints") return c.effects.footprints ? "1" : "0";
    if (k == "effects.storms") return c.effects.storms ? "1" : "0";
    if (k == "effects.grass") return c.effects.grass ? "1" : "0";
    if (k == "effects.grass_amount") return std::to_string(c.effects.grassAmount);
    if (k == "effects.skill_sparks") return c.effects.skillSparks ? "1" : "0";
    if (k == "effects.skill_sparks_amount") return std::to_string(c.effects.skillSparksAmount);
    if (k == "effects.film_grain") return c.effects.filmGrain ? "1" : "0";
    if (k == "effects.film_grain_amount") return std::to_string(c.effects.filmGrainAmount);
    if (k == "effects.map_fade") return c.effects.mapFade ? "1" : "0";
    if (k == "effects.round_light") return c.effects.roundLight ? "1" : "0";
    if (k == "effects.appear") return c.effects.appear ? "1" : "0";
    if (k == "effects.ripples") return c.effects.ripples ? "1" : "0";
    if (k == "effects.glint") return c.effects.glint ? "1" : "0";
    if (k == "effects.hit_flash") return c.effects.hitFlash ? "1" : "0";
    if (k == "effects.wet_ground") return c.effects.wetGround ? "1" : "0";
    if (k == "effects.flare") return c.effects.flare ? "1" : "0";
    if (k == "effects.tree_volume") return c.effects.treeVolume ? "1" : "0";
    if (k == "effects.leaf_light") return c.effects.leafLight ? "1" : "0";
    if (k == "effects.tree_tints") return c.effects.treeTints ? "1" : "0";
    if (k == "effects.gusts") return c.effects.gusts ? "1" : "0";
    if (k == "effects.dapple") return c.effects.dapple ? "1" : "0";
    if (k == "effects.flowers") return c.effects.flowers ? "1" : "0";
    if (k == "effects.dew") return c.effects.dew ? "1" : "0";
    if (k == "effects.morning_fog") return c.effects.morningFog ? "1" : "0";
    if (k == "effects.sand_drift") return c.effects.sandDrift ? "1" : "0";
    if (k == "effects.snow_sparkle") return c.effects.snowSparkle ? "1" : "0";
    if (k == "effects.wet_walls") return c.effects.wetWalls ? "1" : "0";
    if (k == "effects.shore_foam") return c.effects.shoreFoam ? "1" : "0";
    if (k == "effects.fish") return c.effects.fish ? "1" : "0";
    if (k == "effects.swamp") return c.effects.swamp ? "1" : "0";
    if (k == "effects.butterflies") return c.effects.butterflies ? "1" : "0";
    if (k == "effects.birds") return c.effects.birds ? "1" : "0";
    if (k == "effects.leaf_fall") return c.effects.leafFall ? "1" : "0";
    if (k == "effects.embers") return c.effects.embers ? "1" : "0";
    if (k == "effects.breath") return c.effects.breath ? "1" : "0";
    if (k == "effects.splashes") return c.effects.splashes ? "1" : "0";
    if (k == "effects.cave_drips") return c.effects.caveDrips ? "1" : "0";
    if (k == "effects.underwater") return c.effects.underwater ? "1" : "0";
    if (k == "effects.tier") return c.effects.tier == 1 ? "classic" : c.effects.tier == 2 ? "remaster" : "auto";
    if (k == "effects.materials") return c.effects.materials ? "1" : "0";
    if (k == "effects.materials_strength") return std::to_string(c.effects.materialsStrength);
    if (k == "effects.ground_seams") return c.effects.groundSeams ? "1" : "0";
    if (k == "effects.full_crowns") return c.effects.fullCrowns ? "1" : "0";
    if (k == "effects.night") return c.effects.night ? "1" : "0";
    if (k == "effects.foliage") return c.effects.foliage ? "1" : "0";
    if (k == "effects.foliage_strength") return std::to_string(c.effects.foliageStrength);
    if (k == "effects.night_strength") return std::to_string(c.effects.nightStrength);
    if (k == "effects.night_clock") return c.effects.nightClock ? "1" : "0";
    if (k == "effects.dof") return c.effects.dof ? "1" : "0";
    if (k == "effects.dof_strength") return std::to_string(c.effects.dofStrength);
    if (k == "effects.dof_range") return std::to_string(c.effects.dofRange);
    if (k == "effects.water") return c.effects.water ? "1" : "0";
    if (k == "effects.water_waves") return std::to_string(c.effects.waterWaves);
    if (k == "effects.water_reflections") return std::to_string(c.effects.waterReflections);
    if (k == "effects.water_clarity") return std::to_string(c.effects.waterClarity);
    if (k == "frames.vsync") return c.vsync ? "1" : "0";
    if (k == "frames.fps_limit") return std::to_string(c.fpsLimit);
    if (k == "frames.keep_fps") return std::to_string(c.keepFps);
    if (k == "debug.profile") return c.profile ? "1" : "0";
    if (k == "frames.save_power") return c.savePower ? "1" : "0";
    if (k == "frames.fps_counter") return c.fpsCounter ? "1" : "0";
    if (k == "display.ui_native") return c.uiNative ? "1" : "0";
    if (k == "display.hdr") return c.hdr ? "1" : "0";
    if (k == "frames.frame_generation") return c.frameGeneration ? std::to_string(c.frameGeneration) + "x" : "off";
    if (k == "frames.allow_tearing") return c.allowTearing ? "1" : "0";
    if (k == "sound.rate") return std::to_string(c.sound.rate);  // 0.91
    if (k == "sound.voices") return c.sound.voices ? "1" : "0";
    if (k == "sound.background") return std::to_string(c.sound.background);
    if (k == "sound.limiter") return c.sound.limiter ? "1" : "0";
    if (k == "sound.stereo") return std::to_string(c.sound.stereo);  // 0.92
    if (k == "sound.steal") return c.sound.steal ? "1" : "0";
    if (k == "sound.crossfade") return c.sound.crossfade ? "1" : "0";
    if (k == "sound.engine") return c.sound.engine ? "1" : "0";  // 1.1
    if (k == "sound.reverb") return std::to_string(c.sound.reverb);
    if (k == "sound.headphones") return c.sound.headphones ? "1" : "0";  // 1.2
    if (k == "sound.duck") return c.sound.duck ? "1" : "0";              // 1.3
    if (k == "debug.log") return c.logLevel == LogLevel::Debug ? "verbose" : "normal";
    return "";
}

bool validToken(const IniSetting& s, const std::string& token) {
    if (s.control == Control::Line)  // 0.50.2: a graphics card's name - one line, no comment or section in it
        return !token.empty() && token.find_first_of("\r\n;[") == std::string::npos;
    if (s.control == Control::Slider || s.control == Control::Number) {
        char* end = nullptr;
        const long n = std::strtol(token.c_str(), &end, 10);
        return !token.empty() && *end == '\0' && n >= s.min && n <= s.max;
    }
    return std::any_of(s.tokens.begin(), s.tokens.end(), [&](const char* t) { return token == t; });
}

}  // namespace uagfx::settings
