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
    if (file.reflex != running.reflex) u.waiting.push_back("reflex");  // 2.5.26: chosen at the renderer's start
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
        {"ai.effects", {wa.effects, fa.effects}}, {"ai.icons", {wa.icons, fa.icons}},
        {"ai.dyes", {wa.dyes, fa.dyes}}};  // 2.5.23
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
        {"ai.models", fa.models != ra.models || fa.familyModels != ra.familyModels},  // 1.6; 2.5.28: families
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
    if (file.worldScale != running.worldScale) {  // 2.5.16
        u.applied.push_back(
            change("world_scale", std::to_string(running.worldScale), std::to_string(file.worldScale)));
        n.worldScale = file.worldScale;
    }
    if (file.superLight != running.superLight) {  // 2.5.20
        u.applied.push_back(change("super_light", running.superLight ? "1" : "0", file.superLight ? "1" : "0"));
        n.superLight = file.superLight;
    }
    if (file.batterySaver != running.batterySaver) {
        u.applied.push_back(change("battery_saver", flag(running.batterySaver), flag(file.batterySaver)));
        n.batterySaver = file.batterySaver;
    }
    if (file.batteryFps != running.batteryFps) {
        u.applied.push_back(
            change("battery_fps", std::to_string(running.batteryFps), std::to_string(file.batteryFps)));
        n.batteryFps = file.batteryFps;
    }
    if (file.fullColour != running.fullColour) {  // 2.5.18: for the textures loaded from now on
        u.applied.push_back(change("full_colour", running.fullColour ? "1" : "0", file.fullColour ? "1" : "0"));
        n.fullColour = file.fullColour;
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
    if (file.windowTitleName != running.windowTitleName) {  // 3.0.3-beta.2
        u.applied.push_back(change("title_name", flag(running.windowTitleName), flag(file.windowTitleName)));
        n.windowTitleName = file.windowTitleName;
    }
    if (file.windowRememberPlace != running.windowRememberPlace) {
        u.applied.push_back(
            change("remember_place", flag(running.windowRememberPlace), flag(file.windowRememberPlace)));
        n.windowRememberPlace = file.windowRememberPlace;
    }
    if (file.windowOthersLight != running.windowOthersLight) {  // 3.0.3-beta.3
        u.applied.push_back(change("others_light", flag(running.windowOthersLight), flag(file.windowOthersLight)));
        n.windowOthersLight = file.windowOthersLight;
    }
    if (file.windowOthersMusic != running.windowOthersMusic) {
        u.applied.push_back(change("others_music", flag(running.windowOthersMusic), flag(file.windowOthersMusic)));
        n.windowOthersMusic = file.windowOthersMusic;
    }
    if (file.windowOthersSounds != running.windowOthersSounds) {  // 3.0.3-beta.9
        u.applied.push_back(
            change("others_sounds", flag(running.windowOthersSounds), flag(file.windowOthersSounds)));
        n.windowOthersSounds = file.windowOthersSounds;
    }
    if (file.windowKeys != running.windowKeys) {  // 3.0.3-beta.6
        u.applied.push_back(
            change("switch_keys", windowKeysToken(running.windowKeys), windowKeysToken(file.windowKeys)));
        n.windowKeys = file.windowKeys;
    }
    if (file.newMinimap != running.newMinimap) {  // 3.0.11-beta.1: the minimap prototype, on and off at once
        u.applied.push_back(change("minimap", flag(running.newMinimap), flag(file.newMinimap)));
        n.newMinimap = file.newMinimap;
    }
    if (file.panelShow != running.panelShow) {  // 3.0.3-beta.8: the game windows' panel
        u.applied.push_back(change("show", flag(running.panelShow), flag(file.panelShow)));
        n.panelShow = file.panelShow;
    }
    if (file.panelHidden != running.panelHidden) {  // another game's Ctrl+Alt+0
        u.applied.push_back(change("hidden", flag(running.panelHidden), flag(file.panelHidden)));
        n.panelHidden = file.panelHidden;
    }
    if (file.panelLayout != running.panelLayout) {
        u.applied.push_back(
            change("layout", panelLayoutToken(running.panelLayout), panelLayoutToken(file.panelLayout)));
        n.panelLayout = file.panelLayout;
    }
    if (file.panelSize != running.panelSize) {
        u.applied.push_back(change("size", std::to_string(running.panelSize), std::to_string(file.panelSize)));
        n.panelSize = file.panelSize;
    }
    if (file.panelOpacity != running.panelOpacity) {
        u.applied.push_back(
            change("opacity", std::to_string(running.panelOpacity), std::to_string(file.panelOpacity)));
        n.panelOpacity = file.panelOpacity;
    }
    if (file.panelPlace != running.panelPlace) {  // another game's panel dragged
        u.applied.push_back(change("place", running.panelPlace, file.panelPlace));
        n.panelPlace = file.panelPlace;
    }
    if (file.buffTimers != running.buffTimers) {  // 3.0.4-beta.6
        u.applied.push_back(change("buff timers", flag(running.buffTimers), flag(file.buffTimers)));
        n.buffTimers = file.buffTimers;
    }
    if (file.cameraIndoor != running.cameraIndoor) {  // 3.0.4-beta.5
        u.applied.push_back(change("camera indoor", flag(running.cameraIndoor), flag(file.cameraIndoor)));
        n.cameraIndoor = file.cameraIndoor;
    }
    if (file.cameraZoomLimit != running.cameraZoomLimit) {  // 3.0.4-beta.17
        u.applied.push_back(change("camera zoom_limit", flag(running.cameraZoomLimit), flag(file.cameraZoomLimit)));
        n.cameraZoomLimit = file.cameraZoomLimit;
    }
    {  // 3.0.4-beta.3: the notifications, each box (3.0.7-beta.6: kNotifyBoxes, and the two thresholds)
        const NotifySettings &f = file.notify, &r = running.notify;
        for (size_t i = 0; i < kNotifyBoxCount; ++i) {
            const bool was = r.*kNotifyBoxes[i].field, now = f.*kNotifyBoxes[i].field;
            const std::string name = std::string("notify ") + kNotifyBoxes[i].key;
            if (was != now) u.applied.push_back(change(name.c_str(), flag(was), flag(now)));
        }
        if (r.buffSeconds != f.buffSeconds)
            u.applied.push_back(change("notify buff_seconds", std::to_string(r.buffSeconds),
                                       std::to_string(f.buffSeconds)));
        if (r.partyHp != f.partyHp)
            u.applied.push_back(change("notify party_hp", std::to_string(r.partyHp), std::to_string(f.partyHp)));
        if (r.recoveredPercent != f.recoveredPercent)  // 3.0.7-beta.7
            u.applied.push_back(change("notify recovered_percent", std::to_string(r.recoveredPercent),
                                       std::to_string(f.recoveredPercent)));
        n.notify = f;
    }
    if (file.sound.ambience != running.sound.ambience) {  // 3.0.4-beta.7: the maps' own sounds
        u.applied.push_back(change("ambience", flag(running.sound.ambience), flag(file.sound.ambience)));
        n.sound.ambience = file.sound.ambience;
    }
    if (file.sound.ambienceVolume != running.sound.ambienceVolume) {
        u.applied.push_back(change("ambience_volume", std::to_string(running.sound.ambienceVolume),
                                   std::to_string(file.sound.ambienceVolume)));
        n.sound.ambienceVolume = file.sound.ambienceVolume;
    }
    if (file.sound.output != running.sound.output) {  // 3.0.4-beta.1: the output device
        const auto word = [](const std::string& id) { return id.empty() ? std::string("auto") : id; };
        u.applied.push_back(change("output", word(running.sound.output), word(file.sound.output)));
        n.sound.output = file.sound.output;
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
    n.developer = file.developer;  // 2.15: uaRO Studio's keys follow the settings app's mode at once
    if (file.xray != running.xray) {  // 2.3: uaRO Studio's X-ray at once (the console, the settings app)
        u.applied.push_back(change("xray", flag(running.xray), flag(file.xray)));
        n.xray = file.xray;
    }
    if (file.seamsView != running.seamsView) {  // 2.7.8: the seams' debug view at once (the console)
        u.applied.push_back(change("seams_view", std::to_string(running.seamsView), std::to_string(file.seamsView)));
        n.seamsView = file.seamsView;
    }
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
           k == "quality.render_scale" || k == "quality.world_scale" || k == "quality.full_colour" ||
           k == "quality.super_light" || k == "frames.battery_saver" || k == "frames.battery_fps" ||
           k == "quality.sharpen" ||
           k == "quality.msaa" || k == "quality.taa" ||
           k == "quality.alpha_to_coverage" || k == "quality.anisotropy" || k == "frames.vsync" ||
           k == "frames.fps_limit" || k == "frames.keep_fps" || k == "frames.save_power" || k == "debug.log" ||
           k == "debug.profile" || k == "debug.xray" || k == "debug.seams_view" || k == "frames.fps_counter" ||
           k == "stats.send" || k.compare(0, 8, "windows.") == 0 || k.compare(0, 8, "effects.") == 0 ||
           k.compare(0, 6, "panel.") == 0 ||  // 3.0.3-beta.8
           k == "interface.minimap" ||        // 3.0.11-beta.1
           k == "sound.output" ||             // 3.0.4-beta.1: the music and the sounds move at once
           k == "sound.ambience" || k == "sound.ambience_volume" ||  // 3.0.4-beta.7
           k.compare(0, 7, "notify.") == 0 ||  // 3.0.4-beta.3
           k == "camera.indoor" ||            // 3.0.4-beta.5
           k == "camera.zoom_limit" ||        // 3.0.4-beta.17
           k == "buffs.timers";               // 3.0.4-beta.6
}

}  // namespace uagfx
