#include "core/live_config.hpp"

#include <utility>

#include "core/settings/schema.hpp"

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
    if (file.bridge != running.bridge) u.waiting.push_back("bridge");
    if (file.remix != running.remix) u.waiting.push_back("remix");
    if (file.dlss != running.dlss) u.waiting.push_back("dlss");
    if (file.frameGeneration != running.frameGeneration) u.waiting.push_back("frame_generation");
    // [ai]: the network and its models start with the 64-bit renderer.
    const AiSettings& fa = file.ai;
    const AiSettings& ra = running.ai;  // as the game started
    // 2.2.2: the kinds shown follow the file at once (aiNow, from what was shown until now): a line of the applied,
    // or of the waiting when the game started without them (aiWaits)
    const AiSettings& wa = running.aiNow;
    const std::pair<const char*, std::pair<bool, bool>> kinds[] = {
        {"ai.enabled", {wa.enabled, fa.enabled}}, {"ai.ground", {wa.ground, fa.ground}},
        {"ai.objects", {wa.objects, fa.objects}}, {"ai.monsters", {wa.monsters, fa.monsters}},
        {"ai.players", {wa.players, fa.players}}, {"ai.npcs", {wa.npcs, fa.npcs}}, {"ai.items", {wa.items, fa.items}},
        {"ai.effects", {wa.effects, fa.effects}}, {"ai.icons", {wa.icons, fa.icons}}};
    for (const auto& [key, was] : kinds) {
        if (was.first == was.second) continue;
        if (aiWaits(key, ra, fa))
            u.waiting.push_back(key);
        else
            u.applied.push_back(change(key, flag(was.first), flag(was.second)));
    }
    const std::pair<const char*, bool> ai[] = {
        {"ai.ui_pieces", fa.uiPieces != ra.uiPieces},
        {"ai.model", fa.model != ra.model},
        {"ai.sprite_model", fa.spriteModel != ra.spriteModel}, {"ai.denoise", fa.denoise != ra.denoise},
        {"ai.scale", fa.scale != ra.scale}, {"ai.budget", fa.budget != ra.budget},
        {"ai.min_size", fa.minSize != ra.minSize}, {"ai.max_size", fa.maxSize != ra.maxSize},
        {"ai.ahead", fa.ahead != ra.ahead}, {"ai.generate", fa.generate != ra.generate},
        {"ai.models", fa.models != ra.models},  // 1.6
        {"ai.small_pictures", fa.smallPictures != ra.smallPictures || fa.smallSide != ra.smallSide ||
                                  fa.smallModel != ra.smallModel}};  // 1.6.3
    for (const auto& k : ai)
        if (k.second) u.waiting.push_back(k.first);
    n.aiNow = file.ai;  // 2.2.1: shown as the file says at once - a kind off hidden now, its pictures kept
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
    if (file.savePower != running.savePower) {  // 0.85
        u.applied.push_back(change("save_power", flag(running.savePower), flag(file.savePower)));
        n.savePower = file.savePower;
    }
    if (file.profile != running.profile) {  // 0.87
        u.applied.push_back(change("profile", flag(running.profile), flag(file.profile)));
        n.profile = file.profile;
    }
    if (file.keepFps != running.keepFps) {  // 0.76
        u.applied.push_back(change("keep_fps", std::to_string(running.keepFps), std::to_string(file.keepFps)));
        n.keepFps = file.keepFps;
    }
    if (file.allowTearing != running.allowTearing) u.waiting.push_back("allow_tearing");
    if (file.statsSend != running.statsSend) {  // 2.2.1: the statistics at once
        u.applied.push_back(change("stats", flag(running.statsSend), flag(file.statsSend)));
        n.statsSend = file.statsSend;
    }
    n.statsId = file.statsId;
    if (file.fpsCounter != running.fpsCounter) {  // 2.2.1: the frame counter at once
        u.applied.push_back(change("fps_counter", flag(running.fpsCounter), flag(file.fpsCounter)));
        n.fpsCounter = file.fpsCounter;
    }
    // 1.9.6: every [effects] key the settings know, by its word in the file - the list written out by hand stopped at
    // 0.33, and a change of a newer one alone (grass, foliage, the sky, the living world...) was not applied until
    // one of the listed changed with it: the console's switches seemed not to work.
    for (const settings::IniSetting& k : settings::iniSettings()) {
        if (std::string(k.section) != "effects") continue;
        const std::string was = settings::tokenOf(k, running), now = settings::tokenOf(k, file);
        if (was != now) u.applied.push_back(change(k.key, was, now));
    }
    n.effects = file.effects;
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
           k == "frames.fps_limit" || k == "frames.keep_fps" || k == "frames.save_power" || k == "debug.log" ||
           k == "debug.profile" || k == "frames.fps_counter" || k == "stats.send" ||
           k.compare(0, 8, "effects.") == 0;
}

}  // namespace uagfx
