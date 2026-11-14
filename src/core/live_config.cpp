#include "core/live_config.hpp"

#include <tuple>
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
    if (file.dpiAware != running.dpiAware) u.waiting.push_back("dpi_aware");  // the window is made with it
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
    if (file.crispNumbers != running.crispNumbers) {  // 3.0.18-beta.178: read at every draw
        u.applied.push_back(change("crisp_numbers", flag(running.crispNumbers), flag(file.crispNumbers)));
        n.crispNumbers = file.crispNumbers;
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
    if (file.crossPresent != running.crossPresent) {  // 3.0.19-beta.9: the renderer takes it at a present
        u.applied.push_back(change("cross_present", flag(running.crossPresent), flag(file.crossPresent)));
        n.crossPresent = file.crossPresent;
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
    if (file.minimapNpcs != running.minimapNpcs) {  // 3.0.11-beta.6: the minimap's hints, at once
        u.applied.push_back(change("minimap_npcs", flag(running.minimapNpcs), flag(file.minimapNpcs)));
        n.minimapNpcs = file.minimapNpcs;
    }
    if (file.minimapWarps != running.minimapWarps) {
        u.applied.push_back(change("minimap_warps", flag(running.minimapWarps), flag(file.minimapWarps)));
        n.minimapWarps = file.minimapWarps;
    }
    if (file.predictWalk != running.predictWalk) {  // 3.0.11-beta.130: the own walk at the click, at once
        u.applied.push_back(change("predict_walk", flag(running.predictWalk), flag(file.predictWalk)));
        n.predictWalk = file.predictWalk;
    }
    if (file.smoothFixes != running.smoothFixes) {  // 3.0.11-beta.123: the position fixes glided to, at once
        u.applied.push_back(change("smooth_fixes", flag(running.smoothFixes), flag(file.smoothFixes)));
        n.smoothFixes = file.smoothFixes;
    }
    if (file.quickAcks != running.quickAcks) {  // 3.0.11-beta.128: the server's packets acknowledged at once
        u.applied.push_back(change("quick_acks", flag(running.quickAcks), flag(file.quickAcks)));
        n.quickAcks = file.quickAcks;
    }
    if (file.netBadge != running.netBadge) {  // 3.0.11-beta.122: the round trip's badge, at once
        u.applied.push_back(change("net_badge", flag(running.netBadge), flag(file.netBadge)));
        n.netBadge = file.netBadge;
    }
    if (file.minimapStyle != running.minimapStyle) {  // 3.0.11-beta.120: the maps' picture, at once
        u.applied.push_back(
            change("minimap_style", mapStyleToken(running.minimapStyle), mapStyleToken(file.minimapStyle)));
        n.minimapStyle = file.minimapStyle;
    }
    if (file.minimapKafra != running.minimapKafra) {  // 3.0.11-beta.119: the map's layers, at once
        u.applied.push_back(change("minimap_kafra", flag(running.minimapKafra), flag(file.minimapKafra)));
        n.minimapKafra = file.minimapKafra;
    }
    for (const auto& [name, f, r] : {std::tuple{"chat_party", &file.chatParty, &running.chatParty},
                                     std::tuple{"chat_guild", &file.chatGuild, &running.chatGuild},
                                     std::tuple{"chat_own", &file.chatOwn, &running.chatOwn},
                                     std::tuple{"chat_others", &file.chatOthers, &running.chatOthers}})
        if (*f != *r) {  // 3.0.13-beta.3: the chat's colours, at once (the lines coming from now)
            u.applied.push_back(change(name, *r, *f));
            if (std::string(name) == "chat_party") n.chatParty = *f;
            if (std::string(name) == "chat_guild") n.chatGuild = *f;
            if (std::string(name) == "chat_own") n.chatOwn = *f;
            if (std::string(name) == "chat_others") n.chatOthers = *f;
        }
    if (file.longChat != running.longChat) {  // 3.0.13-beta.12
        u.applied.push_back(change("long_chat", flag(running.longChat), flag(file.longChat)));
        n.longChat = file.longChat;
    }
    if (file.chatLog != running.chatLog) {  // 3.0.13-beta.3
        u.applied.push_back(change("chat_log", flag(running.chatLog), flag(file.chatLog)));
        n.chatLog = file.chatLog;
    }
    if (file.chatFade != running.chatFade) {  // 3.0.11-beta.240: the chat's fade, at once
        u.applied.push_back(change("chat_fade", std::to_string(running.chatFade), std::to_string(file.chatFade)));
        n.chatFade = file.chatFade;
    }
    if (file.expGain != running.expGain) {  // 3.0.11-beta.237: a kill's experience over the monster, at once
        u.applied.push_back(change("exp_gain", flag(running.expGain), flag(file.expGain)));
        n.expGain = file.expGain;
    }
    if (file.expBars != running.expBars) {  // 3.0.11-beta.236: the experience's bars at the bottom, at once
        u.applied.push_back(change("exp_bars", flag(running.expBars), flag(file.expBars)));
        n.expBars = file.expBars;
    }
    if (file.minimapClassic != running.minimapClassic) {  // 3.0.11-beta.222: the marks' look, at once
        u.applied.push_back(change("minimap_classic", flag(running.minimapClassic), flag(file.minimapClassic)));
        n.minimapClassic = file.minimapClassic;
    }
    if (file.minimapView != running.minimapView) {  // 3.0.11-beta.187: the camera's fan, at once
        u.applied.push_back(change("minimap_view", flag(running.minimapView), flag(file.minimapView)));
        n.minimapView = file.minimapView;
    }
    if (file.minimapParty != running.minimapParty) {
        u.applied.push_back(change("minimap_party", flag(running.minimapParty), flag(file.minimapParty)));
        n.minimapParty = file.minimapParty;
    }
    if (file.minimapMobs != running.minimapMobs) {
        u.applied.push_back(change("minimap_mobs", flag(running.minimapMobs), flag(file.minimapMobs)));
        n.minimapMobs = file.minimapMobs;
    }
    if (file.minimapReach != running.minimapReach) {  // 3.0.11-beta.65: where one can walk, at once
        u.applied.push_back(change("minimap_reach", flag(running.minimapReach), flag(file.minimapReach)));
        n.minimapReach = file.minimapReach;
    }
    if (file.minimapHd != running.minimapHd) {  // 3.0.11-beta.72: the maps' pictures, at once
        u.applied.push_back(change("minimap_hd", flag(running.minimapHd), flag(file.minimapHd)));
        n.minimapHd = file.minimapHd;
    }
    if (file.packIcons != running.packIcons) {  // 3.0.11-beta.74: the panels' icons, at once
        u.applied.push_back(change("pack_icons", flag(running.packIcons), flag(file.packIcons)));
        n.packIcons = file.packIcons;
    }
    if (file.lockLayout != running.lockLayout) {  // 3.0.11-beta.77: the parts kept where they are, at once
        u.applied.push_back(change("lock_parts", flag(running.lockLayout), flag(file.lockLayout)));
        n.lockLayout = file.lockLayout;
    }
    if (file.hotbarPerCharacter != running.hotbarPerCharacter) {  // 3.0.11-beta.78: each character's own bar
        u.applied.push_back(
            change("hotbar_per_character", flag(running.hotbarPerCharacter), flag(file.hotbarPerCharacter)));
        n.hotbarPerCharacter = file.hotbarPerCharacter;
    }
    if (file.hotbarAmmo != running.hotbarAmmo) {  // 3.0.11-beta.125: the ammo left over the hotbar, at once
        u.applied.push_back(change("hotbar_ammo", flag(running.hotbarAmmo), flag(file.hotbarAmmo)));
        n.hotbarAmmo = file.hotbarAmmo;
    }
    if (file.elementColours != running.elementColours) {  // 3.0.18-beta.144: the things' element colours
        u.applied.push_back(change("element_colours", flag(running.elementColours), flag(file.elementColours)));
        n.elementColours = file.elementColours;
    }
    if (file.ammoSound != running.ammoSound) {  // 3.0.11-beta.129: the chime at 10 left and at none, at once
        u.applied.push_back(change("ammo_sound", flag(running.ammoSound), flag(file.ammoSound)));
        n.ammoSound = file.ammoSound;
    }
    if (file.padHints != running.padHints) {  // 3.0.11-beta.86: the controller's buttons on the interface
        u.applied.push_back(change("pad_hints", flag(running.padHints), flag(file.padHints)));
        n.padHints = file.padHints;
    }
    if (file.padTiles != running.padTiles) {  // 3.0.11-beta.90: Start opens the windows' tile
        u.applied.push_back(change("pad_tiles", flag(running.padTiles), flag(file.padTiles)));
        n.padTiles = file.padTiles;
    }
    if (file.padWalk != running.padWalk) {  // 3.0.11-beta.88: the left stick without the mouse
        u.applied.push_back(change("pad_walk", padWalkToken(running.padWalk), padWalkToken(file.padWalk)));
        n.padWalk = file.padWalk;
    }
    if (file.ownKeys != running.ownKeys) {  // 3.0.18-beta.125: our windows' Ctrl letters, at once
        u.applied.push_back(change("key_*", "", ""));
        n.ownKeys = file.ownKeys;
    }
    if (file.walkArrow != running.walkArrow) {  // 3.0.11-beta.103: the look of the stick's arrow
        u.applied.push_back(change("walk_arrow", walkLookToken(running.walkArrow), walkLookToken(file.walkArrow)));
        n.walkArrow = file.walkArrow;
    }
    if (file.cursorLook != running.cursorLook) {  // 3.0.11-beta.198: the cursor's look
        u.applied.push_back(change("cursor", cursorLookToken(running.cursorLook), cursorLookToken(file.cursorLook)));
        n.cursorLook = file.cursorLook;
    }
    if (file.spaceAction != running.spaceAction) {  // 3.0.18-beta.209: Space acts
        u.applied.push_back(change("space_action", flag(running.spaceAction), flag(file.spaceAction)));
        n.spaceAction = file.spaceAction;
    }
    if (file.walkMode != running.walkMode) {  // 3.0.18-beta.208: the walk mode
        u.applied.push_back(change("walk_mode", flag(running.walkMode), flag(file.walkMode)));
        n.walkMode = file.walkMode;
    }
    if (file.arrowsWalk != running.arrowsWalk) {  // 3.0.18-beta.207: the arrows walk
        u.applied.push_back(change("arrows_walk", flag(running.arrowsWalk), flag(file.arrowsWalk)));
        n.arrowsWalk = file.arrowsWalk;
    }
    if (file.walkMark != running.walkMark) {  // 3.0.11-beta.105: the mark at the arrow's end
        u.applied.push_back(change("walk_mark", flag(running.walkMark), flag(file.walkMark)));
        n.walkMark = file.walkMark;
    }
    if (file.mobHp != running.mobHp) {  // 3.0.11-beta.95: the monsters' HP bars
        u.applied.push_back(change("mob_hp", mobHpToken(running.mobHp), mobHpToken(file.mobHp)));
        n.mobHp = file.mobHp;
    }
    if (file.mobHpPercent != running.mobHpPercent) {  // 3.0.11-beta.293
        u.applied.push_back(change("mob_hp_percent", flag(running.mobHpPercent), flag(file.mobHpPercent)));
        n.mobHpPercent = file.mobHpPercent;
    }
    if (file.follow != running.follow) {  // 3.0.13-beta.11
        u.applied.push_back(change("follow", flag(running.follow), flag(file.follow)));
        n.follow = file.follow;
    }
    if (file.escCutin != running.escCutin) {  // 3.0.13-beta.10
        u.applied.push_back(change("esc_cutin", flag(running.escCutin), flag(file.escCutin)));
        n.escCutin = file.escCutin;
    }
    if (file.swearFilter != running.swearFilter) {  // 3.0.13-beta.9
        u.applied.push_back(change("swear_filter", flag(running.swearFilter), flag(file.swearFilter)));
        n.swearFilter = file.swearFilter;
    }
    if (file.skillJokes != running.skillJokes) {  // 3.0.13-beta.8
        u.applied.push_back(change("skill_jokes", flag(running.skillJokes), flag(file.skillJokes)));
        n.skillJokes = file.skillJokes;
    }
    if (file.screenShake != running.screenShake) {  // 3.0.13-beta.8
        u.applied.push_back(change("screen_shake", flag(running.screenShake), flag(file.screenShake)));
        n.screenShake = file.screenShake;
    }
    if (file.quickTeleport != running.quickTeleport) {  // 3.0.13-beta.7
        u.applied.push_back(change("quick_teleport", flag(running.quickTeleport), flag(file.quickTeleport)));
        n.quickTeleport = file.quickTeleport;
    }
    if (file.whoHits != running.whoHits) {  // 3.0.13-beta.13
        u.applied.push_back(change("who_hits", flag(running.whoHits), flag(file.whoHits)));
        n.whoHits = file.whoHits;
    }
    if (file.bossBars != running.bossBars) {  // 3.0.13-beta.3
        u.applied.push_back(change("boss_bars", flag(running.bossBars), flag(file.bossBars)));
        n.bossBars = file.bossBars;
    }
    if (file.targetRing != running.targetRing) {  // 3.0.11-beta.87: the target's ring on the ground
        u.applied.push_back(
            change("target_ring", targetRingToken(running.targetRing), targetRingToken(file.targetRing)));
        n.targetRing = file.targetRing;
    }
    if (file.minimapWay != running.minimapWay) {  // 3.0.11-beta.33: the way on the ground, at once
        u.applied.push_back(
            change("minimap_way", wayOnGroundToken(running.minimapWay), wayOnGroundToken(file.minimapWay)));
        n.minimapWay = file.minimapWay;
    }
    if (file.buffs != running.buffs) {  // 3.0.11-beta.35: the buffs' column, at once
        u.applied.push_back(change("buffs", buffsLookToken(running.buffs), buffsLookToken(file.buffs)));
        n.buffs = file.buffs;
    }
    if (file.buffsOverflow != running.buffsOverflow) {  // 3.0.11-beta.63: what the column does not hold, at once
        u.applied.push_back(change("buffs_overflow", buffsOverflowToken(running.buffsOverflow),
                                   buffsOverflowToken(file.buffsOverflow)));
        n.buffsOverflow = file.buffsOverflow;
    }
    if (file.itemsWindow != running.itemsWindow) {  // 3.0.11-beta.42: «Речі», at once
        u.applied.push_back(change("items", flag(running.itemsWindow), flag(file.itemsWindow)));
        n.itemsWindow = file.itemsWindow;
    }
    if (file.itemToast != running.itemToast) {  // 3.0.11-beta.186
        u.applied.push_back(change("item_toast", flag(running.itemToast), flag(file.itemToast)));
        n.itemToast = file.itemToast;
    }
    if (file.statusWindow != running.statusWindow) {  // 3.0.11-beta.42: «Характеристики», at once
        u.applied.push_back(change("status", flag(running.statusWindow), flag(file.statusWindow)));
        n.statusWindow = file.statusWindow;
    }
    if (file.skillsWindow != running.skillsWindow) {  // 3.0.11-beta.96: «Навички», at once
        u.applied.push_back(change("skills", flag(running.skillsWindow), flag(file.skillsWindow)));
        n.skillsWindow = file.skillsWindow;
    }
    if (file.equipWindow != running.equipWindow) {  // 3.0.11-beta.97: «Спорядження», at once
        u.applied.push_back(change("equip", flag(running.equipWindow), flag(file.equipWindow)));
        n.equipWindow = file.equipWindow;
    }
    if (file.homunAi != running.homunAi || file.homunAggressive != running.homunAggressive ||
        file.homunDefend != running.homunDefend || file.homunSkills != running.homunSkills) {  // .290: at once
        u.applied.push_back(change("homun_ai", flag(running.homunAi), flag(file.homunAi)));
        n.homunAi = file.homunAi;
        n.homunAggressive = file.homunAggressive;
        n.homunDefend = file.homunDefend;
        n.homunSkills = file.homunSkills;
    }
    if (file.layoutPerChar != running.layoutPerChar) {  // 3.0.11-beta.288: each character's layout, at once
        u.applied.push_back(change("layout_per_char", flag(running.layoutPerChar), flag(file.layoutPerChar)));
        n.layoutPerChar = file.layoutPerChar;
    }
    if (file.dropNames != running.dropNames) {  // 3.0.11-beta.283: the items on the ground named with Alt, at once
        u.applied.push_back(change("drop_names", flag(running.dropNames), flag(file.dropNames)));
        n.dropNames = file.dropNames;
    }
    if (file.dropHighlight != running.dropHighlight) {  // 3.0.11-beta.283: the valuable ones always, at once
        u.applied.push_back(change("drop_highlight", flag(running.dropHighlight), flag(file.dropHighlight)));
        n.dropHighlight = file.dropHighlight;
    }
    if (file.partyAlarm != running.partyAlarm) {  // 3.0.11-beta.282: the healer's alarm, at once
        u.applied.push_back(change("party_alarm", flag(running.partyAlarm), flag(file.partyAlarm)));
        n.partyAlarm = file.partyAlarm;
    }
    if (file.potionWarn != running.potionWarn) {  // 3.0.11-beta.281: the healing items' warning, at once
        u.applied.push_back(change("potion_warn", flag(running.potionWarn), flag(file.potionWarn)));
        n.potionWarn = file.potionWarn;
    }
    if (file.mvpTimers != running.mvpTimers) {  // 3.0.11-beta.280: the bosses' timers, at once
        u.applied.push_back(change("mvp_timers", flag(running.mvpTimers), flag(file.mvpTimers)));
        n.mvpTimers = file.mvpTimers;
    }
    if (file.mobCard != running.mobCard) {  // 3.0.11-beta.279: the monster's card, at once
        u.applied.push_back(change("mob_card", flag(running.mobCard), flag(file.mobCard)));
        n.mobCard = file.mobCard;
    }
    if (file.huntPanel != running.huntPanel) {  // 3.0.11-beta.278: «Полювання», at once
        u.applied.push_back(change("hunt_panel", flag(running.huntPanel), flag(file.huntPanel)));
        n.huntPanel = file.huntPanel;
    }
    if (file.deathRecap != running.deathRecap) {  // 3.0.11-beta.277: the death's last hits, at once
        u.applied.push_back(change("death_recap", flag(running.deathRecap), flag(file.deathRecap)));
        n.deathRecap = file.deathRecap;
    }
    if (file.delayRing != running.delayRing) {  // 3.0.11-beta.276: the hotbar's delay sweep, at once
        u.applied.push_back(change("delay_ring", flag(running.delayRing), flag(file.delayRing)));
        n.delayRing = file.delayRing;
    }
    if (file.charCarousel != running.charCarousel) {  // 3.0.11-beta.275: the character select's look, at once
        u.applied.push_back(change("char_carousel", flag(running.charCarousel), flag(file.charCarousel)));
        n.charCarousel = file.charCarousel;
    }
    if (file.portraits != running.portraits) {  // 3.0.11-beta.273: the character's picture, at once
        u.applied.push_back(change("portraits", flag(running.portraits), flag(file.portraits)));
        n.portraits = file.portraits;
    }
    if (file.npcRemember != running.npcRemember) {  // 3.0.11-beta.255: the option chosen last time, at once
        u.applied.push_back(change("npc_remember", flag(running.npcRemember), flag(file.npcRemember)));
        n.npcRemember = file.npcRemember;
    }
    if (file.npcWindow != running.npcWindow) {  // 3.0.11-beta.98: the NPC talk, at once
        u.applied.push_back(change("npc_talk", flag(running.npcWindow), flag(file.npcWindow)));
        n.npcWindow = file.npcWindow;
    }
    if (file.shopWindow != running.shopWindow) {  // 3.0.11-beta.100: the NPCs' shops, at once
        u.applied.push_back(change("shop", flag(running.shopWindow), flag(file.shopWindow)));
        n.shopWindow = file.shopWindow;
    }
    if (file.storageWindow != running.storageWindow) {  // 3.0.11-beta.115: the Kafra storage, at once
        u.applied.push_back(change("storage", flag(running.storageWindow), flag(file.storageWindow)));
        n.storageWindow = file.storageWindow;
    }
    if (file.tradeWindow != running.tradeWindow) {  // 3.0.11-beta.116: the trade, at once
        u.applied.push_back(change("trade", flag(running.tradeWindow), flag(file.tradeWindow)));
        n.tradeWindow = file.tradeWindow;
    }
    if (file.menuWindow != running.menuWindow) {  // 3.0.11-beta.140: the menu and the death screen, at once
        u.applied.push_back(change("menu", flag(running.menuWindow), flag(file.menuWindow)));
        n.menuWindow = file.menuWindow;
    }
    if (file.cardWindows != running.cardWindows) {  // 3.0.11-beta.149: the item card and the skill card, at once
        u.applied.push_back(change("cards", flag(running.cardWindows), flag(file.cardWindows)));
        n.cardWindows = file.cardWindows;
    }
    if (file.askWindows != running.askWindows) {  // 3.0.11-beta.150: the requests and the NPC's inputs, at once
        u.applied.push_back(change("asks", flag(running.askWindows), flag(file.askWindows)));
        n.askWindows = file.askWindows;
    }
    if (file.messageBoxes != running.messageBoxes) {  // 3.0.11-beta.151: the game's message boxes, at once
        u.applied.push_back(change("messages", flag(running.messageBoxes), flag(file.messageBoxes)));
        n.messageBoxes = file.messageBoxes;
    }
    if (file.loginWindow != running.loginWindow) {  // 3.0.11-beta.151: the login window, at once
        u.applied.push_back(change("login", flag(running.loginWindow), flag(file.loginWindow)));
        n.loginWindow = file.loginWindow;
    }
    if (file.language != running.language) {  // 3.0.11-beta.174: the driver's words' language, at once
        u.applied.push_back(change("language", running.language, file.language));
        n.language = file.language;
    }
    if (file.loginView != running.loginView) {  // 3.0.11-beta.177: the login screen's view, at once
        u.applied.push_back(
            change("login_view", loginview::token(running.loginView), loginview::token(file.loginView)));
        n.loginView = file.loginView;
    }
    if (file.loginNews != running.loginNews) {  // 3.0.11-beta.174: the site's item now on the login, at once
        u.applied.push_back(change("login_news", flag(running.loginNews), flag(file.loginNews)));
        n.loginNews = file.loginNews;
    }
    if (file.loginStatus != running.loginStatus) {  // 3.0.11-beta.173: the server's status on the login, at once
        u.applied.push_back(change("login_status", flag(running.loginStatus), flag(file.loginStatus)));
        n.loginStatus = file.loginStatus;
    }
    if (file.charSelectWindow != running.charSelectWindow) {  // 3.0.11-beta.152: the character select, at once
        u.applied.push_back(change("char_select", flag(running.charSelectWindow), flag(file.charSelectWindow)));
        n.charSelectWindow = file.charSelectWindow;
    }
    if (file.charMakeWindow != running.charMakeWindow) {  // 3.0.11-beta.153: the character make, at once
        u.applied.push_back(change("char_make", flag(running.charMakeWindow), flag(file.charMakeWindow)));
        n.charMakeWindow = file.charMakeWindow;
    }
    if (file.cartWindow != running.cartWindow) {  // 3.0.11-beta.154: «Візок», at once
        u.applied.push_back(change("cart", flag(running.cartWindow), flag(file.cartWindow)));
        n.cartWindow = file.cartWindow;
    }
    if (file.countWindow != running.countWindow) {  // 3.0.11-beta.155: the «how many» card, at once
        u.applied.push_back(change("counts", flag(running.countWindow), flag(file.countWindow)));
        n.countWindow = file.countWindow;
    }
    if (file.vendWindow != running.vendWindow) {  // 3.0.11-beta.156: «Лавка», at once
        u.applied.push_back(change("vending", flag(running.vendWindow), flag(file.vendWindow)));
        n.vendWindow = file.vendWindow;
    }
    if (file.pickWindows != running.pickWindows) {  // 3.0.11-beta.157: the choice of things, at once
        u.applied.push_back(change("picks", flag(running.pickWindows), flag(file.pickWindows)));
        n.pickWindows = file.pickWindows;
    }
    if (file.partySetupWindow != running.partySetupWindow) {  // 3.0.11-beta.158: the party's settings, at once
        u.applied.push_back(change("party_setup", flag(running.partySetupWindow), flag(file.partySetupWindow)));
        n.partySetupWindow = file.partySetupWindow;
    }
    if (file.petWindow != running.petWindow) {  // 3.0.11-beta.159: the pet's card, at once
        u.applied.push_back(change("pet", flag(running.petWindow), flag(file.petWindow)));
        n.petWindow = file.petWindow;
    }
    if (file.homunWindow != running.homunWindow) {  // 3.0.11-beta.160: the homunculus' card, at once
        u.applied.push_back(change("homun", flag(running.homunWindow), flag(file.homunWindow)));
        n.homunWindow = file.homunWindow;
    }
    if (file.mercWindow != running.mercWindow) {  // 3.0.11-beta.161: the mercenary's card, at once
        u.applied.push_back(change("merc", flag(running.mercWindow), flag(file.mercWindow)));
        n.mercWindow = file.mercWindow;
    }
    if (file.optionsWindow != running.optionsWindow) {  // 3.0.11-beta.162: the options card, at once
        u.applied.push_back(change("options", flag(running.optionsWindow), flag(file.optionsWindow)));
        n.optionsWindow = file.optionsWindow;
    }
    if (file.socialWindow != running.socialWindow) {  // 3.0.11-beta.163: the friends and party card, at once
        u.applied.push_back(change("social", flag(running.socialWindow), flag(file.socialWindow)));
        n.socialWindow = file.socialWindow;
    }
    if (file.serverWindow != running.serverWindow) {  // 3.0.11-beta.164: the server select, at once
        u.applied.push_back(change("servers", flag(running.serverWindow), flag(file.serverWindow)));
        n.serverWindow = file.serverWindow;
    }
    if (file.guildWindow != running.guildWindow) {  // 3.0.11-beta.165: the guild card, at once
        u.applied.push_back(change("guild", flag(running.guildWindow), flag(file.guildWindow)));
        n.guildWindow = file.guildWindow;
    }
    if (file.myShopWindow != running.myShopWindow) {  // 3.0.11-beta.166: one's own shop card, at once
        u.applied.push_back(change("my_shop", flag(running.myShopWindow), flag(file.myShopWindow)));
        n.myShopWindow = file.myShopWindow;
    }
    if (file.vendSetupWindow != running.vendSetupWindow) {  // 3.0.16-beta.33: the shop's opening card, at once
        u.applied.push_back(change("vend_setup", flag(running.vendSetupWindow), flag(file.vendSetupWindow)));
        n.vendSetupWindow = file.vendSetupWindow;
    }
    if (file.whisperSetupWindow != running.whisperSetupWindow) {  // 3.0.11-beta.168: the whisper settings, at once
        u.applied.push_back(change("whisper_setup", flag(running.whisperSetupWindow), flag(file.whisperSetupWindow)));
        n.whisperSetupWindow = file.whisperSetupWindow;
    }
    if (file.macroWindow != running.macroWindow) {  // 3.0.11-beta.169: the macros card, at once
        u.applied.push_back(change("macros", flag(running.macroWindow), flag(file.macroWindow)));
        n.macroWindow = file.macroWindow;
    }
    if (file.tipWindow != running.tipWindow) {  // 3.0.11-beta.204: «Tip of the Day» as a card, at once
        u.applied.push_back(change("tips", flag(running.tipWindow), flag(file.tipWindow)));
        n.tipWindow = file.tipWindow;
    }
    if (file.loadingBar != running.loadingBar) {  // 3.0.18-beta.180: the map loading bar, at the next loading
        u.applied.push_back(change("loading_bar", flag(running.loadingBar), flag(file.loadingBar)));
        n.loadingBar = file.loadingBar;
    }
    if (file.emoteWindow != running.emoteWindow) {  // 3.0.11-beta.205: the emotion list as a card, at once
        u.applied.push_back(change("emotes", flag(running.emoteWindow), flag(file.emoteWindow)));
        n.emoteWindow = file.emoteWindow;
    }
    if (file.commandWindow != running.commandWindow) {  // 3.0.11-beta.206: the command list as a card, at once
        u.applied.push_back(change("commands", flag(running.commandWindow), flag(file.commandWindow)));
        n.commandWindow = file.commandWindow;
    }
    if (file.chatRoomWindows != running.chatRoomWindows) {  // 3.0.11-beta.207: the chat rooms as cards, at once
        u.applied.push_back(change("chat_rooms", flag(running.chatRoomWindows), flag(file.chatRoomWindows)));
        n.chatRoomWindows = file.chatRoomWindows;
    }
    if (file.detailConsole != running.detailConsole) {  // 3.0.11-beta.209: Alt+D's console, at once
        u.applied.push_back(change("detail_console", flag(running.detailConsole), flag(file.detailConsole)));
        n.detailConsole = file.detailConsole;
    }
    if (file.popupMenus != running.popupMenus) {  // 3.0.11-beta.211: the popup menus as cards, at once
        u.applied.push_back(change("popups", flag(running.popupMenus), flag(file.popupMenus)));
        n.popupMenus = file.popupMenus;
    }
    if (file.whisperWindows != running.whisperWindows) {  // 3.0.11-beta.212: the whisper windows as cards, at once
        u.applied.push_back(change("whispers", flag(running.whisperWindows), flag(file.whisperWindows)));
        n.whisperWindows = file.whisperWindows;
    }
    if (file.skillCells != running.skillCells) {  // 3.0.11-beta.142: a ground skill's cells, at once
        u.applied.push_back(change("skill_cells", flag(running.skillCells), flag(file.skillCells)));
        n.skillCells = file.skillCells;
    }
    const auto flagAtOnce = [&](const char* key, bool Config::*field) {  // 3.0.16-beta.13
        if (file.*field == running.*field) return;
        u.applied.push_back(change(key, flag(running.*field), flag(file.*field)));
        n.*field = file.*field;
    };
    flagAtOnce("skill_needs", &Config::skillNeeds);
    flagAtOnce("combo_window", &Config::comboWindow);
    flagAtOnce("broken_gear", &Config::brokenGear);
    flagAtOnce("pet_hunger", &Config::petHunger);
    flagAtOnce("ground_timers", &Config::groundTimers);  // 3.0.16-beta.21
    if (file.hotbarSupplies != running.hotbarSupplies) {  // 3.0.16-beta.8: the supplies over the hotbar, at once
        u.applied.push_back(change("hotbar_supplies", flag(running.hotbarSupplies), flag(file.hotbarSupplies)));
        n.hotbarSupplies = file.hotbarSupplies;
    }
    if (file.mouseCell != running.mouseCell) {  // 3.0.16-beta.7: the cell under the mouse, at once
        u.applied.push_back(change("mouse_cell", flag(running.mouseCell), flag(file.mouseCell)));
        n.mouseCell = file.mouseCell;
    }
    if (file.worldSigns != running.worldSigns) {  // 3.0.16-beta.7: the signs over the characters, at once
        u.applied.push_back(change("signs", flag(running.worldSigns), flag(file.worldSigns)));
        n.worldSigns = file.worldSigns;
    }
    if (file.groundCounts != running.groundCounts) {  // 3.0.11-beta.143: walls standing, traps left, at once
        u.applied.push_back(change("ground_counts", flag(running.groundCounts), flag(file.groundCounts)));
        n.groundCounts = file.groundCounts;
    }
    if (file.uiSize != running.uiSize) {  // 3.0.11-beta.54: the new interface's size, at once
        const auto token = [](int v) { return v > 0 ? std::to_string(v) : std::string("auto"); };
        u.applied.push_back(change("size", token(running.uiSize), token(file.uiSize)));
        n.uiSize = file.uiSize;
    }
    if (file.textSize != running.textSize) {  // 3.0.11-beta.67: the reading windows' text, at once
        u.applied.push_back(change("text_size", std::to_string(running.textSize), std::to_string(file.textSize)));
        n.textSize = file.textSize;
    }
    if (file.itemWindows != running.itemWindows) {  // 3.0.15-beta.9: the item windows' size, at once
        u.applied.push_back(
            change("item_windows", std::to_string(running.itemWindows), std::to_string(file.itemWindows)));
        n.itemWindows = file.itemWindows;
    }
    if (file.lowHpEdge != running.lowHpEdge) {  // 3.0.11-beta.50: the low HP edge, at once
        u.applied.push_back(
            change("low_hp_edge", std::to_string(running.lowHpEdge), std::to_string(file.lowHpEdge)));
        n.lowHpEdge = file.lowHpEdge;
    }
    if (file.castLabel != running.castLabel) {  // 3.0.11-beta.48: the cast's label, at once
        u.applied.push_back(change("cast", flag(running.castLabel), flag(file.castLabel)));
        n.castLabel = file.castLabel;
    }
    if (file.hudOpacity != running.hudOpacity) {  // 3.0.11-beta.47: the new interface's grounds, at once
        u.applied.push_back(
            change("panel_opacity", std::to_string(running.hudOpacity), std::to_string(file.hudOpacity)));
        n.hudOpacity = file.hudOpacity;
    }
    if (file.windowOpacity != running.windowOpacity) {
        u.applied.push_back(
            change("window_opacity", std::to_string(running.windowOpacity), std::to_string(file.windowOpacity)));
        n.windowOpacity = file.windowOpacity;
    }
    if (file.uiTheme != running.uiTheme) {  // 3.0.18-beta.13: uaUI's theme, at once
        u.applied.push_back(change("theme", running.uiTheme, file.uiTheme));
        n.uiTheme = file.uiTheme;
    }
    if (file.uauiMotion != running.uauiMotion) {  // 3.0.18-beta.58
        u.applied.push_back(change("uaui_motion", flag(running.uauiMotion), flag(file.uauiMotion)));
        n.uauiMotion = file.uauiMotion;
    }
    if (file.uauiEffects != running.uauiEffects) {  // 3.0.18-beta.61
        u.applied.push_back(change("uaui_effects", flag(running.uauiEffects), flag(file.uauiEffects)));
        n.uauiEffects = file.uauiEffects;
    }
    if (file.uauiGlass != running.uauiGlass) {  // 3.0.18-beta.64
        u.applied.push_back(change("uaui_glass", flag(running.uauiGlass), flag(file.uauiGlass)));
        n.uauiGlass = file.uauiGlass;
    }
    if (file.uauiShowcase != running.uauiShowcase) {  // 3.0.18-beta.11
        u.applied.push_back(change("uaui_showcase", flag(running.uauiShowcase), flag(file.uauiShowcase)));
        n.uauiShowcase = file.uauiShowcase;
    }
    if (file.animations != running.animations) {  // 3.0.11-beta.46: the new interface's motion, at once
        u.applied.push_back(change("animations", flag(running.animations), flag(file.animations)));
        n.animations = file.animations;
    }
    if (file.chat != running.chat) {  // 3.0.11-beta.41: the chat, at once
        u.applied.push_back(change("chat", flag(running.chat), flag(file.chat)));
        n.chat = file.chat;
    }
    if (file.hotbar != running.hotbar) {  // 3.0.11-beta.40: the hotkeys' bar, at once
        u.applied.push_back(change("hotbar", flag(running.hotbar), flag(file.hotbar)));
        n.hotbar = file.hotbar;
    }
    if (file.tracker != running.tracker) {  // 3.0.11-beta.70: the tracker, at once
        u.applied.push_back(change("tracker", flag(running.tracker), flag(file.tracker)));
        n.tracker = file.tracker;
    }
    if (file.partyPanel != running.partyPanel) {  // 3.0.11-beta.39: the party's panel, at once
        u.applied.push_back(change("party", flag(running.partyPanel), flag(file.partyPanel)));
        n.partyPanel = file.partyPanel;
    }
    if (file.targetCard != running.targetCard) {  // 3.0.11-beta.38: the target's card, at once
        u.applied.push_back(change("target", flag(running.targetCard), flag(file.targetCard)));
        n.targetCard = file.targetCard;
    }
    if (file.character != running.character) {  // 3.0.11-beta.36: the character block, at once
        u.applied.push_back(
            change("character", characterLookToken(running.character), characterLookToken(file.character)));
        n.character = file.character;
    }
    if (file.referenceWindow != running.referenceWindow) {  // 3.0.11-beta.7: «Довідник», at once
        u.applied.push_back(change("reference", flag(running.referenceWindow), flag(file.referenceWindow)));
        n.referenceWindow = file.referenceWindow;
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
    if (file.cameraTilt != running.cameraTilt) {  // 3.0.13-beta.5
        u.applied.push_back(change("camera tilt", running.cameraTilt, file.cameraTilt));
        n.cameraTilt = file.cameraTilt;
    }
    if (file.view3dFovFirst != running.view3dFovFirst) {  // 3.0.19-beta.10: the 3D view's, at its next frame
        u.applied.push_back(change("3D view fov first", std::to_string(running.view3dFovFirst),
                                   std::to_string(file.view3dFovFirst)));
        n.view3dFovFirst = file.view3dFovFirst;
    }
    if (file.view3dFovBehind != running.view3dFovBehind) {
        u.applied.push_back(change("3D view fov behind", std::to_string(running.view3dFovBehind),
                                   std::to_string(file.view3dFovBehind)));
        n.view3dFovBehind = file.view3dFovBehind;
    }
    if (file.view3dMouse != running.view3dMouse) {
        u.applied.push_back(
            change("3D view mouse", std::to_string(running.view3dMouse), std::to_string(file.view3dMouse)));
        n.view3dMouse = file.view3dMouse;
    }
    if (file.cameraRemember != running.cameraRemember) {  // 3.0.11-beta.184
        u.applied.push_back(change("camera remember", flag(running.cameraRemember), flag(file.cameraRemember)));
        n.cameraRemember = file.cameraRemember;
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
    if (file.lightSkills != running.lightSkills) {  // 3.0.18-beta.186
        u.applied.push_back(change("light_skills", flag(running.lightSkills), flag(file.lightSkills)));
        n.lightSkills = file.lightSkills;
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
    if (file.connection.patientEntry != running.connection.patientEntry)  // 2.6.17: the way onto a map at once
        u.applied.push_back(
            change("patient_entry", flag(running.connection.patientEntry), flag(file.connection.patientEntry)));
    if (file.connection.entryPause != running.connection.entryPause)
        u.applied.push_back(
            change("entry_pause", flag(running.connection.entryPause), flag(file.connection.entryPause)));
    n.connection = file.connection;
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
           k == "display.crisp_numbers" ||
           k == "display.window_art" ||
           k == "quality.render_scale" || k == "quality.world_scale" || k == "quality.full_colour" ||
           k == "quality.super_light" || k == "frames.battery_saver" || k == "frames.battery_fps" ||
           k == "quality.sharpen" ||
           k == "quality.msaa" || k == "quality.taa" ||
           k == "quality.alpha_to_coverage" || k == "quality.anisotropy" || k == "frames.vsync" ||
           k == "frames.fps_limit" || k == "frames.keep_fps" || k == "frames.save_power" || k == "debug.log" ||
           k == "frames.light_skills" ||  // 3.0.18-beta.186
           k == "renderer.cross_present" ||  // 3.0.19-beta.9
           k == "debug.profile" || k == "debug.xray" || k == "debug.seams_view" || k == "frames.fps_counter" ||
           k == "debug.quick_acks" ||  // 3.0.11-beta.128
           k == "utility.language" ||  // 3.0.11-beta.174
           k == "stats.send" || k.compare(0, 8, "windows.") == 0 || k.compare(0, 8, "effects.") == 0 ||
           k.compare(0, 6, "panel.") == 0 ||  // 3.0.3-beta.8
           k.compare(0, 10, "interface.") == 0 ||  // 3.0.11-beta.1: the minimap; 3.0.11-beta.6: its hints
           k == "sound.output" ||             // 3.0.4-beta.1: the music and the sounds move at once
           k == "sound.ambience" || k == "sound.ambience_volume" ||  // 3.0.4-beta.7
           k.compare(0, 7, "notify.") == 0 ||  // 3.0.4-beta.3
           k == "camera.indoor" ||            // 3.0.4-beta.5
           k == "camera.zoom_limit" ||        // 3.0.4-beta.17
           k == "camera.remember" ||          // 3.0.11-beta.184
           k == "camera.tilt" ||              // 3.0.13-beta.5
           k.compare(0, 7, "view3d.") == 0 ||  // 3.0.19-beta.10
           k == "buffs.timers" ||              // 3.0.4-beta.6
           k.compare(0, 11, "connection.") == 0;  // 3.0.11-beta.148: the way onto a map
}

}  // namespace uagfx
