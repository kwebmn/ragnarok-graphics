#include "core/settings/schema.hpp"

#include <cctype>

#include "core/effect_groups.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

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
        case Page::Debug: return Text::PageDebug;
        case Page::Launcher: return Text::ViewPlay;
        case Page::Interface: return Text::InterfaceHeading;  // 3.0.11-beta.3
        case Page::Service: break;
    }
    return Text::PageService;
}

// 3.0.18-beta.125: a window key's choices - none, or Ctrl and a letter the game leaves free (closedwin::kFreeLetters)
const std::vector<const char*> kKeyTokens = {"off", "b", "d", "g", "h", "i", "j", "k", "l", "n", "u", "y"};
const std::vector<Text> kKeyChoices = {Text::KeyOff,   Text::KeyCtrlB, Text::KeyCtrlD, Text::KeyCtrlG,
                                       Text::KeyCtrlH, Text::KeyCtrlI, Text::KeyCtrlJ, Text::KeyCtrlK,
                                       Text::KeyCtrlL, Text::KeyCtrlN, Text::KeyCtrlU, Text::KeyCtrlY};

const std::vector<IniSetting>& iniSettings() {
    using E = EffectSettings;  // 2.2.2: [effects] entries bound to their fields
    // 3.0.13-beta.3: the chat's colours (core/chat_panel chatColour)
    const std::vector<const char*> kChatColours{"default", "blue", "green", "yellow",
                                                "orange",  "pink", "violet", "white"};
    const std::vector<Text> kChatColourNames{Text::ChatColourDefault, Text::ChatColourBlue,   Text::ChatColourGreen,
                                             Text::ChatColourYellow,  Text::ChatColourOrange, Text::ChatColourPink,
                                             Text::ChatColourViolet,  Text::ChatColourWhite};
    static const std::vector<IniSetting> list{
        {"renderer", "api", Page::Render, Control::Choice, 0, 0, 0, {"d3d12", "d3d11", "auto", "system"},
         {Text::RenderApiD3d12, Text::RenderApiD3d11, Text::RenderApiAuto, Text::RenderApiSystem}, Text::RenderApi,
         Text::RenderApiHint, ""},
        // 0.50.2: a card's name as the Renderer page lists them, or auto (a list of its own there, not a text field)
        {"renderer", "gpu", Page::Render, Control::Line, 0, 0, 0, {}, {}, Text::RenderGpu, Text::RenderGpuHint, ""},
        // 0.51: the Diagnostics page's own check box
        {"report", "auto", Page::Diagnostics, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ReportAuto,
         Text::ReportAutoHint, ""},
        {"stats", "send", Page::Diagnostics, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::StatsSend,
         Text::StatsSendHint, ""},  // 2.2.1
        // 2.9: uaRO Pad inside the driver, on by default; the Main page shows it too
        {"pad", "enabled", Page::Pad, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::PadEnabled, Text::PadEnabledHint,
         ""},
        // 2.11: a direct start of uaRO.exe opens the launcher «Гра» unless this is ticked
        {"launcher", "direct_start", Page::Launcher, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::LauncherDirect,
         Text::LauncherDirectHint, ""},
        // 3.0.1-beta.5: our own libraries in place of the game's old ones (core/client_libs.hpp), «Сервіс»
        {"client", "cps", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientCps, Text::ClientCpsHint,
         ""},
        {"client", "bink", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientBink,
         Text::ClientBinkHint, ""},
        {"client", "ijl", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientIjl, Text::ClientIjlHint,
         ""},  // 3.0.1-beta.6
        {"client", "mss", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientMss, Text::ClientMssHint,
         ""},  // 3.0.1-beta.7
        {"client", "roext", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientRoext,
         Text::ClientRoextHint, ""},  // 3.0.1-beta.8: ROExt's work in the driver; its own rows on «Гра й чат»
        {"client", "updates", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientUpdates,
         Text::ClientUpdatesHint, ""},  // 3.0.2-beta.5: the client's graphics updates (updates\)
        {"roext", "window_lock", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextLock,
         Text::RoextLockHint, ""},
        {"roext", "window_on_top", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextOnTop,
         Text::RoextOnTopHint, ""},
        {"roext", "mouse_buttons", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextMouse,
         Text::RoextMouseHint, ""},
        {"roext", "alt_f4", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextAltF4, Text::RoextAltF4Hint,
         ""},
        {"roext", "minimize_key", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextMinimize,
         Text::RoextMinimizeHint, ""},
        {"roext", "free_cpu", Page::Game, Control::Slider, 0, 1000, 1, {}, {}, Text::RoextFreeCpu,
         Text::RoextFreeCpuHint, "ms"},
        {"roext", "code_page", Page::Game, Control::Number, -1, 65535, 1, {}, {}, Text::RoextCodePage,
         Text::RoextCodePageHint, ""},
        {"roext", "window_x", Page::Game, Control::Number, -100000, 100000, 1, {}, {}, Text::RoextWindowX,
         Text::RoextWindowPlaceHint, ""},
        {"roext", "window_y", Page::Game, Control::Number, -100000, 100000, 1, {}, {}, Text::RoextWindowY,
         Text::RoextWindowPlaceHint, ""},
        // 3.0.3-beta.2: several game windows at once
        {"windows", "title_name", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsTitleName,
         Text::WindowsTitleNameHint, ""},
        {"windows", "remember_place", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsRememberPlace,
         Text::WindowsRememberPlaceHint, ""},
        {"windows", "start_count", Page::Game, Control::Slider, 1, 8, 1, {}, {}, Text::WindowsStartCount,
         Text::WindowsStartCountHint, ""},  // 3.0.4-beta.4: a set of game windows by «Грати»
        {"windows", "switch_keys", Page::Game, Control::Choice, 0, 0, 0, {"all", "back", "off"},
         {Text::WindowsKeysAll, Text::WindowsKeysBack, Text::WindowsKeysOff}, Text::WindowsKeys,
         Text::WindowsKeysHint, ""},  // 3.0.3-beta.6: the keys that switch the game windows
        // 3.0.11-beta.1: the new interface's minimap prototype (stage 1) - off unless asked for
        {"interface", "minimap", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceMinimap,
         Text::InterfaceMinimapHint, ""},
        // 3.0.11-beta.6: its hints from the game's reference - each its own switch
        {"interface", "minimap_npcs", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapNpcs, Text::InterfaceMinimapNpcsHint, ""},
        {"interface", "minimap_warps", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapWarps, Text::InterfaceMinimapWarpsHint, ""},
        // 3.0.11-beta.123: the server's position fixes of 1-3 cells glided to
        {"interface", "smooth_fixes", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceSmoothFixes, Text::InterfaceSmoothFixesHint, ""},
        // 3.0.11-beta.130: the own walk at the click (an experiment, off)
        {"interface", "predict_walk", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfacePredictWalk, Text::InterfacePredictWalkHint, ""},
        // 3.0.11-beta.122: the round trip to the map server on the minimap's bar
        {"interface", "net_badge", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceNetBadge,
         Text::InterfaceNetBadgeHint, ""},
        // 3.0.11-beta.120: the maps' picture - the game's own or drawn from the walkable cells
        {"interface", "minimap_style", Page::Interface, Control::Choice, 0, 0, 0, {"picture", "scheme"},
         {Text::MinimapStylePicture, Text::MinimapStyleScheme}, Text::InterfaceMinimapStyle,
         Text::InterfaceMinimapStyleHint, ""},
        // 3.0.11-beta.119: the Kafras and the party, layers of their own (switched on the maps too)
        {"interface", "minimap_kafra", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapKafra, Text::InterfaceMinimapKafraHint, ""},
        {"interface", "minimap_party", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapParty, Text::InterfaceMinimapPartyHint, ""},
        // 3.0.11-beta.187: the camera's fan round the arrow
        {"interface", "minimap_view", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapView, Text::InterfaceMinimapViewHint, ""},
        // 3.0.11-beta.222: the marks as the game drew them
        {"interface", "minimap_classic", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},
         Text::InterfaceMinimapClassic, Text::InterfaceMinimapClassicHint, ""},
        // 3.0.11-beta.236: the experience's bars at the screen's bottom
        {"interface", "exp_bars", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceExpBars,
         Text::InterfaceExpBarsHint, ""},
        {"interface", "long_chat", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.13-beta.12
         Text::InterfaceLongChat, Text::InterfaceLongChatHint, ""},
        {"interface", "chat_log", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},  // 3.0.13-beta.3
         Text::InterfaceChatLog, Text::InterfaceChatLogHint, ""},
        {"interface", "skill_jokes", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.13-beta.8
         Text::InterfaceSkillJokes, Text::InterfaceSkillJokesHint, ""},
        {"interface", "swear_filter", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.13-beta.9
         Text::InterfaceSwearFilter, Text::InterfaceSwearFilterHint, ""},
        {"interface", "chat_party", Page::Interface, Control::Choice, 0, 0, 0, kChatColours, kChatColourNames,
         Text::InterfaceChatColourParty, Text::InterfaceChatColourHint, ""},  // 3.0.13-beta.3
        {"interface", "chat_guild", Page::Interface, Control::Choice, 0, 0, 0, kChatColours, kChatColourNames,
         Text::InterfaceChatColourGuild, Text::InterfaceChatColourHint, ""},  // 3.0.13-beta.3
        {"interface", "chat_own", Page::Interface, Control::Choice, 0, 0, 0, kChatColours, kChatColourNames,
         Text::InterfaceChatColourOwn, Text::InterfaceChatColourHint, ""},  // 3.0.13-beta.3
        {"interface", "chat_others", Page::Interface, Control::Choice, 0, 0, 0, kChatColours, kChatColourNames,
         Text::InterfaceChatColourOthers, Text::InterfaceChatColourHint, ""},  // 3.0.13-beta.3
        // 3.0.11-beta.240: the chat's fade when idle
        {"interface", "chat_fade", Page::Interface, Control::Choice, 0, 0, 0, {"12", "30", "0"},
         {Text::InterfaceChatFade12, Text::InterfaceChatFade30, Text::InterfaceChatFadeNever}, Text::InterfaceChatFade,
         Text::InterfaceChatFadeHint, ""},
        // 3.0.11-beta.237: a kill's experience over the monster
        {"interface", "exp_gain", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceExpGain,
         Text::InterfaceExpGainHint, ""},
        {"interface", "minimap_mobs", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapMobs, Text::InterfaceMinimapMobsHint, ""},
        // 3.0.11-beta.65: where one can walk from here - the concept's experiment
        {"interface", "minimap_reach", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapReach, Text::InterfaceMinimapReachHint, ""},
        // 3.0.11-beta.72: the maps' pictures four times bigger when the game has them
        {"interface", "minimap_hd", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapHd, Text::InterfaceMinimapHdHint, ""},
        // 3.0.11-beta.74: the panels' icons from the AI texture pack
        {"interface", "pack_icons", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfacePackIcons, Text::InterfacePackIconsHint, ""},
        // 3.0.11-beta.77: the parts kept where they are
        // 3.0.11-beta.78: each character's own hotkeys bar
        {"interface", "hotbar_per_character", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceHotbarOwn, Text::InterfaceHotbarOwnHint, ""},
        // 3.0.11-beta.125: the ammo left over the hotbar
        {"interface", "hotbar_ammo", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceHotbarAmmo, Text::InterfaceHotbarAmmoHint, ""},
        // 3.0.16-beta.8: the gems, stones and spheres the bar's skills spend
        {"interface", "hotbar_supplies", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceHotbarSupplies, Text::InterfaceHotbarSuppliesHint, ""},
        // 3.0.16-beta.13: what a skill lacks, the combo's window, broken gear, the pet's hunger
        {"interface", "skill_needs", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceSkillNeeds, Text::InterfaceSkillNeedsHint, ""},
        {"interface", "combo_window", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceComboWindow, Text::InterfaceComboWindowHint, ""},
        {"interface", "broken_gear", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceBrokenGear, Text::InterfaceBrokenGearHint, ""},
        {"interface", "pet_hunger", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfacePetHunger, Text::InterfacePetHungerHint, ""},
        {"interface", "ground_timers", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceGroundTimers, Text::InterfaceGroundTimersHint, ""},  // 3.0.16-beta.21
        // 3.0.11-beta.129: the chime at 10 arrows left and at none
        {"interface", "ammo_sound", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceAmmoSound, Text::InterfaceAmmoSoundHint, ""},
        {"interface", "element_colours", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // .144
         Text::InterfaceElementColours, Text::InterfaceElementColoursHint, ""},
        {"interface", "pad_hints", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.11-beta.86
         Text::InterfacePadHints, Text::InterfacePadHintsHint, ""},
        {"interface", "target_ring", Page::Interface, Control::Choice, 0, 0, 0, {"off", "pad", "always"},  // .87
         {Text::InterfaceTargetRingOff, Text::InterfaceTargetRingPad, Text::InterfaceTargetRingAlways},
         Text::InterfaceTargetRing, Text::InterfaceTargetRingHint, ""},
        {"interface", "mob_hp", Page::Interface, Control::Choice, 0, 0, 0, {"off", "target", "wounded", "all"},  // .95
         {Text::InterfaceMobHpOff, Text::InterfaceMobHpTarget, Text::InterfaceMobHpWounded, Text::InterfaceMobHpAll},
         Text::InterfaceMobHp, Text::InterfaceMobHpHint, ""},
        {"interface", "mob_hp_percent", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.11-beta.293
         Text::InterfaceMobHpPercent, Text::InterfaceMobHpPercentHint, ""},
        {"interface", "screen_shake", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.13-beta.8
         Text::InterfaceScreenShake, Text::InterfaceScreenShakeHint, ""},
        {"interface", "follow", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.13-beta.11
         Text::InterfaceFollow, Text::InterfaceFollowHint, ""},
        {"interface", "esc_cutin", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.13-beta.10
         Text::InterfaceEscCutin, Text::InterfaceEscCutinHint, ""},
        {"interface", "quick_teleport", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},  // 3.0.13-beta.7
         Text::InterfaceQuickTeleport, Text::InterfaceQuickTeleportHint, ""},
        {"interface", "who_hits", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},  // 3.0.13-beta.13
         Text::InterfaceWhoHits, Text::InterfaceWhoHitsHint, ""},
        {"interface", "boss_bars", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},  // 3.0.13-beta.3
         Text::InterfaceBossBars, Text::InterfaceBossBarsHint, ""},
        {"interface", "pad_tiles", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.11-beta.90
         Text::InterfacePadTiles, Text::InterfacePadTilesHint, ""},
        {"interface", "pad_walk", Page::Interface, Control::Choice, 0, 0, 0, {"mouse", "arrow"},  // 3.0.11-beta.88
         {Text::InterfacePadWalkMouse, Text::InterfacePadWalkArrow}, Text::InterfacePadWalk, Text::InterfacePadWalkHint,
         ""},
        // 3.0.11-beta.103: the look of the stick's arrow - the doc's variants
        // 3.0.18-beta.125 (the concept «Ігровий інтерфейс»: «Клавіші»): our windows' Ctrl
        // letters
        {"interface", "key_hunt", Page::Interface, Control::Choice, 0, 0, 0, kKeyTokens, kKeyChoices,
         Text::InterfaceKeyHunt, Text::InterfaceKeyHint, ""},
        {"interface", "key_mvp", Page::Interface, Control::Choice, 0, 0, 0, kKeyTokens, kKeyChoices,
         Text::InterfaceKeyMvp, Text::InterfaceKeyHint, ""},
        {"interface", "key_tracker", Page::Interface, Control::Choice, 0, 0, 0, kKeyTokens, kKeyChoices,
         Text::InterfaceKeyTracker, Text::InterfaceKeyHint, ""},
        {"interface", "key_party", Page::Interface, Control::Choice, 0, 0, 0, kKeyTokens, kKeyChoices,
         Text::InterfaceKeyParty, Text::InterfaceKeyHint, ""},
        {"interface", "key_recap", Page::Interface, Control::Choice, 0, 0, 0, kKeyTokens, kKeyChoices,
         Text::InterfaceKeyRecap, Text::InterfaceKeyHint, ""},
        {"interface", "arrows_walk", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.18-beta.207
         Text::InterfaceArrowsWalk, Text::InterfaceArrowsWalkHint, ""},
        {"interface", "walk_mode", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},  // 3.0.18-beta.208
         Text::InterfaceWalkMode, Text::InterfaceWalkModeHint, ""},
        {"interface", "space_action", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.18-beta.209
         Text::InterfaceSpaceAction, Text::InterfaceSpaceActionHint, ""},
        {"interface", "walk_arrow", Page::Interface, Control::Choice, 0, 0, 0,
         {"chevrons", "slim", "concept", "ring", "dots", "cell", "off"},
         {Text::WalkArrowChevrons, Text::WalkArrowSlim, Text::WalkArrowConcept, Text::WalkArrowRing,
          Text::WalkArrowDots, Text::WalkArrowCell, Text::WalkArrowOff},
         Text::InterfaceWalkArrow, Text::InterfaceWalkArrowHint, ""},
        // 3.0.11-beta.198: the cursor's look - the game's, the interface's dark or light
        {"interface", "cursor", Page::Interface, Control::Choice, 0, 0, 0, {"game", "dark", "light"},
         {Text::CursorGame, Text::CursorDark, Text::CursorLight}, Text::InterfaceCursor, Text::InterfaceCursorHint, ""},
        {"interface", "walk_mark", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},  // 3.0.11-beta.105
         Text::InterfaceWalkMark, Text::InterfaceWalkMarkHint, ""},
        // 3.0.11-beta.33: the way followed on the ground - footprints, a strip or none
        {"interface", "minimap_way", Page::Interface, Control::Choice, 0, 0, 0, {"steps", "line", "off"},
         {Text::InterfaceMinimapWaySteps, Text::InterfaceMinimapWayLine, Text::InterfaceMinimapWayOff},
         Text::InterfaceMinimapWay, Text::InterfaceMinimapWayHint, ""},
        // 3.0.11-beta.35: the buffs - the game's own icons, the driver's column, its row
        {"interface", "buffs", Page::Interface, Control::Choice, 0, 0, 0, {"off", "column", "row"},
         {Text::InterfaceBuffsOff, Text::InterfaceBuffsColumn, Text::InterfaceBuffsRow}, Text::InterfaceBuffs,
         Text::InterfaceBuffsHint, ""},
        // 3.0.11-beta.63: what the column does not hold - under «ще N» or in a second column
        {"interface", "buffs_overflow", Page::Interface, Control::Choice, 0, 0, 0, {"more", "column"},
         {Text::InterfaceBuffsOverflowMore, Text::InterfaceBuffsOverflowColumn}, Text::InterfaceBuffsOverflow,
         Text::InterfaceBuffsOverflowHint, ""},
        // 3.0.11-beta.42: the windows
        {"interface", "items", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceItems,
         Text::InterfaceItemsHint, ""},
        // 3.0.11-beta.186: the line on a thing picked up, in place of the game's box
        {"interface", "item_toast", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceItemToast,
         Text::InterfaceItemToastHint, ""},
        {"interface", "status", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceStatus,
         Text::InterfaceStatusHint, ""},
        {"interface", "skills", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceSkills,
         Text::InterfaceSkillsHint, ""},  // 3.0.11-beta.96
        {"interface", "equip", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceEquip,
         Text::InterfaceEquipHint, ""},  // 3.0.11-beta.97
        {"interface", "npc_talk", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceNpc,
         Text::InterfaceNpcHint, ""},  // 3.0.11-beta.98
        {"interface", "npc_remember", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceNpcRemember, Text::InterfaceNpcRememberHint, ""},  // 3.0.11-beta.255
        {"interface", "portraits", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfacePortraits, Text::InterfacePortraitsHint, ""},  // 3.0.11-beta.273
        {"interface", "char_carousel", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceCarousel, Text::InterfaceCarouselHint, ""},  // 3.0.11-beta.275
        {"interface", "delay_ring", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceDelayRing, Text::InterfaceDelayRingHint, ""},  // 3.0.11-beta.276
        {"interface", "death_recap", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceDeathRecap, Text::InterfaceDeathRecapHint, ""},  // 3.0.11-beta.277
        {"interface", "hunt_panel", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},
         Text::InterfaceHunt, Text::InterfaceHuntHint, ""},  // 3.0.11-beta.278
        {"interface", "mob_card", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMobCard, Text::InterfaceMobCardHint, ""},  // 3.0.11-beta.279
        {"interface", "mvp_timers", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMvp, Text::InterfaceMvpHint, ""},  // 3.0.11-beta.280
        {"interface", "potion_warn", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfacePotionWarn, Text::InterfacePotionWarnHint, ""},  // 3.0.11-beta.281
        {"interface", "party_alarm", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfacePartyAlarm, Text::InterfacePartyAlarmHint, ""},  // 3.0.11-beta.282
        {"interface", "drop_names", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceDropNames, Text::InterfaceDropNamesHint, ""},  // 3.0.11-beta.283
        {"interface", "drop_highlight", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceDropHighlight, Text::InterfaceDropHighlightHint, ""},  // 3.0.11-beta.283
        {"interface", "layout_per_char", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},
         Text::InterfaceLayoutPerChar, Text::InterfaceLayoutPerCharHint, ""},  // 3.0.11-beta.288
        {"interface", "homun_ai", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceHomunAi, Text::InterfaceHomunAiHint, ""},  // 3.0.11-beta.290
        {"interface", "homun_aggressive", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},
         Text::InterfaceHomunAggressive, Text::InterfaceHomunAggressiveHint, ""},
        {"interface", "homun_defend", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceHomunDefend, Text::InterfaceHomunDefendHint, ""},
        {"interface", "homun_skills", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceHomunSkills, Text::InterfaceHomunSkillsHint, ""},
        {"interface", "shop", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceShop,
         Text::InterfaceShopHint, ""},  // 3.0.11-beta.100
        {"interface", "storage", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceStorage,
         Text::InterfaceStorageHint, ""},  // 3.0.11-beta.115
        {"interface", "trade", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceTrade,
         Text::InterfaceTradeHint, ""},  // 3.0.11-beta.116
        {"interface", "menu", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceMenu,
         Text::InterfaceMenuHint, ""},  // 3.0.11-beta.140
        {"interface", "cards", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceCards,
         Text::InterfaceCardsHint, ""},  // 3.0.11-beta.149
        {"interface", "asks", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceAsks,
         Text::InterfaceAsksHint, ""},  // 3.0.11-beta.150
        {"interface", "messages", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceMessages,
         Text::InterfaceMessagesHint, ""},  // 3.0.11-beta.151
        {"interface", "login", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceLogin,
         Text::InterfaceLoginHint, ""},  // 3.0.11-beta.151
        {"interface", "login_status", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceLoginStatus, Text::InterfaceLoginStatusHint, ""},  // 3.0.11-beta.173
        {"interface", "login_news", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceLoginNews,
         Text::InterfaceLoginNewsHint, ""},  // 3.0.11-beta.174
        {"interface", "login_view", Page::Interface, Control::Choice, 0, 0, 1, {"classic", "shelf", "continue"},
         {Text::LoginViewClassic, Text::LoginViewShelf, Text::LoginViewContinue}, Text::InterfaceLoginView,
         Text::InterfaceLoginViewHint, ""},  // 3.0.11-beta.177
        {"interface", "char_select", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceCharSelect, Text::InterfaceCharSelectHint, ""},  // 3.0.11-beta.152
        {"interface", "char_make", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceCharMake, Text::InterfaceCharMakeHint, ""},  // 3.0.11-beta.153
        {"interface", "cart", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceCart,
         Text::InterfaceCartHint, ""},  // 3.0.11-beta.154
        {"interface", "counts", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceCounts,
         Text::InterfaceCountsHint, ""},  // 3.0.11-beta.155
        {"interface", "vending", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceVending,
         Text::InterfaceVendingHint, ""},  // 3.0.11-beta.156
        {"interface", "picks", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfacePicks,
         Text::InterfacePicksHint, ""},  // 3.0.11-beta.157
        {"interface", "party_setup", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfacePartySetup, Text::InterfacePartySetupHint, ""},  // 3.0.11-beta.158
        {"interface", "pet", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfacePet,
         Text::InterfacePetHint, ""},  // 3.0.11-beta.159
        {"interface", "homun", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceHomun,
         Text::InterfaceHomunHint, ""},  // 3.0.11-beta.160
        {"interface", "merc", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceMerc,
         Text::InterfaceMercHint, ""},  // 3.0.11-beta.161
        {"interface", "options", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceOptions,
         Text::InterfaceOptionsHint, ""},  // 3.0.11-beta.162
        {"interface", "social", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceSocial,
         Text::InterfaceSocialHint, ""},  // 3.0.11-beta.163
        {"interface", "servers", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceServers,
         Text::InterfaceServersHint, ""},  // 3.0.11-beta.164
        {"interface", "guild", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceGuild,
         Text::InterfaceGuildHint, ""},  // 3.0.11-beta.165
        {"interface", "my_shop", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceMyShop,
         Text::InterfaceMyShopHint, ""},  // 3.0.11-beta.166
        {"interface", "vend_setup", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceVendSetup, Text::InterfaceVendSetupHint, ""},  // 3.0.16-beta.33
        {"interface", "whisper_setup", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceWhisperSetup, Text::InterfaceWhisperSetupHint, ""},  // 3.0.11-beta.168
        {"interface", "macros", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceMacros,
         Text::InterfaceMacrosHint, ""},  // 3.0.11-beta.169
        {"interface", "tips", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceTips,
         Text::InterfaceTipsHint, ""},  // 3.0.11-beta.204
        {"interface", "loading_bar", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceLoadingBar, Text::InterfaceLoadingBarHint, ""},  // 3.0.18-beta.180
        {"interface", "emotes", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceEmotes,
         Text::InterfaceEmotesHint, ""},  // 3.0.11-beta.205
        {"interface", "commands", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceCommands,
         Text::InterfaceCommandsHint, ""},  // 3.0.11-beta.206
        {"interface", "chat_rooms", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceChatRooms, Text::InterfaceChatRoomsHint, ""},  // 3.0.11-beta.207
        {"interface", "detail_console", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceDetailConsole, Text::InterfaceDetailConsoleHint, ""},  // 3.0.11-beta.209
        {"interface", "popups", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfacePopups,
         Text::InterfacePopupsHint, ""},  // 3.0.11-beta.211
        {"interface", "whispers", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceWhispers,
         Text::InterfaceWhispersHint, ""},  // 3.0.11-beta.212
        {"interface", "skill_cells", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceSkillCells, Text::InterfaceSkillCellsHint, ""},  // 3.0.11-beta.142
        {"interface", "mouse_cell", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMouseCell, Text::InterfaceMouseCellHint, ""},  // 3.0.16-beta.7
        {"interface", "signs", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceSigns,
         Text::InterfaceSignsHint, ""},  // 3.0.16-beta.7
        {"interface", "ground_counts", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceGroundCounts, Text::InterfaceGroundCountsHint, ""},  // 3.0.11-beta.143
        // 3.0.11-beta.54: the new interface's size
        {"interface", "size", Page::Interface, Control::Choice, 0, 0, 0, {"auto", "70", "85", "100", "115", "130"},
         {Text::InterfaceSizeAuto, Text::InterfaceSize70, Text::InterfaceSize85, Text::InterfaceSize100,
          Text::InterfaceSize115, Text::InterfaceSize130},
         Text::InterfaceSize, Text::InterfaceSizeHint, ""},
        // 3.0.11-beta.67: the reading windows' text
        {"interface", "text_size", Page::Interface, Control::Choice, 0, 0, 0, {"80", "90", "100", "115", "130", "150"},
         {Text::InterfaceText80, Text::InterfaceText90, Text::InterfaceSize100, Text::InterfaceSize115,
          Text::InterfaceSize130, Text::InterfaceText150},
         Text::InterfaceTextSize, Text::InterfaceTextSizeHint, ""},
        // 3.0.15-beta.9: «Речі», the storage and the cart smaller
        {"interface", "item_windows", Page::Interface, Control::Choice, 0, 0, 0, {"70", "85", "100"},
         {Text::InterfaceSize70, Text::InterfaceSize85, Text::InterfaceSize100}, Text::InterfaceItemWindows,
         Text::InterfaceItemWindowsHint, ""},
        // 3.0.11-beta.50: the screen's edges red at low HP
        {"interface", "low_hp_edge", Page::Interface, Control::Slider, 0, 100, 10, {}, {}, Text::InterfaceLowHpEdge,
         Text::InterfaceLowHpEdgeHint, " %"},
        // 3.0.11-beta.49: the world dimmed under the windows
        // 3.0.11-beta.48: the cast's label over the hero
        {"interface", "cast", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceCast,
         Text::InterfaceCastHint, ""},
        // 3.0.11-beta.47: the new interface's grounds
        {"interface", "panel_opacity", Page::Interface, Control::Slider, 50, 100, 5, {}, {},
         Text::InterfacePanelOpacity, Text::InterfacePanelOpacityHint, " %"},
        {"interface", "window_opacity", Page::Interface, Control::Slider, 80, 100, 5, {}, {},
         Text::InterfaceWindowOpacity, Text::InterfaceWindowOpacityHint, " %"},
        // 3.0.11-beta.46: the new interface's motion
        {"interface", "animations", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceAnimations,
         Text::InterfaceAnimationsHint, ""},
        // 3.0.18-beta.7: the RmlUi spike's card (win/uaui)
        // 3.0.18-beta.13: uaUI's theme
        {"interface", "theme", Page::Interface, Control::Choice, 0, 0, 0, {"standard", "classic", "contrast"},
         {Text::InterfaceThemeStandard, Text::InterfaceThemeClassic, Text::InterfaceThemeContrast},
         Text::InterfaceTheme, Text::InterfaceThemeHint, ""},
        // 3.0.18-beta.58
        {"interface", "uaui_motion", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},
         Text::InterfaceUauiMotion, Text::InterfaceUauiMotionHint, ""},
        // 3.0.18-beta.61
        {"interface", "uaui_effects", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},
         Text::InterfaceUauiEffects, Text::InterfaceUauiEffectsHint, ""},
        // 3.0.18-beta.64
        {"interface", "uaui_glass", Page::Interface, Control::Check, 0, 1, 0, {"0", "1"}, {},
         Text::InterfaceUauiGlass, Text::InterfaceUauiGlassHint, ""},
        // 3.0.18-beta.11: uaUI's components' window
        {"interface", "uaui_showcase", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceUauiShowcase, Text::InterfaceUauiShowcaseHint, ""},
        // 3.0.11-beta.41: the chat
        {"interface", "chat", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceChat,
         Text::InterfaceChatHint, ""},
        // 3.0.11-beta.40: the hotkeys' bar
        {"interface", "hotbar", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceHotbar,
         Text::InterfaceHotbarHint, ""},
        // 3.0.11-beta.70: the tracker - the pinned quests, way and items
        {"interface", "tracker", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceTracker,
         Text::InterfaceTrackerHint, ""},
        // 3.0.11-beta.39: the party's panel
        {"interface", "party", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceParty,
         Text::InterfacePartyHint, ""},
        // 3.0.11-beta.38: the target's card
        {"interface", "target", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceTarget,
         Text::InterfaceTargetHint, ""},
        // 3.0.11-beta.36: the character block - the game's own window, with bars, with rings
        {"interface", "character", Page::Interface, Control::Choice, 0, 0, 0, {"off", "bars", "rings"},
         {Text::InterfaceCharacterOff, Text::InterfaceCharacterBars, Text::InterfaceCharacterRings},
         Text::InterfaceCharacter, Text::InterfaceCharacterHint, ""},
        // 3.0.11-beta.7: «Довідник» - Ctrl+F in the game
        {"interface", "reference", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceReference,
         Text::InterfaceReferenceHint, ""},
        // 3.0.3-beta.8: the game windows' panel in the game's picture - a section of its own («Панель вікон»)
        {"panel", "show", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsPanel,
         Text::WindowsPanelHint, ""},
        {"panel", "layout", Page::Game, Control::Choice, 0, 0, 0, {"row", "column", "icons"},  // 3.0.3-beta.11: icons
         {Text::WindowsPanelRow, Text::WindowsPanelColumn, Text::WindowsPanelIcons}, Text::WindowsPanelLayout,
         Text::WindowsPanelLayoutHint, ""},
        {"panel", "size", Page::Game, Control::Slider, 75, 150, 25, {}, {}, Text::WindowsPanelSize,
         Text::WindowsPanelSizeHint, " %"},
        {"panel", "opacity", Page::Game, Control::Slider, 40, 100, 10, {}, {}, Text::PanelOpacity,
         Text::PanelOpacityHint, " %"},
        // 3.0.4-beta.5: the camera - building maps as free as outdoors (a section of its own, «Камера»)
        {"camera", "indoor", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::CameraIndoor,
         Text::CameraIndoorHint, ""},
        // 3.0.4-beta.17: the camera no further out than the character is drawn (R-35N5QN)
        {"camera", "zoom_limit", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::CameraZoomLimit,
         Text::CameraZoomLimitHint, ""},
        // 3.0.11-beta.184 (R-ANMTEJ): the camera's zoom and tilt kept between the games
        {"camera", "remember", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::CameraRemember,
         Text::CameraRememberHint, ""},
        // 3.0.13-beta.5 (the owner's pick 15 of «50 идей»): how near the horizon Shift tilts the camera
        {"camera", "tilt", Page::Game, Control::Choice, 0, 0, 0, {"game", "lower", "ground"},
         {Text::CameraTiltGame, Text::CameraTiltLower, Text::CameraTiltGround}, Text::CameraTilt,
         Text::CameraTiltHint, ""},
        // 3.0.4-beta.6: the time left on the buff icons
        {"buffs", "timers", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::BuffTimers,
         Text::BuffTimersHint, ""},
        // 3.0.4-beta.3: notifications while the game is behind other windows - a section of its own («Сповіщення»)
        {"notify", "on", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyOn, Text::NotifyOnHint, ""},
        {"notify", "whisper", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyWhisper,
         Text::NotifyKindHint, ""},
        {"notify", "party", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyParty,
         Text::NotifyKindHint, ""},
        {"notify", "trade", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyTrade,
         Text::NotifyKindHint, ""},
        {"notify", "guild", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyGuild,
         Text::NotifyKindHint, ""},
        {"notify", "friend", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyFriend,
         Text::NotifyKindHint, ""},
        {"notify", "death", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyDeath,
         Text::NotifyKindHint, ""},
        // 3.0.7-beta.6 (the user chose 1, 5, 6, 7, 8, 9, 10 «и чтобы это в конфигураторе настраивалось»)
        {"notify", "attack", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyAttack,
         Text::NotifyAttackHint, ""},
        // 3.0.7-beta.7 (the user: «надо сделать чтобы когда хп и сп восстановились»)
        {"notify", "recovered", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyRecovered,
         Text::NotifyRecoveredHint, ""},
        {"notify", "recovered_percent", Page::Game, Control::Slider, kRecoveredLeast, kRecoveredMost, 5, {}, {},
         Text::NotifyRecoveredPercent, Text::NotifyRecoveredPercentHint, " %"},
        {"notify", "buff_ending", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyBuffEnding,
         Text::NotifyBuffEndingHint, ""},
        {"notify", "buff_seconds", Page::Game, Control::Slider, kBuffSecondsLeast, kBuffSecondsMost, 1, {}, {},
         Text::NotifyBuffSeconds, Text::NotifyBuffSecondsHint, " s"},
        {"notify", "party_death", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyPartyDeath,
         Text::NotifyPartyDeathHint, ""},
        {"notify", "party_low_hp", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyPartyLowHp,
         Text::NotifyPartyLowHpHint, ""},
        {"notify", "party_hp", Page::Game, Control::Slider, kPartyHpLeast, kPartyHpMost, 5, {}, {},
         Text::NotifyPartyHp, Text::NotifyPartyHpHint, " %"},
        {"notify", "party_chat", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyPartyChat,
         Text::NotifyChatHint, ""},
        {"notify", "guild_chat", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyGuildChat,
         Text::NotifyChatHint, ""},
        {"notify", "mention", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyMention,
         Text::NotifyMentionHint, ""},
        {"notify", "disconnect", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyDisconnect,
         Text::NotifyDisconnectHint, ""},
        {"notify", "sound", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifySound,
         Text::NotifySoundHint, ""},
        // 3.0.9-beta.4: the developer answered a report
        {"notify", "report_answer", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyAnswer,
         Text::NotifyAnswerHint, ""},
        {"renderer", "bridge", Page::Render, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderBridge,
         Text::RenderBridgeHint, ""},
        // 2.12: Direct3D 11 without the risky parts, also by itself after two failed starts (core/safe_mode)
        {"renderer", "safe_mode", Page::Render, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::RenderSafeMode,
         Text::RenderSafeModeHint, ""},
        {"renderer", "render_thread", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderThread,
         Text::RenderThreadHint, ""},  // 2.5.12
        {"renderer", "remix", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderRemix,
         Text::RenderRemixHint, ""},
        {"quality", "super_light", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualitySuperLight,
         Text::QualitySuperLightHint, ""},  // 2.5.20: first - over the rest of the page
        {"display", "high_resolution", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityHighRes,
         Text::QualityHighResHint, ""},
        {"display", "dpi_aware", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityDpiAware,
         Text::QualityDpiAwareHint, ""},  // 3.0.11-beta.292
        {"quality", "render_scale", Page::Quality, Control::Slider, 50, 200, 10, {}, {}, Text::QualityRenderScale,
         Text::QualityRenderScaleHint, " %"},
        {"quality", "world_scale", Page::Quality, Control::Slider, 50, 100, 5, {}, {}, Text::QualityWorldScale,
         Text::QualityWorldScaleHint, " %"},  // 2.5.16
        {"quality", "full_colour", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityFullColour,
         Text::QualityFullColourHint, ""},  // 2.5.18
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
        {"display", "crisp_numbers", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {},  // 3.0.18-beta.178
         Text::QualityCrispNumbers, Text::QualityCrispNumbersHint, ""},
        {"display", "window_art", Page::Quality, Control::Choice, 0, 0, 0, {"xbr", "ai", "pixels"},
         {Text::WindowArtXbr, Text::WindowArtAi, Text::WindowArtPixels}, Text::QualityWindowArt,
         Text::QualityWindowArtHint, ""},
        {"quality", "dlss", Page::Smoothing, Control::Choice, 0, 0, 0,
         {"off", "dlaa", "quality", "balanced", "performance", "ultra", "custom"},
         {Text::SmoothOff, Text::DlssDlaa, Text::DlssQuality, Text::DlssBalanced, Text::DlssPerformance,
          Text::DlssUltra, Text::DlssCustom},
         Text::SmoothDlss, Text::SmoothDlssHint, ""},
        // 2.4.2: DLSS at the player's own size and its network
        {"quality", "dlss_scale", Page::Smoothing, Control::Slider, 33, 100, 1, {}, {}, Text::DlssScale,
         Text::DlssScaleHint, " %"},
        {"quality", "dlss_width", Page::Smoothing, Control::Number, 0, 16384, 1, {}, {}, Text::DlssWidth,
         Text::DlssSizeHint, ""},
        {"quality", "dlss_height", Page::Smoothing, Control::Number, 0, 16384, 1, {}, {}, Text::DlssHeight,
         Text::DlssSizeHint, ""},
        {"quality", "dlss_model", Page::Smoothing, Control::Choice, 0, 0, 0, {"auto", "k", "j", "l", "m"},
         {Text::DlssModelAuto, Text::DlssModelK, Text::DlssModelJ, Text::DlssModelL, Text::DlssModelM},
         Text::DlssModel, Text::DlssModelHint, ""},
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
         Text::FxSmoothLightHint, "", &E::smoothLight},
        {"effects", "torch", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTorch, Text::FxTorchHint,
         "", &E::torch},
        {"effects", "torch_flame", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTorchFlame,
         Text::FxTorchFlameHint, "", &E::torchFlame},
        {"effects", "torch_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxTorchBrightness,
         Text::FxTorchBrightnessHint, " %", nullptr, &E::torchBrightness},
        {"effects", "torch_glow", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxTorchGlow,
         Text::FxTorchGlowHint, " %", nullptr, &E::torchGlow},
        {"effects", "torch_light", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxTorchLight,
         Text::FxTorchLightHint, " %", nullptr, &E::torchLight},
        {"effects", "torch_flicker", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxTorchFlicker,
         Text::FxTorchFlickerHint, " %", nullptr, &E::torchFlicker},
        // 3.0.16-beta.16: the torches' and the lamps' shadows (core/point_shadows.hpp)
        {"effects", "torch_shadows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTorchShadows,
         Text::FxTorchShadowsHint, "", &E::torchShadows},
        {"effects", "torch_shadows_strength", Page::Effects, Control::Slider, 0, 100, 10, {}, {},
         Text::FxTorchShadowsStrength, Text::FxTorchShadowsStrengthHint, " %", nullptr, &E::torchShadowsStrength},
        {"effects", "torch_leaf_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxTorchLeafLight, Text::FxTorchLeafLightHint, "", &E::torchLeafLight},  // 3.0.16-beta.30
        {"effects", "smoke", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSmoke, Text::FxSmokeHint,
         "", &E::smoke},
        {"effects", "smoke_density", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxSmokeDensity,
         Text::FxSmokeDensityHint, " %", nullptr, &E::smokeDensity},
        {"effects", "lamps", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLamps, Text::FxLampsHint,
         "", &E::lamps},
        {"effects", "lamps_light", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxLampsLight,
         Text::FxLampsLightHint, " %", nullptr, &E::lampsLight},
        {"effects", "ao", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxAo, Text::FxAoHint, "",
         &E::ao},
        {"effects", "ao_strength", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxAoStrength,
         Text::FxAoStrengthHint, " %", nullptr, &E::aoStrength},
        {"effects", "ao_size", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxAoSize, Text::FxAoSizeHint,
         " %", nullptr, &E::aoSize},
        {"effects", "glow", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGlow, Text::FxGlowHint,
         "", &E::glow},
        {"effects", "glow_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxGlowStrength,
         Text::FxGlowStrengthHint, " %", nullptr, &E::glowStrength},
        {"effects", "glow_size", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxGlowSize,
         Text::FxGlowSizeHint, " %", nullptr, &E::glowSize},
        {"effects", "glow_light", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxGlowLight,
         Text::FxGlowLightHint, " %", nullptr, &E::glowLight},
        {"effects", "fire_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFireLight,
         Text::FxFireLightHint, "", &E::fireLight},
        {"effects", "fire_light_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxFireLightStrength, Text::FxFireLightStrengthHint, " %", nullptr, &E::fireLightStrength},
        {"effects", "skill_colours", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkillColours,
         Text::FxSkillColoursHint, "", &E::skillColours},
        {"effects", "light_orbs", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLightOrbs,
         Text::FxLightOrbsHint, "", &E::lightOrbs},  // 2.7.25: a part of fire_light
        {"effects", "aura", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxAura, Text::FxAuraHint,
         "", &E::aura},
        {"effects", "crowd_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxCrowdEffects,
         Text::FxCrowdEffectsHint, "", &E::crowdEffects},  // 3.0.13-beta.20
        {"effects", "mage_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxMage,
         Text::FxMageHint, "", &E::mageEffects},  // 3.0.15-beta.11
        {"effects", "mage_casting", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMageCasting,
         Text::FxMageCastingHint, "", &E::mageCasting},
        {"effects", "mage_fire", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMageFire,
         Text::FxMageFireHint, "", &E::mageFire},
        {"effects", "mage_water", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMageWater,
         Text::FxMageWaterHint, "", &E::mageWater},
        {"effects", "mage_wind", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMageWind,
         Text::FxMageWindHint, "", &E::mageWind},
        {"effects", "mage_earth", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMageEarth,
         Text::FxMageEarthHint, "", &E::mageEarth},
        {"effects", "mage_spirit", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMageSpirit,
         Text::FxMageSpiritHint, "", &E::mageSpirit},
        {"effects", "mage_soft", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMageSoft,
         Text::FxMageSoftHint, "", &E::mageSoft},  // 3.0.16-beta.27
        {"effects", "crisp_storm_gust", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxCrispStormGust, Text::FxCrispStormGustHint, "", &E::crispStormGust},  // 3.0.18-beta.181
        {"effects", "sword_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxSword,
         Text::FxSwordHint, "", &E::swordEffects},  // 3.0.15-beta.12
        {"effects", "acolyte_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxAcolyte,
         Text::FxAcolyteHint, "", &E::acolyteEffects},  // 3.0.15-beta.12
        {"effects", "archer_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxArcher,
         Text::FxArcherHint, "", &E::archerEffects},  // 3.0.15-beta.12
        {"effects", "thief_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxThief,
         Text::FxThiefHint, "", &E::thiefEffects},  // 3.0.15-beta.12
        {"effects", "merchant_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxMerchant,
         Text::FxMerchantHint, "", &E::merchantEffects},  // 3.0.15-beta.12
        {"effects", "other_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxOther,
         Text::FxOtherHint, "", &E::otherEffects},  // 3.0.15-beta.12
        {"effects", "hit_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxHit,
         Text::FxHitHint, "", &E::hitEffects},  // 3.0.16-beta.1
        {"effects", "item_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxItem,
         Text::FxItemHint, "", &E::itemEffects},  // 3.0.16-beta.1
        {"effects", "status_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxStatus,
         Text::FxStatusHint, "", &E::statusEffects},  // 3.0.16-beta.1
        {"effects", "travel_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxTravel,
         Text::FxTravelHint, "", &E::travelEffects},  // 3.0.16-beta.1
        {"effects", "monster_effects", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxMonster,
         Text::FxMonsterHint, "", &E::monsterEffects},  // 3.0.16-beta.1
        {"effects", "silhouette", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxSilhouette,
         Text::FxSilhouetteHint, "", &E::silhouette},  // 3.0.13-beta.30
        {"effects", "aura_portals", Page::Effects, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::FxAuraPortals,
         Text::FxAuraPortalsHint, "", &E::auraPortals},  // 3.0.13-beta.15
        {"effects", "aura_brightness", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxAuraBrightness,
         Text::FxAuraBrightnessHint, " %", nullptr, &E::auraBrightness},
        {"effects", "skills_soft", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkillsSoft,
         Text::FxSkillsSoftHint, "", &E::skillsSoft},
        {"effects", "skill_brightness", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxSkillBrightness,
         Text::FxSkillBrightnessHint, " %", nullptr, &E::skillBrightness},
        {"effects", "fireflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFireflies,
         Text::FxFirefliesHint, "", &E::fireflies},
        {"effects", "fireflies_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxFirefliesBrightness, Text::FxFirefliesBrightnessHint, " %", nullptr, &E::firefliesBrightness},
        {"effects", "firefly_dots", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFireflyDots,
         Text::FxFireflyDotsHint, "", &E::fireflyDots},  // 3.0.7-beta.3: a part of fireflies
        {"effects", "map_glows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMapGlows,
         Text::FxMapGlowsHint, "", &E::mapGlows},
        {"effects", "map_glows_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxMapGlowsBrightness, Text::FxMapGlowsBrightnessHint, " %", nullptr, &E::mapGlowsBrightness},
        {"effects", "map_glow_orbs", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMapGlowOrbs,
         Text::FxMapGlowOrbsHint, "", &E::mapGlowOrbs},  // 3.0.7-beta.1: a part of map_glows
        {"effects", "rays", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRays, Text::FxRaysHint,
         "", &E::rays},
        {"effects", "rays_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxRaysBrightness,
         Text::FxRaysBrightnessHint, " %", nullptr, &E::raysBrightness},
        {"effects", "lava", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLava, Text::FxLavaHint,
         "", &E::lava},
        {"effects", "lava_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxLavaBrightness,
         Text::FxLavaBrightnessHint, " %", nullptr, &E::lavaBrightness},
        {"effects", "actor_shadows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxActorShadows,
         Text::FxActorShadowsHint, "", &E::actorShadows},
        {"effects", "actor_shadows_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {},
         Text::FxActorShadowsStrength, Text::FxActorShadowsStrengthHint, " %", nullptr, &E::actorShadowsStrength},
        {"effects", "actor_shadows_spot", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxActorShadowsSpot, Text::FxActorShadowsSpotHint, "", &E::actorShadowsSpot},  // 2.6.15
        {"effects", "heat", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxHeat, Text::FxHeatHint,
         "", &E::heat},
        {"effects", "heat_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxHeatStrength,
         Text::FxHeatStrengthHint, " %", nullptr, &E::heatStrength},
        {"effects", "portals", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxPortals,
         Text::FxPortalsHint, "", &E::portals},
        {"effects", "portals_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxPortalsStrength,
         Text::FxPortalsStrengthHint, " %", nullptr, &E::portalsStrength},
        {"effects", "portal_new", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxPortalNew,
         Text::FxPortalNewHint, "", &E::portalNew},  // 2.7.48: a part of portals
        {"effects", "weather", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWeather,
         Text::FxWeatherHint, "", &E::weather},
        {"effects", "weather_amount", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxWeatherAmount,
         Text::FxWeatherAmountHint, " %", nullptr, &E::weatherAmount},
        {"effects", "grade", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGrade, Text::FxGradeHint,
         "", &E::grade},
        {"effects", "grade_saturation", Page::Effects, Control::Slider, 0, 200, 5, {}, {}, Text::FxGradeSaturation,
         Text::FxGradeSaturationHint, " %", nullptr, &E::gradeSaturation},
        {"effects", "grade_contrast", Page::Effects, Control::Slider, 50, 150, 2, {}, {}, Text::FxGradeContrast,
         Text::FxGradeContrastHint, " %", nullptr, &E::gradeContrast},
        {"effects", "grade_brightness", Page::Effects, Control::Slider, 50, 150, 2, {}, {}, Text::FxGradeBrightness,
         Text::FxGradeBrightnessHint, " %", nullptr, &E::gradeBrightness},
        {"effects", "grade_warmth", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxGradeWarmth,
         Text::FxGradeWarmthHint, " %", nullptr, &E::gradeWarmth},
        {"effects", "grade_vignette", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxGradeVignette,
         Text::FxGradeVignetteHint, " %", nullptr, &E::gradeVignette},
        {"effects", "foliage", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFoliage,
         Text::FxFoliageHint, "", &E::foliage},
        {"effects", "foliage_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxFoliageStrength,
         Text::FxFoliageStrengthHint, " %", nullptr, &E::foliageStrength},
        {"effects", "soft_particles", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSoftParticles,
         Text::FxSoftParticlesHint, "", &E::softParticles},
        {"effects", "rim_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRimLight,
         Text::FxRimLightHint, "", &E::rimLight},
        {"effects", "rim_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxRimStrength,
         Text::FxRimStrengthHint, " %", nullptr, &E::rimStrength},
        {"effects", "motes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMotes,
         Text::FxMotesHint, "", &E::motes},
        {"effects", "motes_amount", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxMotesAmount,
         Text::FxMotesAmountHint, " %", nullptr, &E::motesAmount},
        {"effects", "relief", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRelief,
         Text::FxReliefHint, "", &E::relief},
        {"effects", "relief_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxReliefStrength,
         Text::FxReliefStrengthHint, " %", nullptr, &E::reliefStrength},
        {"effects", "floor_reflections", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxFloorReflections, Text::FxFloorReflectionsHint, "", &E::floorReflections},
        {"effects", "floor_reflect_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {},
         Text::FxFloorReflectStrength, Text::FxFloorReflectStrengthHint, " %", nullptr, &E::floorReflectStrength},
        {"effects", "shafts", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxShafts,
         Text::FxShaftsHint, "", &E::shafts},
        {"effects", "shafts_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxShaftsStrength,
         Text::FxShaftsStrengthHint, " %", nullptr, &E::shaftsStrength},
        {"effects", "shafts_lamps", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxShaftsLamps,
         Text::FxShaftsLampsHint, "", &E::shaftsLamps},  // 3.0.7-beta.5: a part of shafts
        {"effects", "day_cycle", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDayCycle,
         Text::FxDayCycleHint, "", &E::dayCycle},
        {"effects", "hallucination_wave", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxHallucinationWave, Text::FxHallucinationWaveHint, "", &E::hallucinationWave},  // 3.0.16-beta.37
        {"effects", "bloom", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxBloom,
         Text::FxBloomHint, "", &E::bloom},
        {"effects", "bloom_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxBloomStrength,
         Text::FxBloomStrengthHint, " %", nullptr, &E::bloomStrength},
        {"effects", "region_looks", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRegionLooks,
         Text::FxRegionLooksHint, "", &E::regionLooks},
        {"effects", "region_looks_strength", Page::Effects, Control::Slider, 0, 100, 10, {}, {},
         Text::FxRegionLooksStrength, Text::FxRegionLooksStrengthHint, " %", nullptr, &E::regionLooksStrength},
        {"effects", "footprints", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFootprints,
         Text::FxFootprintsHint, "", &E::footprints},
        {"effects", "storms", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxStorms,
         Text::FxStormsHint, "", &E::storms},
        {"effects", "grass", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGrass,
         Text::FxGrassHint, "", &E::grass},
        {"effects", "grass_amount", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxGrassAmount,
         Text::FxGrassAmountHint, " %", nullptr, &E::grassAmount},
        {"effects", "skill_sparks", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkillSparks,
         Text::FxSkillSparksHint, "", &E::skillSparks},
        {"effects", "skill_sparks_amount", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxSkillSparksAmount, Text::FxSkillSparksAmountHint, " %", nullptr, &E::skillSparksAmount},
        {"effects", "map_fade", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMapFade,
         Text::FxMapFadeHint, "", &E::mapFade},
        {"effects", "no_game_fog", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxNoGameFog,
         Text::FxNoGameFogHint, "", &E::noGameFog},  // 2.6.10
        {"effects", "no_server_night", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxNoServerNight,
         Text::FxNoServerNightHint, "", &E::noServerNight},  // 2.6.13
        {"effects", "fix_negative", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFixNegative,
         Text::FxFixNegativeHint, "", &E::fixNegative},  // 3.0.18-beta.229
        {"effects", "moon_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMoonLight,
         Text::FxMoonLightHint, "", &E::moonLight},  // 2.7.18
        {"effects", "moon_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxMoonStrength,
         Text::FxMoonStrengthHint, " %", nullptr, &E::moonStrength},
        {"effects", "fireflies_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFirefliesLight,
         Text::FxFirefliesLightHint, "", &E::firefliesLight},  // 2.7.18: a part of night_fireflies
        {"effects", "moon_beams", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMoonBeams,
         Text::FxMoonBeamsHint, "", &E::moonBeams},  // 2.7.20: a part of moon_light
        {"effects", "round_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRoundLight,
         Text::FxRoundLightHint, "", &E::roundLight},
        {"effects", "ripples", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRipples,
         Text::FxRipplesHint, "", &E::ripples},
        {"effects", "glint", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGlint,
         Text::FxGlintHint, "", &E::glint},
        {"effects", "hit_flash", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxHitFlash,
         Text::FxHitFlashHint, "", &E::hitFlash},
        {"effects", "death_flash", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDeathFlash,
         Text::FxDeathFlashHint, "", &E::deathFlash},  // 3.0.16-beta.7
        // 3.0.16-beta.13: the body's tints (.14: the afterimages gone - the owner: «убери»)
        {"effects", "body_tints", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxBodyTints,
         Text::FxBodyTintsHint, "", &E::bodyTints},
        {"effects", "body_stamp", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxBodyStamp,
         Text::FxBodyStampHint, "", &E::bodyStamp},  // 3.0.16-beta.25
        {"effects", "sharp_sprites", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSharpSprites,
         Text::FxSharpSpritesHint, "", &E::sharpSprites},  // 3.0.16-beta.29
        {"effects", "wet_ground", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWetGround,
         Text::FxWetGroundHint, "", &E::wetGround},
        {"effects", "tree_volume", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTreeVolume,
         Text::FxTreeVolumeHint, "", &E::treeVolume},
        {"effects", "leaf_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLeafLight,
         Text::FxLeafLightHint, "", &E::leafLight},
        {"effects", "tree_tints", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTreeTints,
         Text::FxTreeTintsHint, "", &E::treeTints},
        {"effects", "flowers", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFlowers,
         Text::FxFlowersHint, "", &E::flowers},
        {"effects", "dew", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDew,
         Text::FxDewHint, "", &E::dew},
        {"effects", "morning_fog", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMorningFog,
         Text::FxMorningFogHint, "", &E::morningFog},
        {"effects", "sand_drift", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSandDrift,
         Text::FxSandDriftHint, "", &E::sandDrift},
        {"effects", "wet_walls", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWetWalls,
         Text::FxWetWallsHint, "", &E::wetWalls},
        {"effects", "swamp", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSwamp,
         Text::FxSwampHint, "", &E::swamp},
        {"effects", "butterflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxButterflies,
         Text::FxButterfliesHint, "", &E::butterflies},
        {"effects", "birds", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxBirds,
         Text::FxBirdsHint, "", &E::birds},
        {"effects", "night_fireflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxNightFireflies, Text::FxNightFirefliesHint, "", &E::nightFireflies},  // 2.6.11
        {"effects", "leaf_fall", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLeafFall,
         Text::FxLeafFallHint, "", &E::leafFall},
        {"effects", "embers", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxEmbers,
         Text::FxEmbersHint, "", &E::embers},
        {"effects", "splashes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSplashes,
         Text::FxSplashesHint, "", &E::splashes},
        {"effects", "cave_drips", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxCaveDrips,
         Text::FxCaveDripsHint, "", &E::caveDrips},
        {"effects", "underwater", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxUnderwater,
         Text::FxUnderwaterHint, "", &E::underwater},
        // 2.2: the set for the card, the groups and their strength (core/effect_groups.hpp)
        {"effects", "preset", Page::Effects, Control::Choice, 0, 0, 0,
         {"auto", "off", "weak", "medium", "strong", "custom"},
         {Text::FxPresetAuto, Text::FxPresetOff, Text::FxPresetWeak, Text::FxPresetMedium, Text::FxPresetStrong,
          Text::FxPresetCustom},
         Text::FxPreset, Text::FxPresetHint, ""},
        // 2.6.22: the groups by what the eye is on (core/effect_groups.hpp), a strength where they have one
        {"effects", "group_time", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupTime,
         Text::FxGroupTimeHint, ""},
        {"effects", "group_weather", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupWeather,
         Text::FxGroupWeatherHint, ""},
        {"effects", "group_weather_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_characters", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupCharacters,
         Text::FxGroupCharactersHint, ""},
        {"effects", "group_characters_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_skill_effects", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxGroupSkillEffects, Text::FxGroupSkillEffectsHint, ""},
        {"effects", "group_skill_effects_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_fire", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupFire,
         Text::FxGroupFireHint, ""},
        {"effects", "group_fire_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_lights", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupLights,
         Text::FxGroupLightsHint, ""},
        {"effects", "group_lights_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_ground", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupGround,
         Text::FxGroupGroundHint, ""},
        {"effects", "group_ground_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_plants", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupPlants,
         Text::FxGroupPlantsHint, ""},
        {"effects", "group_plants_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_waters", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupWaters,
         Text::FxGroupWatersHint, ""},
        {"effects", "group_waters_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_creatures", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupCreatures,
         Text::FxGroupCreaturesHint, ""},
        {"effects", "group_caves", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupCaves,
         Text::FxGroupCavesHint, ""},
        {"effects", "group_sky", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupSky,
         Text::FxGroupSkyHint, ""},
        {"effects", "group_sky_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_screen", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupScreen,
         Text::FxGroupScreenHint, ""},
        {"effects", "group_screen_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_game_fixes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupGameFixes,
         Text::FxGroupGameFixesHint, ""},
        {"effects", "tier", Page::Effects, Control::Choice, 0, 0, 0, {"auto", "classic", "remaster"},
         {Text::FxTierAuto, Text::FxTierClassic, Text::FxTierRemaster}, Text::FxTier, Text::FxTierHint, ""},
        // 2.6.11: the time of day for all that goes by the hour (core/daylight.hpp Clock)
        {"effects", "clock", Page::Effects, Control::Choice, 0, 0, 0,
         {"auto", "server", "dawn", "morning", "day", "sunset", "night", "deep_night"},  // 2.6.19, 2.7.16
         {Text::FxClockAuto, Text::FxClockServer, Text::FxClockDawn, Text::FxClockMorning, Text::FxClockDay,
          Text::FxClockSunset, Text::FxClockNight, Text::FxClockDeepNight},
         Text::FxClock, Text::FxClockHint, ""},
        // 3.0.18-beta.183: every class's skills - the game's own, the game's sharp, redrawn, redrawn soft
        {"effects", "skill_looks", Page::Effects, Control::Choice, 0, 0, 0, {"original", "improved", "redrawn", "soft"},
         {Text::FxSkillLooksOriginal, Text::FxSkillLooksImproved, Text::FxSkillLooksRedrawn, Text::FxSkillLooksSoft},
         Text::FxSkillLooks, Text::FxSkillLooksHint, ""},
        // 2.6.22: the season for all that goes by the year (core/seasons.hpp Choice)
        {"effects", "season", Page::Effects, Control::Choice, 0, 0, 0, {"auto", "spring", "summer", "autumn", "winter"},
         {Text::FxSeasonAuto, Text::FxSeasonSpring, Text::FxSeasonSummer, Text::FxSeasonAutumn, Text::FxSeasonWinter},
         Text::FxSeason, Text::FxSeasonHint, ""},
        // 2.6.22: the weather for all that goes by it (core/weather.hpp Choice)
        {"effects", "weather_kind", Page::Effects, Control::Choice, 0, 0, 0,
         {"auto", "clear", "rain", "snow", "leaves", "sand"},
         {Text::FxWeatherKindAuto, Text::FxWeatherKindClear, Text::FxWeatherKindRain, Text::FxWeatherKindSnow,
          Text::FxWeatherKindLeaves, Text::FxWeatherKindSand},
         Text::FxWeatherKind, Text::FxWeatherKindHint, ""},
        {"effects", "materials", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMaterials,
         Text::FxMaterialsHint, "", &E::materials},
        {"effects", "materials_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxMaterialsStrength,
         Text::FxMaterialsStrengthHint, " %", nullptr, &E::materialsStrength},
        {"effects", "ground_seams", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroundSeams,
         Text::FxGroundSeamsHint, "", &E::groundSeams},
        {"effects", "smooth_hills", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSmoothHills,
         Text::FxSmoothHillsHint, "", &E::smoothHills},
        {"effects", "smooth_hills_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxSmoothHillsStrength, Text::FxSmoothHillsStrengthHint, " %", nullptr, &E::smoothHillsStrength},
        {"effects", "own_ground", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxOwnGround,
         Text::FxOwnGroundHint, "", &E::ownGround},  // 3.0.4-beta.10: the own world's prototype
        {"effects", "own_far_models", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxOwnFarModels,
         Text::FxOwnFarModelsHint, "", &E::ownFarModels},  // 3.0.4-beta.14: a part of own_ground
        {"effects", "world_reflections", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxWorldReflections, Text::FxWorldReflectionsHint, "", &E::worldReflections},
        {"effects", "sprite_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSpriteLight,
         Text::FxSpriteLightHint, "", &E::spriteLight},
        {"effects", "sprite_light_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxSpriteLightStrength, Text::FxSpriteLightStrengthHint, " %", nullptr, &E::spriteLightStrength},
        {"effects", "seasons", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSeasons,
         Text::FxSeasonsHint, "", &E::seasons},
        {"effects", "rain_wet", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRainWet,
         Text::FxRainWetHint, "", &E::rainWet},
        {"effects", "night_windows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxNightWindows,
         Text::FxNightWindowsHint, "", &E::nightWindows},
        {"effects", "shared_wind", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSharedWind,
         Text::FxSharedWindHint, "", &E::sharedWind},
        {"effects", "lit_dust", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLitDust,
         Text::FxLitDustHint, "", &E::litDust},
        // 2.4: the parts of the effects, each a box under its effect (the water's foam, glints, ...)
        {"effects", "water_foam", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWaterFoam,
         Text::FxWaterFoamHint, "", &E::waterFoam},
        {"effects", "water_glints", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWaterGlints,
         Text::FxWaterGlintsHint, "", &E::waterGlints},
        {"effects", "caustics", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxCaustics,
         Text::FxCausticsHint, "", &E::caustics},
        {"effects", "waterfalls", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWaterfalls,
         Text::FxWaterfallsHint, "", &E::waterfalls},
        {"effects", "step_dust", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxStepDust,
         Text::FxStepDustHint, "", &E::stepDust},
        {"effects", "season_snow", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSeasonSnow,
         Text::FxSeasonSnowHint, "", &E::seasonSnow},
        {"effects", "season_grass", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSeasonGrass,
         Text::FxSeasonGrassHint, "", &E::seasonGrass},
        {"effects", "pollen", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxPollen,
         Text::FxPollenHint, "", &E::pollen},
        {"effects", "indoor_dust", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxIndoorDust,
         Text::FxIndoorDustHint, "", &E::indoorDust},
        {"effects", "rain_splashes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRainSplashes,
         Text::FxRainSplashesHint, "", &E::rainSplashes},
        {"effects", "desert_heat", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDesertHeat,
         Text::FxDesertHeatHint, "", &E::desertHeat},
        {"effects", "sky_rays", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkyRays,
         Text::FxSkyRaysHint, "", &E::skyRays},
        {"effects", "tree_seasons", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTreeSeasons,
         Text::FxTreeSeasonsHint, "", &E::treeSeasons},
        {"effects", "dragonflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDragonflies,
         Text::FxDragonfliesHint, "", &E::dragonflies},
        {"effects", "night_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxNightStrength,
         Text::FxNightStrengthHint, " %", nullptr, &E::nightStrength},
        {"effects", "water", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWater, Text::FxWaterHint,
         "", &E::water},
        {"effects", "water_waves", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxWaterWaves,
         Text::FxWaterWavesHint, " %", nullptr, &E::waterWaves},
        {"effects", "water_reflections", Page::Effects, Control::Slider, 0, 100, 10, {}, {},
         Text::FxWaterReflections, Text::FxWaterReflectionsHint, " %", nullptr, &E::waterReflections},
        {"effects", "water_clarity", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxWaterClarity,
         Text::FxWaterClarityHint, " %", nullptr, &E::waterClarity},
        {"ai", "enabled", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiEnable, Text::AiEnableHint, ""},
        {"ai", "ground", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiGround, Text::AiGroundHint, ""},
        {"ai", "objects", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiObjects, Text::AiObjectsHint,
         ""},
        {"ai", "monsters", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiSprites, Text::AiSpritesHint,
         ""},
        {"ai", "players", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiPlayers, Text::AiPlayersHint, ""},
        {"ai", "npcs", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiNpcs, Text::AiNpcsHint, ""},
        {"ai", "items", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiItems, Text::AiItemsHint, ""},
        {"ai", "dyes", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiDyes, Text::AiDyesHint, ""},
        {"ai", "effects", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiEffects, Text::AiEffectsHint,
         ""},
        {"ai", "icons", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiIcons, Text::AiIconsHint, ""},
        {"ai", "ui_pieces", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiUiPieces, Text::AiUiPiecesHint,
         ""},
        {"ai", "model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiModelWorld, Text::AiModelWorldHint, ""},
        {"ai", "sprite_model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiModelSprites, Text::AiModelSpritesHint, ""},
        // 1.6.3: small pictures by a model of their own
        {"ai", "small_pictures", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiSmall, Text::AiSmallHint,
         ""},
        {"ai", "small_side", Page::Ai, Control::Number, 16, 256, 1, {}, {}, Text::AiSmallSide, Text::AiSmallHint, ""},
        {"ai", "small_model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi}, Text::AiSmallModel,
         Text::AiSmallHint, ""},
        // 1.6: a model for each group of pictures; 2.5.28: 38 of them (core/ai/groups.hpp)
        {"ai", "model_ground_town", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundTown, Text::AiGroupModelHint, ""},
        {"ai", "model_ground_field", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundField, Text::AiGroupModelHint, ""},
        {"ai", "model_ground_dungeon", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundDungeon, Text::AiGroupModelHint, ""},
        {"ai", "model_ground_indoor", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundIndoor, Text::AiGroupModelHint, ""},
        {"ai", "model_ground_other", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundOther, Text::AiGroupModelHint, ""},
        {"ai", "model_buildings_town", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupBuildingsTown, Text::AiGroupModelHint, ""},
        {"ai", "model_models_field", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupModelsField, Text::AiGroupModelHint, ""},
        {"ai", "model_models_dungeon", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupModelsDungeon, Text::AiGroupModelHint, ""},
        {"ai", "model_furniture", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupFurniture, Text::AiGroupModelHint, ""},
        {"ai", "model_models_other", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupModelsOther, Text::AiGroupModelHint, ""},
        {"ai", "model_trees", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupTrees, Text::AiGroupModelHint, ""},
        {"ai", "model_plants", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupPlants, Text::AiGroupModelHint, ""},
        {"ai", "model_water", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupWater, Text::AiGroupModelHint, ""},
        {"ai", "model_bodies", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupBodies, Text::AiGroupModelHint, ""},
        {"ai", "model_mounted", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupMounted, Text::AiGroupModelHint, ""},
        {"ai", "model_heads", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupHeads, Text::AiGroupModelHint, ""},
        {"ai", "model_weapons", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupWeapons, Text::AiGroupModelHint, ""},
        {"ai", "model_shields", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupShields, Text::AiGroupModelHint, ""},
        {"ai", "model_headgear", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupHeadgear, Text::AiGroupModelHint, ""},
        {"ai", "model_garments", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGarments, Text::AiGroupModelHint, ""},
        {"ai", "model_monsters", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupMonsters, Text::AiGroupModelHint, ""},
        {"ai", "model_bosses", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupBosses, Text::AiGroupModelHint, ""},
        {"ai", "model_homunculi", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupHomunculi, Text::AiGroupModelHint, ""},
        {"ai", "model_npcs", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupNpcs, Text::AiGroupModelHint, ""},
        {"ai", "model_items_ground", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupItems, Text::AiGroupModelHint, ""},
        {"ai", "model_item_icons", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupItemIcons, Text::AiGroupModelHint, ""},
        {"ai", "model_item_pictures", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupItemPictures, Text::AiGroupModelHint, ""},
        {"ai", "model_card_pictures", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupCardPictures, Text::AiGroupModelHint, ""},
        {"ai", "model_skill_animations", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupSkillAnimations, Text::AiGroupModelHint, ""},
        {"ai", "model_effect_sprites", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupEffectSprites, Text::AiGroupModelHint, ""},
        {"ai", "model_emotions", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupEmotions, Text::AiGroupModelHint, ""},
        {"ai", "model_cursor", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupCursor, Text::AiGroupModelHint, ""},
        {"ai", "model_windows", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupWindows, Text::AiGroupModelHint, ""},
        {"ai", "model_menu_icons", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupMenuIcons, Text::AiGroupModelHint, ""},
        {"ai", "model_portraits", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupPortraits, Text::AiGroupModelHint, ""},
        {"ai", "model_maps", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupMaps, Text::AiGroupModelHint, ""},
        {"ai", "model_loading", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupLoading, Text::AiGroupModelHint, ""},
        {"ai", "model_login", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupLogin, Text::AiGroupModelHint, ""},
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
        {"frames", "reflex", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesReflex,
         Text::FramesReflexHint, ""},  // 2.5.26
        {"frames", "fps_counter", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesFpsCounter,
         Text::FramesFpsCounterHint, ""},
        {"frames", "fps_limit", Page::Frames, Control::Number, 0, 1000, 1, {}, {}, Text::FramesFpsLimit,
         Text::FramesFpsLimitHint, ""},
        {"frames", "save_power", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesSavePower,
         Text::FramesSavePowerHint, ""},
        {"frames", "battery_saver", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesBattery,
         Text::FramesBatteryHint, ""},  // 2.5.20
        {"windows", "others_light", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsOthersLight,
         Text::WindowsOthersLightHint, ""},  // 3.0.3-beta.3: several game windows
        {"frames", "battery_fps", Page::Frames, Control::Slider, 15, 60, 5, {}, {}, Text::FramesBatteryFps,
         Text::FramesBatteryFpsHint, ""},
        {"frames", "keep_fps", Page::Frames, Control::Number, 0, 240, 1, {}, {}, Text::FramesKeepFps,
         Text::FramesKeepFpsHint, ""},
        {"frames", "light_skills", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesLightSkills,
         Text::FramesLightSkillsHint, ""},  // 3.0.18-beta.186
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
        {"windows", "others_music", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsOthersMusic,
         Text::WindowsOthersMusicHint, ""},  // 3.0.3-beta.3: several game windows
        {"windows", "others_sounds", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsOthersSounds,
         Text::WindowsOthersSoundsHint, ""},  // 3.0.3-beta.9
        {"sound", "limiter", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundLimiter,
         Text::GfxSoundLimiterHint, ""},
        {"sound", "stereo", Page::Sound, Control::Slider, 0, 100, 5, {}, {}, Text::GfxSoundStereo,
         Text::GfxSoundStereoHint, "%"},  // 0.92
        {"sound", "stereo_flip", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundStereoFlip,
         Text::GfxSoundStereoFlipHint, ""},  // 3.0.11-beta.301: it was in the file alone
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
        // 3.0.4-beta.7: the maps' own sounds
        {"sound", "ambience", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundAmbience,
         Text::GfxSoundAmbienceHint, ""},
        {"sound", "ambience_volume", Page::Sound, Control::Slider, 0, 100, 5, {}, {}, Text::GfxSoundAmbienceVolume,
         Text::GfxSoundAmbienceVolumeHint, "%"},
        // 3.0.4-beta.1: the output device - the page's own list of Windows' devices (app/page_settings.cpp)
        {"sound", "output", Page::Sound, Control::Line, 0, 0, 0, {}, {}, Text::SoundOutput, Text::SoundOutputHint,
         ""},
        // 2.6: the Debug page - the log, the profiler and X-ray from the Diagnostics page, then the test switches
        // the ini alone held before
        {"connection", "patient_entry", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ConnPatientEntry,
         Text::ConnPatientEntryHint, ""},  // 2.6.17
        {"connection", "entry_pause", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ConnEntryPause,
         Text::ConnEntryPauseHint, ""},
        {"debug", "log", Page::Debug, Control::Check, 0, 1, 1, {"normal", "verbose"}, {}, Text::DiagVerboseLog,
         Text::DiagVerboseLogHint, ""},
        {"debug", "profile", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DiagProfile,
         Text::DiagProfileHint, ""},
        {"debug", "xray", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DiagXray,
         Text::DiagXrayHint, ""},  // 2.3
        {"debug", "seams_view", Page::Debug, Control::Choice, 0, 0, 0, {"0", "1", "2"},  // 2.7.8
         {Text::DebugSeamsOff, Text::DebugSeamsColour, Text::DebugSeamsOnTop}, Text::DebugSeams,
         Text::DebugSeamsHint, ""},
        {"debug", "weak_card", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugWeakCard,
         Text::DebugWeakCardHint, ""},  // 3.0.4-beta.35
        {"quality", "dlss_jitter", Page::Debug, Control::Check, 0, 1, 1, {"normal", "inverted"}, {},
         Text::DebugDlssJitter, Text::DebugDlssJitterHint, ""},
        {"quality", "dlss_motion", Page::Debug, Control::Check, 0, 1, 1, {"normal", "inverted"}, {},
         Text::DebugDlssMotion, Text::DebugDlssMotionHint, ""},
        {"renderer", "bridge_present", Page::Debug, Control::Choice, 0, 0, 0, {"auto", "window", "composition"},
         {Text::DebugPresentAuto, Text::DebugPresentWindow, Text::DebugPresentComposition}, Text::DebugPresent,
         Text::DebugPresentHint, ""},
        {"renderer", "bridge_dpi_scale", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugDpiScale,
         Text::DebugDpiScaleHint, ""},
        {"debug", "shader_variants", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugVariants,
         Text::DebugVariantsHint, ""},
        {"debug", "record_scene", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugRecordScene,
         Text::DebugRecordSceneHint, ""},  // 3.0.17-beta.7
        {"debug", "state_cache", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugStateCache,
         Text::DebugStateCacheHint, ""},
        {"debug", "merge_draws", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugMergeDraws,
         Text::DebugMergeDrawsHint, ""},
        {"debug", "fast_pick", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugFastPick,
         Text::DebugFastPickHint, ""},
        {"debug", "fast_ftol", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugFastFtol,
         Text::DebugFastFtolHint, ""},
        {"debug", "fast_textures", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugFastTextures,
         Text::DebugFastTexturesHint, ""},
        {"debug", "quick_acks", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugQuickAcks,
         Text::DebugQuickAcksHint, ""},  // 3.0.11-beta.128
    };
    return list;
}

const IniSetting* findSetting(const std::string& sectionKey) {
    // 2.2.2: by an index made once - parseConfig asks for each [effects] line now
    static const std::unordered_map<std::string, const IniSetting*> index = [] {
        std::unordered_map<std::string, const IniSetting*> m;
        for (const IniSetting& s : iniSettings()) m.emplace(std::string(s.section) + "." + s.key, &s);
        return m;
    }();
    const auto it = index.find(sectionKey);
    return it == index.end() ? nullptr : it->second;
}

std::string sectionKey(const IniSetting& s) { return std::string(s.section) + "." + s.key; }

std::string tokenOf(const IniSetting& s, const Config& c) {
    if (s.flag) return c.effects.*s.flag ? s.tokens[1] : s.tokens[0];  // 2.2.2: [effects] by the entry's field
    if (s.number) return std::to_string(c.effects.*s.number);
    const std::string k = sectionKey(s);
    if (k == "renderer.api") return apiToken(c.api);
    if (k == "renderer.gpu") return c.gpu.empty() ? "auto" : c.gpu;
    if (k == "report.auto") return c.reportAuto ? "1" : "0";
    if (k == "pad.enabled") return c.padEnabled ? "1" : "0";  // 2.9
    if (k == "launcher.direct_start") return c.directStart ? "1" : "0";  // 2.11
    if (k == "client.cps") return c.clientCps ? "1" : "0";  // 3.0.1-beta.5
    if (k == "client.bink") return c.clientBink ? "1" : "0";
    if (k == "client.ijl") return c.clientIjl ? "1" : "0";  // 3.0.1-beta.6
    if (k == "client.mss") return c.clientMss ? "1" : "0";  // 3.0.1-beta.7
    if (k == "client.roext") return c.clientRoext ? "1" : "0";  // 3.0.1-beta.8
    if (k == "client.updates") return c.clientUpdates ? "1" : "0";  // 3.0.2-beta.5
    if (k == "roext.window_lock") return c.roext.windowLock ? "1" : "0";
    if (k == "roext.window_on_top") return c.roext.windowOnTop ? "1" : "0";
    if (k == "roext.mouse_buttons") return c.roext.mouseButtons ? "1" : "0";
    if (k == "roext.alt_f4") return c.roext.altF4 ? "1" : "0";
    if (k == "roext.minimize_key") return c.roext.minimizeKey ? "1" : "0";
    if (k == "roext.free_cpu") return std::to_string(c.roext.freeCpu);
    if (k == "roext.code_page") return std::to_string(c.roext.codePage);
    if (k == "roext.window_x") return std::to_string(c.roext.windowX);
    if (k == "roext.window_y") return std::to_string(c.roext.windowY);
    if (k == "windows.title_name") return c.windowTitleName ? "1" : "0";  // 3.0.3-beta.2
    if (k == "windows.remember_place") return c.windowRememberPlace ? "1" : "0";
    if (k == "windows.others_light") return c.windowOthersLight ? "1" : "0";  // 3.0.3-beta.3
    if (k == "windows.others_music") return c.windowOthersMusic ? "1" : "0";
    if (k == "windows.others_sounds") return c.windowOthersSounds ? "1" : "0";  // 3.0.3-beta.9
    if (k == "windows.switch_keys") return windowKeysToken(c.windowKeys);  // 3.0.3-beta.6
    if (k == "panel.show") return c.panelShow ? "1" : "0";  // 3.0.3-beta.8
    if (k == "interface.minimap") return c.newMinimap ? "1" : "0";  // 3.0.11-beta.1
    if (k == "interface.minimap_npcs") return c.minimapNpcs ? "1" : "0";  // 3.0.11-beta.6
    if (k == "interface.minimap_warps") return c.minimapWarps ? "1" : "0";
    if (k == "interface.minimap_mobs") return c.minimapMobs ? "1" : "0";
    if (k == "interface.net_badge") return c.netBadge ? "1" : "0";  // 3.0.11-beta.122
    if (k == "interface.smooth_fixes") return c.smoothFixes ? "1" : "0";  // 3.0.11-beta.123
    if (k == "interface.predict_walk") return c.predictWalk ? "1" : "0";  // 3.0.11-beta.130
    if (k == "interface.minimap_style") return mapStyleToken(c.minimapStyle);  // 3.0.11-beta.120
    if (k == "interface.minimap_kafra") return c.minimapKafra ? "1" : "0";  // 3.0.11-beta.119
    if (k == "interface.minimap_view") return c.minimapView ? "1" : "0";    // 3.0.11-beta.187
    if (k == "interface.minimap_classic") return c.minimapClassic ? "1" : "0";  // 3.0.11-beta.222
    if (k == "interface.exp_bars") return c.expBars ? "1" : "0";                // 3.0.11-beta.236
    if (k == "interface.exp_gain") return c.expGain ? "1" : "0";                // 3.0.11-beta.237
    if (k == "interface.chat_fade") return std::to_string(c.chatFade);          // 3.0.11-beta.240
    if (k == "interface.long_chat") return c.longChat ? "1" : "0";              // 3.0.13-beta.12
    if (k == "interface.chat_log") return c.chatLog ? "1" : "0";                // 3.0.13-beta.3
    if (k == "interface.skill_jokes") return c.skillJokes ? "1" : "0";          // 3.0.13-beta.8
    if (k == "interface.swear_filter") return c.swearFilter ? "1" : "0";        // 3.0.13-beta.9
    if (k == "interface.chat_party") return c.chatParty;                        // 3.0.13-beta.3
    if (k == "interface.chat_guild") return c.chatGuild;
    if (k == "interface.chat_own") return c.chatOwn;
    if (k == "interface.chat_others") return c.chatOthers;
    if (k == "interface.minimap_party") return c.minimapParty ? "1" : "0";
    if (k == "interface.minimap_reach") return c.minimapReach ? "1" : "0";  // 3.0.11-beta.65
    if (k == "interface.minimap_hd") return c.minimapHd ? "1" : "0";        // 3.0.11-beta.72
    if (k == "interface.pack_icons") return c.packIcons ? "1" : "0";        // 3.0.11-beta.74
    if (k == "interface.hotbar_per_character") return c.hotbarPerCharacter ? "1" : "0";  // 3.0.11-beta.78
    if (k == "interface.hotbar_ammo") return c.hotbarAmmo ? "1" : "0";                    // 3.0.11-beta.125
    if (k == "interface.element_colours") return c.elementColours ? "1" : "0";  // 3.0.18-beta.144
    if (k == "interface.ammo_sound") return c.ammoSound ? "1" : "0";                      // 3.0.11-beta.129
    if (k == "interface.pad_hints") return c.padHints ? "1" : "0";                         // 3.0.11-beta.86
    if (k == "interface.target_ring") return targetRingToken(c.targetRing);                // 3.0.11-beta.87
    if (k == "interface.mob_hp") return mobHpToken(c.mobHp);                               // 3.0.11-beta.95
    if (k == "interface.mob_hp_percent") return c.mobHpPercent ? "1" : "0";                // 3.0.11-beta.293
    if (k == "interface.screen_shake") return c.screenShake ? "1" : "0";                   // 3.0.13-beta.8
    if (k == "interface.follow") return c.follow ? "1" : "0";                              // 3.0.13-beta.11
    if (k == "interface.esc_cutin") return c.escCutin ? "1" : "0";                         // 3.0.13-beta.10
    if (k == "interface.quick_teleport") return c.quickTeleport ? "1" : "0";               // 3.0.13-beta.7
    if (k == "interface.who_hits") return c.whoHits ? "1" : "0";                           // 3.0.13-beta.13
    if (k == "interface.boss_bars") return c.bossBars ? "1" : "0";                         // 3.0.13-beta.3
    if (k == "interface.pad_walk") return padWalkToken(c.padWalk);                         // 3.0.11-beta.88
    if (k == "interface.walk_arrow") return walkLookToken(c.walkArrow);                    // 3.0.11-beta.103
    if (k.rfind("interface.key_", 0) == 0) {  // 3.0.18-beta.125: our windows' Ctrl letters
        const std::string w = k.substr(14);
        const closedwin::Own o = w == "hunt" ? closedwin::Own::Hunt : w == "mvp" ? closedwin::Own::Mvp
                                 : w == "tracker" ? closedwin::Own::Tracker : w == "party" ? closedwin::Own::Party
                                                                                : closedwin::Own::Recap;
        const char ch = c.ownKeys[size_t(o)];
        return ch ? std::string(1, char(std::tolower(ch))) : std::string("off");
    }
    if (k == "interface.cursor") return cursorLookToken(c.cursorLook);                     // 3.0.11-beta.198
    if (k == "interface.walk_mark") return c.walkMark ? "1" : "0";                         // 3.0.11-beta.105
    if (k == "interface.arrows_walk") return c.arrowsWalk ? "1" : "0";  // 3.0.18-beta.207
    if (k == "interface.walk_mode") return c.walkMode ? "1" : "0";      // 3.0.18-beta.208
    if (k == "interface.space_action") return c.spaceAction ? "1" : "0";  // 3.0.18-beta.209
    if (k == "interface.pad_tiles") return c.padTiles ? "1" : "0";                         // 3.0.11-beta.90
    if (k == "interface.minimap_way") return wayOnGroundToken(c.minimapWay);  // 3.0.11-beta.33
    if (k == "interface.buffs") return buffsLookToken(c.buffs);               // 3.0.11-beta.35
    if (k == "interface.buffs_overflow") return buffsOverflowToken(c.buffsOverflow);  // 3.0.11-beta.63
    if (k == "interface.character") return characterLookToken(c.character);  // 3.0.11-beta.36
    if (k == "interface.target") return c.targetCard ? "1" : "0";              // 3.0.11-beta.38
    if (k == "interface.party") return c.partyPanel ? "1" : "0";               // 3.0.11-beta.39
    if (k == "interface.tracker") return c.tracker ? "1" : "0";                // 3.0.11-beta.70
    if (k == "interface.hotbar") return c.hotbar ? "1" : "0";                  // 3.0.11-beta.40
    if (k == "interface.chat") return c.chat ? "1" : "0";                      // 3.0.11-beta.41
    if (k == "interface.items") return c.itemsWindow ? "1" : "0";              // 3.0.11-beta.42
    if (k == "interface.item_toast") return c.itemToast ? "1" : "0";           // 3.0.11-beta.186
    if (k == "interface.status") return c.statusWindow ? "1" : "0";
    if (k == "interface.skills") return c.skillsWindow ? "1" : "0";  // 3.0.11-beta.96
    if (k == "interface.equip") return c.equipWindow ? "1" : "0";    // 3.0.11-beta.97
    if (k == "interface.npc_talk") return c.npcWindow ? "1" : "0";   // 3.0.11-beta.98
    if (k == "interface.npc_remember") return c.npcRemember ? "1" : "0";  // 3.0.11-beta.255
    if (k == "interface.portraits") return c.portraits ? "1" : "0";  // 3.0.11-beta.273
    if (k == "interface.char_carousel") return c.charCarousel ? "1" : "0";  // 3.0.11-beta.275
    if (k == "interface.delay_ring") return c.delayRing ? "1" : "0";  // 3.0.11-beta.276
    if (k == "interface.death_recap") return c.deathRecap ? "1" : "0";  // 3.0.11-beta.277
    if (k == "interface.hunt_panel") return c.huntPanel ? "1" : "0";  // 3.0.11-beta.278
    if (k == "interface.mob_card") return c.mobCard ? "1" : "0";  // 3.0.11-beta.279
    if (k == "interface.mvp_timers") return c.mvpTimers ? "1" : "0";  // 3.0.11-beta.280
    if (k == "interface.potion_warn") return c.potionWarn ? "1" : "0";  // 3.0.11-beta.281
    if (k == "interface.party_alarm") return c.partyAlarm ? "1" : "0";  // 3.0.11-beta.282
    if (k == "interface.drop_names") return c.dropNames ? "1" : "0";  // 3.0.11-beta.283
    if (k == "interface.drop_highlight") return c.dropHighlight ? "1" : "0";  // 3.0.11-beta.283
    if (k == "interface.layout_per_char") return c.layoutPerChar ? "1" : "0";  // 3.0.11-beta.288
    if (k == "interface.homun_ai") return c.homunAi ? "1" : "0";  // 3.0.11-beta.290
    if (k == "interface.homun_aggressive") return c.homunAggressive ? "1" : "0";
    if (k == "interface.homun_defend") return c.homunDefend ? "1" : "0";
    if (k == "interface.homun_skills") return c.homunSkills ? "1" : "0";
    if (k == "interface.shop") return c.shopWindow ? "1" : "0";      // 3.0.11-beta.100
    if (k == "interface.storage") return c.storageWindow ? "1" : "0";  // 3.0.11-beta.115
    if (k == "interface.trade") return c.tradeWindow ? "1" : "0";      // 3.0.11-beta.116
    if (k == "interface.menu") return c.menuWindow ? "1" : "0";        // 3.0.11-beta.140
    if (k == "interface.cards") return c.cardWindows ? "1" : "0";      // 3.0.11-beta.149
    if (k == "interface.asks") return c.askWindows ? "1" : "0";        // 3.0.11-beta.150
    if (k == "interface.messages") return c.messageBoxes ? "1" : "0";  // 3.0.11-beta.151
    if (k == "interface.login") return c.loginWindow ? "1" : "0";
    if (k == "interface.login_status") return c.loginStatus ? "1" : "0";  // 3.0.11-beta.173
    if (k == "interface.login_news") return c.loginNews ? "1" : "0";      // 3.0.11-beta.174
    if (k == "interface.login_view") return loginview::token(c.loginView);  // 3.0.11-beta.177
    if (k == "interface.char_select") return c.charSelectWindow ? "1" : "0";  // 3.0.11-beta.152
    if (k == "interface.char_make") return c.charMakeWindow ? "1" : "0";      // 3.0.11-beta.153
    if (k == "interface.cart") return c.cartWindow ? "1" : "0";               // 3.0.11-beta.154
    if (k == "interface.counts") return c.countWindow ? "1" : "0";            // 3.0.11-beta.155
    if (k == "interface.vending") return c.vendWindow ? "1" : "0";            // 3.0.11-beta.156
    if (k == "interface.picks") return c.pickWindows ? "1" : "0";             // 3.0.11-beta.157
    if (k == "interface.party_setup") return c.partySetupWindow ? "1" : "0";  // 3.0.11-beta.158
    if (k == "interface.pet") return c.petWindow ? "1" : "0";                 // 3.0.11-beta.159
    if (k == "interface.homun") return c.homunWindow ? "1" : "0";             // 3.0.11-beta.160
    if (k == "interface.merc") return c.mercWindow ? "1" : "0";               // 3.0.11-beta.161
    if (k == "interface.options") return c.optionsWindow ? "1" : "0";         // 3.0.11-beta.162
    if (k == "interface.social") return c.socialWindow ? "1" : "0";           // 3.0.11-beta.163
    if (k == "interface.servers") return c.serverWindow ? "1" : "0";          // 3.0.11-beta.164
    if (k == "interface.guild") return c.guildWindow ? "1" : "0";             // 3.0.11-beta.165
    if (k == "interface.my_shop") return c.myShopWindow ? "1" : "0";          // 3.0.11-beta.166
    if (k == "interface.vend_setup") return c.vendSetupWindow ? "1" : "0";    // 3.0.16-beta.33
    if (k == "interface.whisper_setup") return c.whisperSetupWindow ? "1" : "0";  // 3.0.11-beta.168
    if (k == "interface.macros") return c.macroWindow ? "1" : "0";            // 3.0.11-beta.169
    if (k == "interface.tips") return c.tipWindow ? "1" : "0";                // 3.0.11-beta.204
    if (k == "interface.loading_bar") return c.loadingBar ? "1" : "0";        // 3.0.18-beta.180
    if (k == "interface.emotes") return c.emoteWindow ? "1" : "0";            // 3.0.11-beta.205
    if (k == "interface.commands") return c.commandWindow ? "1" : "0";        // 3.0.11-beta.206
    if (k == "interface.chat_rooms") return c.chatRoomWindows ? "1" : "0";    // 3.0.11-beta.207
    if (k == "interface.detail_console") return c.detailConsole ? "1" : "0";  // 3.0.11-beta.209
    if (k == "interface.popups") return c.popupMenus ? "1" : "0";             // 3.0.11-beta.211
    if (k == "interface.whispers") return c.whisperWindows ? "1" : "0";       // 3.0.11-beta.212
    if (k == "interface.skill_cells") return c.skillCells ? "1" : "0";  // 3.0.11-beta.142
    if (k == "interface.mouse_cell") return c.mouseCell ? "1" : "0";    // 3.0.16-beta.7
    if (k == "interface.hotbar_supplies") return c.hotbarSupplies ? "1" : "0";  // 3.0.16-beta.8
    if (k == "interface.skill_needs") return c.skillNeeds ? "1" : "0";  // 3.0.16-beta.13
    if (k == "interface.combo_window") return c.comboWindow ? "1" : "0";
    if (k == "interface.broken_gear") return c.brokenGear ? "1" : "0";
    if (k == "interface.pet_hunger") return c.petHunger ? "1" : "0";
    if (k == "interface.ground_timers") return c.groundTimers ? "1" : "0";  // 3.0.16-beta.21
    if (k == "interface.signs") return c.worldSigns ? "1" : "0";        // 3.0.16-beta.7
    if (k == "interface.ground_counts") return c.groundCounts ? "1" : "0";  // 3.0.11-beta.143
    if (k == "interface.animations") return c.animations ? "1" : "0";  // 3.0.11-beta.46
    if (k == "interface.uaui_motion") return c.uauiMotion ? "1" : "0";  // 3.0.18-beta.58
    if (k == "interface.uaui_effects") return c.uauiEffects ? "1" : "0";  // 3.0.18-beta.61
    if (k == "interface.uaui_glass") return c.uauiGlass ? "1" : "0";  // 3.0.18-beta.64
    if (k == "interface.uaui_showcase") return c.uauiShowcase ? "1" : "0";  // 3.0.18-beta.11
    if (k == "interface.theme") return c.uiTheme;  // 3.0.18-beta.13
    if (k == "interface.panel_opacity") return std::to_string(c.hudOpacity);  // 3.0.11-beta.47
    if (k == "interface.cast") return c.castLabel ? "1" : "0";                 // 3.0.11-beta.48
    if (k == "interface.low_hp_edge") return std::to_string(c.lowHpEdge);      // 3.0.11-beta.50
    if (k == "interface.size") return c.uiSize > 0 ? std::to_string(c.uiSize) : "auto";  // 3.0.11-beta.54
    if (k == "interface.text_size") return std::to_string(c.textSize);  // 3.0.11-beta.67
    if (k == "interface.item_windows") return std::to_string(c.itemWindows);  // 3.0.15-beta.9
    if (k == "interface.window_opacity") return std::to_string(c.windowOpacity);
    if (k == "interface.reference") return c.referenceWindow ? "1" : "0";  // 3.0.11-beta.7
    if (k == "windows.start_count") return std::to_string(c.windowStartCount);  // 3.0.4-beta.4
    if (k == "camera.indoor") return c.cameraIndoor ? "1" : "0";                // 3.0.4-beta.5
    if (k == "camera.zoom_limit") return c.cameraZoomLimit ? "1" : "0";         // 3.0.4-beta.17
    if (k == "camera.remember") return c.cameraRemember ? "1" : "0";            // 3.0.11-beta.184
    if (k == "camera.tilt") return c.cameraTilt;                                // 3.0.13-beta.5
    if (k == "buffs.timers") return c.buffTimers ? "1" : "0";                   // 3.0.4-beta.6
    if (k.compare(0, 7, "notify.") == 0) {                   // 3.0.4-beta.3
        const NotifySettings& n = c.notify;
        const std::string w = k.substr(7);
        if (w == "buff_seconds") return std::to_string(n.buffSeconds);  // 3.0.7-beta.6
        if (w == "party_hp") return std::to_string(n.partyHp);
        if (w == "recovered_percent") return std::to_string(n.recoveredPercent);  // 3.0.7-beta.7
        for (size_t i = 0; i < kNotifyBoxCount; ++i)
            if (w == kNotifyBoxes[i].key) return n.*kNotifyBoxes[i].field ? "1" : "0";
        return "0";
    }
    if (k == "panel.layout") return panelLayoutToken(c.panelLayout);
    if (k == "panel.size") return std::to_string(c.panelSize);
    if (k == "panel.opacity") return std::to_string(c.panelOpacity);
    if (k == "renderer.safe_mode") return c.safeMode ? "1" : "0";  // 2.12
    if (k == "stats.send") return c.statsSend ? "1" : "0";
    if (k == "renderer.bridge") return c.bridge ? "1" : "0";
    if (k == "renderer.render_thread") return c.renderThread ? "1" : "0";
    if (k == "renderer.remix") return c.remix ? "1" : "0";
    if (k == "display.high_resolution") return c.highResolution ? "1" : "0";
    if (k == "display.dpi_aware") return c.dpiAware ? "1" : "0";
    if (k == "quality.render_scale") return std::to_string(c.renderScale);
    if (k == "quality.world_scale") return std::to_string(c.worldScale);  // 2.5.16
    if (k == "quality.full_colour") return c.fullColour ? "1" : "0";        // 2.5.18
    if (k == "quality.super_light") return c.superLight ? "1" : "0";        // 2.5.20
    if (k == "frames.battery_saver") return c.batterySaver ? "1" : "0";
    if (k == "frames.battery_fps") return std::to_string(c.batteryFps);
    if (k == "quality.sharpen") return std::to_string(c.sharpen);
    if (k == "display.ui_filter") return uiFilterToken(c.uiFilter);
    if (k == "display.window_art") return windowArtToken(c.windowArt);
    if (k == "display.crisp_text") return c.crispText ? "1" : "0";
    if (k == "display.crisp_numbers") return c.crispNumbers ? "1" : "0";  // 3.0.18-beta.178
    if (k == "quality.dlss") return c.dlss;
    if (k == "quality.dlss_scale") return std::to_string(c.dlssScale);  // 2.4.2
    if (k == "quality.dlss_width") return std::to_string(c.dlssWidth);
    if (k == "quality.dlss_height") return std::to_string(c.dlssHeight);
    if (k == "quality.dlss_model") return c.dlssModel;
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
    if (k == "ai.dyes") return a.dyes ? "1" : "0";  // 2.5.23
    if (k == "ai.compress") return a.compress;
    if (k == "ai.generate") return a.generate ? "1" : "0";
    if (k == "quality.anisotropy") return std::to_string(c.anisotropy);
    if (k == "effects.preset") return effect_groups::presetToken(c.effects.preset);  // 2.2
    for (int g = 0; g < kEffectGroups; ++g) {
        const std::string word = std::string("effects.group_") + effect_groups::groupWord(g);
        if (k == word) return c.effects.groups[g] ? "1" : "0";
        if (k == word + "_strength") return std::to_string(c.effects.groupStrength[g]);
    }
    if (k == "effects.tier") return c.effects.tier == 1 ? "classic" : c.effects.tier == 2 ? "remaster" : "auto";
    if (k == "effects.clock")  // 2.6.11
        return s.tokens[c.effects.clock > 0 && c.effects.clock < int(s.tokens.size()) ? size_t(c.effects.clock) : 0];
    if (k == "effects.skill_looks")  // 3.0.18-beta.183
        return s.tokens[c.effects.skillLooks >= 0 && c.effects.skillLooks < int(s.tokens.size())
                            ? size_t(c.effects.skillLooks)
                            : size_t(kSkillSoft)];
    if (k == "effects.season")  // 2.6.22
        return s.tokens[c.effects.season > 0 && c.effects.season < int(s.tokens.size()) ? size_t(c.effects.season) : 0];
    if (k == "effects.weather_kind")  // 2.6.22
        return s.tokens[c.effects.weatherKind > 0 && c.effects.weatherKind < int(s.tokens.size())
                            ? size_t(c.effects.weatherKind)
                            : 0];
    if (k == "frames.vsync") return c.vsync ? "1" : "0";
    if (k == "frames.fps_limit") return std::to_string(c.fpsLimit);
    if (k == "frames.keep_fps") return std::to_string(c.keepFps);
    if (k == "frames.light_skills") return c.lightSkills ? "1" : "0";  // 3.0.18-beta.186
    if (k == "debug.profile") return c.profile ? "1" : "0";
    if (k == "debug.xray") return c.xray ? "1" : "0";  // 2.3
    if (k == "debug.seams_view") return std::to_string(c.seamsView >= 0 && c.seamsView <= 2 ? c.seamsView : 0);
    if (k == "debug.weak_card") return c.weakCard ? "1" : "0";  // 3.0.4-beta.35
    if (k == "frames.save_power") return c.savePower ? "1" : "0";
    if (k == "frames.fps_counter") return c.fpsCounter ? "1" : "0";
    if (k == "connection.patient_entry") return c.connection.patientEntry ? "1" : "0";  // 2.6.17
    if (k == "connection.entry_pause") return c.connection.entryPause ? "1" : "0";
    if (k == "display.ui_native") return c.uiNative ? "1" : "0";
    if (k == "display.hdr") return c.hdr ? "1" : "0";
    if (k == "frames.frame_generation") return c.frameGeneration ? std::to_string(c.frameGeneration) + "x" : "off";
    if (k == "frames.allow_tearing") return c.allowTearing ? "1" : "0";
    if (k == "frames.reflex") return c.reflex ? "1" : "0";  // 2.5.26
    if (k == "sound.rate") return std::to_string(c.sound.rate);  // 0.91
    if (k == "sound.voices") return c.sound.voices ? "1" : "0";
    if (k == "sound.background") return std::to_string(c.sound.background);
    if (k == "sound.limiter") return c.sound.limiter ? "1" : "0";
    if (k == "sound.stereo") return std::to_string(c.sound.stereo);  // 0.92
    if (k == "sound.stereo_flip") return c.sound.stereoFlip ? "1" : "0";  // 3.0.11-beta.301
    if (k == "sound.steal") return c.sound.steal ? "1" : "0";
    if (k == "sound.crossfade") return c.sound.crossfade ? "1" : "0";
    if (k == "sound.engine") return c.sound.engine ? "1" : "0";  // 1.1
    if (k == "sound.reverb") return std::to_string(c.sound.reverb);
    if (k == "sound.headphones") return c.sound.headphones ? "1" : "0";  // 1.2
    if (k == "sound.duck") return c.sound.duck ? "1" : "0";              // 1.3
    if (k == "sound.output") return c.sound.output.empty() ? "auto" : c.sound.output;  // 3.0.4-beta.1
    if (k == "sound.ambience") return c.sound.ambience ? "1" : "0";                    // 3.0.4-beta.7
    if (k == "sound.ambience_volume") return std::to_string(c.sound.ambienceVolume);
    if (k == "debug.log") return c.logLevel == LogLevel::Debug ? "verbose" : "normal";
    // 2.6: the Debug page's test switches
    if (k == "quality.dlss_jitter") return c.dlssJitterInverted ? "inverted" : "normal";
    if (k == "quality.dlss_motion") return c.dlssMotionInverted ? "inverted" : "normal";
    if (k == "renderer.bridge_present") return c.bridgePresent;
    if (k == "renderer.bridge_dpi_scale") return c.bridgeDpiScale ? "1" : "0";
    if (k == "debug.shader_variants") return c.shaderVariants ? "1" : "0";
    if (k == "debug.record_scene") return c.recordScene ? "1" : "0";
    if (k == "debug.state_cache") return c.stateCache ? "1" : "0";
    if (k == "debug.merge_draws") return c.mergeDraws ? "1" : "0";
    if (k == "debug.fast_pick") return c.fastPick ? "1" : "0";
    if (k == "debug.fast_ftol") return c.fastFtol ? "1" : "0";
    if (k == "debug.fast_textures") return c.fastTextures ? "1" : "0";
    if (k == "debug.quick_acks") return c.quickAcks ? "1" : "0";  // 3.0.11-beta.128
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
