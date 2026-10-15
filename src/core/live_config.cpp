#include "core/live_config.hpp"

#include <utility>

namespace uagfx {
namespace {

const char* logToken(LogLevel l) { return l == LogLevel::Debug ? "verbose" : "normal"; }

std::string change(const char* key, const std::string& was, const std::string& now) {
    return std::string(key) + " " + was + " -> " + now;
}

std::string flag(bool b) { return b ? "1" : "0"; }

}  // namespace

LiveUpdate liveUpdate(const Config& running, const Config& file) {
    LiveUpdate u;
    u.next = running;
    Config& n = u.next;
    if (file.backend != running.backend) u.waiting.push_back("backend");
    if (file.bridge != running.bridge) u.waiting.push_back("bridge");
    if (file.remix != running.remix) u.waiting.push_back("remix");
    if (file.dlss != running.dlss) u.waiting.push_back("dlss");
    if (file.frameGeneration != running.frameGeneration) u.waiting.push_back("frame_generation");
    // [ai]: the network and its models start with the 64-bit renderer.
    const AiSettings& fa = file.ai;
    const AiSettings& ra = running.ai;
    const std::pair<const char*, bool> ai[] = {
        {"ai.enabled", fa.enabled != ra.enabled}, {"ai.ground", fa.ground != ra.ground},
        {"ai.objects", fa.objects != ra.objects}, {"ai.monsters", fa.monsters != ra.monsters},
        {"ai.players", fa.players != ra.players}, {"ai.npcs", fa.npcs != ra.npcs}, {"ai.items", fa.items != ra.items},
        {"ai.effects", fa.effects != ra.effects}, {"ai.icons", fa.icons != ra.icons},
        {"ai.ui_pieces", fa.uiPieces != ra.uiPieces},
        {"ai.model", fa.model != ra.model},
        {"ai.sprite_model", fa.spriteModel != ra.spriteModel}, {"ai.denoise", fa.denoise != ra.denoise},
        {"ai.scale", fa.scale != ra.scale}, {"ai.budget", fa.budget != ra.budget},
        {"ai.min_size", fa.minSize != ra.minSize}, {"ai.max_size", fa.maxSize != ra.maxSize},
        {"ai.ahead", fa.ahead != ra.ahead}, {"ai.generate", fa.generate != ra.generate}};
    for (const auto& k : ai)
        if (k.second) u.waiting.push_back(k.first);
    if (file.mode != running.mode) u.waiting.push_back("mode");
    if (file.highResolution != running.highResolution) {
        u.applied.push_back(change("high_resolution", flag(running.highResolution), flag(file.highResolution)));
        n.highResolution = file.highResolution;
    }
    if (file.uiFilter != running.uiFilter) {
        u.applied.push_back(change("ui_filter", uiFilterToken(running.uiFilter), uiFilterToken(file.uiFilter)));
        n.uiFilter = file.uiFilter;
    }
    if (file.crispText != running.crispText) {
        u.applied.push_back(change("crisp_text", flag(running.crispText), flag(file.crispText)));
        n.crispText = file.crispText;
    }
    if (file.windowArt != running.windowArt) {
        u.applied.push_back(
            change("window_art", windowArtToken(running.windowArt), windowArtToken(file.windowArt)));
        n.windowArt = file.windowArt;
    }
    if (file.monitor != running.monitor) u.waiting.push_back("monitor");
    if (file.windowWidth != running.windowWidth) u.waiting.push_back("window_width");
    if (file.windowHeight != running.windowHeight) u.waiting.push_back("window_height");
    if (file.renderScale != running.renderScale) {
        u.applied.push_back(
            change("render_scale", std::to_string(running.renderScale), std::to_string(file.renderScale)));
        n.renderScale = file.renderScale;
    }
    if (file.sharpen != running.sharpen) {
        u.applied.push_back(change("sharpen", std::to_string(running.sharpen), std::to_string(file.sharpen)));
        n.sharpen = file.sharpen;
    }
    if (file.taa != running.taa) {  // 0.75
        u.applied.push_back(change("taa", flag(running.taa), flag(file.taa)));
        n.taa = file.taa;
    }
    if (file.msaa != running.msaa) {
        u.applied.push_back(change("msaa", std::to_string(running.msaa), std::to_string(file.msaa)));
        n.msaa = file.msaa;
    }
    if (file.alphaToCoverage != running.alphaToCoverage) {
        u.applied.push_back(change("alpha_to_coverage", flag(running.alphaToCoverage), flag(file.alphaToCoverage)));
        n.alphaToCoverage = file.alphaToCoverage;
    }
    if (file.anisotropy != running.anisotropy) {
        u.applied.push_back(
            change("anisotropy", std::to_string(running.anisotropy), std::to_string(file.anisotropy)));
        n.anisotropy = file.anisotropy;
    }
    if (file.vsync != running.vsync) {
        u.applied.push_back(change("vsync", flag(running.vsync), flag(file.vsync)));
        n.vsync = file.vsync;
    }
    if (file.fpsLimit != running.fpsLimit) {
        u.applied.push_back(change("fps_limit", std::to_string(running.fpsLimit), std::to_string(file.fpsLimit)));
        n.fpsLimit = file.fpsLimit;
    }
    if (file.allowTearing != running.allowTearing) u.waiting.push_back("allow_tearing");
    const EffectSettings& fe = file.effects;
    const EffectSettings& re = running.effects;
    if (fe.smoothLight != re.smoothLight)
        u.applied.push_back(change("smooth_light", flag(re.smoothLight), flag(fe.smoothLight)));
    if (fe.torch != re.torch) u.applied.push_back(change("torch", flag(re.torch), flag(fe.torch)));
    if (fe.torchFlame != re.torchFlame)
        u.applied.push_back(change("torch_flame", flag(re.torchFlame), flag(fe.torchFlame)));
    if (fe.smoke != re.smoke) u.applied.push_back(change("smoke", flag(re.smoke), flag(fe.smoke)));
    if (fe.lamps != re.lamps) u.applied.push_back(change("lamps", flag(re.lamps), flag(fe.lamps)));
    if (fe.fireLight != re.fireLight)
        u.applied.push_back(change("fire_light", flag(re.fireLight), flag(fe.fireLight)));
    if (fe.skillColours != re.skillColours)
        u.applied.push_back(change("skill_colours", flag(re.skillColours), flag(fe.skillColours)));
    if (fe.skillsSoft != re.skillsSoft)
        u.applied.push_back(change("skills_soft", flag(re.skillsSoft), flag(fe.skillsSoft)));
    if (fe.aura != re.aura) u.applied.push_back(change("aura", flag(re.aura), flag(fe.aura)));
    if (fe.fireflies != re.fireflies)
        u.applied.push_back(change("fireflies", flag(re.fireflies), flag(fe.fireflies)));
    if (fe.mapGlows != re.mapGlows) u.applied.push_back(change("map_glows", flag(re.mapGlows), flag(fe.mapGlows)));
    if (fe.rays != re.rays) u.applied.push_back(change("rays", flag(re.rays), flag(fe.rays)));
    if (fe.lava != re.lava) u.applied.push_back(change("lava", flag(re.lava), flag(fe.lava)));
    if (fe.actorShadows != re.actorShadows)
        u.applied.push_back(change("actor_shadows", flag(re.actorShadows), flag(fe.actorShadows)));
    if (fe.clouds != re.clouds) u.applied.push_back(change("clouds", flag(re.clouds), flag(fe.clouds)));
    if (fe.haze != re.haze) u.applied.push_back(change("haze", flag(re.haze), flag(fe.haze)));
    if (fe.heat != re.heat) u.applied.push_back(change("heat", flag(re.heat), flag(fe.heat)));
    if (fe.portals != re.portals) u.applied.push_back(change("portals", flag(re.portals), flag(fe.portals)));
    if (fe.weather != re.weather) u.applied.push_back(change("weather", flag(re.weather), flag(fe.weather)));
    if (fe.glow != re.glow) u.applied.push_back(change("glow", flag(re.glow), flag(fe.glow)));
    if (fe.glowStrength != re.glowStrength)
        u.applied.push_back(
            change("glow_strength", std::to_string(re.glowStrength), std::to_string(fe.glowStrength)));
    if (fe.glowSize != re.glowSize)
        u.applied.push_back(change("glow_size", std::to_string(re.glowSize), std::to_string(fe.glowSize)));
    if (fe.ao != re.ao) u.applied.push_back(change("ao", flag(re.ao), flag(fe.ao)));
    if (fe.grade != re.grade) u.applied.push_back(change("grade", flag(re.grade), flag(fe.grade)));
    if (fe.dof != re.dof) u.applied.push_back(change("dof", flag(re.dof), flag(fe.dof)));
    if (fe.water != re.water) u.applied.push_back(change("water", flag(re.water), flag(fe.water)));
    const std::pair<const char*, std::pair<int, int>> numbers[] = {
        {"torch_brightness", {re.torchBrightness, fe.torchBrightness}},
        {"torch_glow", {re.torchGlow, fe.torchGlow}},
        {"torch_light", {re.torchLight, fe.torchLight}},
        {"torch_flicker", {re.torchFlicker, fe.torchFlicker}},
        {"smoke_density", {re.smokeDensity, fe.smokeDensity}},
        {"lamps_light", {re.lampsLight, fe.lampsLight}},
        {"fire_light_strength", {re.fireLightStrength, fe.fireLightStrength}},
        {"aura_brightness", {re.auraBrightness, fe.auraBrightness}},
        {"skill_brightness", {re.skillBrightness, fe.skillBrightness}},
        {"fireflies_brightness", {re.firefliesBrightness, fe.firefliesBrightness}},
        {"map_glows_brightness", {re.mapGlowsBrightness, fe.mapGlowsBrightness}},
        {"rays_brightness", {re.raysBrightness, fe.raysBrightness}},
        {"lava_brightness", {re.lavaBrightness, fe.lavaBrightness}},
        {"actor_shadows_strength", {re.actorShadowsStrength, fe.actorShadowsStrength}},
        {"clouds_strength", {re.cloudsStrength, fe.cloudsStrength}},
        {"haze_strength", {re.hazeStrength, fe.hazeStrength}},
        {"heat_strength", {re.heatStrength, fe.heatStrength}},
        {"portals_strength", {re.portalsStrength, fe.portalsStrength}},
        {"weather_amount", {re.weatherAmount, fe.weatherAmount}},
        {"glow_light", {re.glowLight, fe.glowLight}},
        {"ao_strength", {re.aoStrength, fe.aoStrength}},
        {"ao_size", {re.aoSize, fe.aoSize}},
        {"grade_saturation", {re.gradeSaturation, fe.gradeSaturation}},
        {"grade_contrast", {re.gradeContrast, fe.gradeContrast}},
        {"grade_brightness", {re.gradeBrightness, fe.gradeBrightness}},
        {"grade_warmth", {re.gradeWarmth, fe.gradeWarmth}},
        {"grade_vignette", {re.gradeVignette, fe.gradeVignette}},
        {"dof_strength", {re.dofStrength, fe.dofStrength}},
        {"dof_range", {re.dofRange, fe.dofRange}},
        {"water_waves", {re.waterWaves, fe.waterWaves}},
        {"water_reflections", {re.waterReflections, fe.waterReflections}},
        {"water_clarity", {re.waterClarity, fe.waterClarity}}};
    for (const auto& k : numbers)
        if (k.second.first != k.second.second)
            u.applied.push_back(change(k.first, std::to_string(k.second.first), std::to_string(k.second.second)));
    n.effects = fe;
    if (file.logLevel != running.logLevel) {
        u.applied.push_back(change("log", logToken(running.logLevel), logToken(file.logLevel)));
        n.logLevel = file.logLevel;
    }
    return u;
}

bool appliesNow(const std::string& k) {
    return k == "display.high_resolution" || k == "display.ui_filter" || k == "display.crisp_text" ||
           k == "display.window_art" ||
           k == "quality.render_scale" || k == "quality.sharpen" || k == "quality.msaa" || k == "quality.taa" ||
           k == "quality.alpha_to_coverage" || k == "quality.anisotropy" || k == "frames.vsync" ||
           k == "frames.fps_limit" || k == "debug.log" || k.compare(0, 8, "effects.") == 0;
}

}  // namespace uagfx
