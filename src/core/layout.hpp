#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace uagfx::layout {

// 2.8: every file of uaRO Graphics in the game's folder lives in uaro-graphics\ - the root keeps ddraw.dll,
// uaro-graphics.exe and the gamepad's dinput.dll. Paths here are relative to the game's folder, with backslashes;
// a 2.x install moves into place by core/layout_migration.
enum class File : uint8_t {
    Ini, DriverLog, Render64Log, PackLog, StudioEngineLog, Report, Pipelines, Shaders32, Shaders64, ShadersD3d11,
    StudioShaders64, Pack, Backups, DgVoodoo, PadIni, PadBinds, PadLog
};
constexpr int kFileCount = 17;

constexpr const char* kOurs = "uaro-graphics";
constexpr const char* kLogs = "uaro-graphics\\logs";
constexpr const char* kCache = "uaro-graphics\\cache";
constexpr const char* kTextures = "uaro-graphics\\textures";
// The folders the driver, the 64-bit host and the settings app make at their start, in this order: what writes into
// them - the shader caches, the pipelines, a new texture pack - does not make them itself.
constexpr const char* kMadeAtStart[] = {kOurs, kLogs, kCache, kTextures};
// 2.10: the installer's places (app/installer): the files an update replaced, kept for a rollback; its downloads and
// the package unpacked; the site's update.ini as last read (and the player's [local] choices).
constexpr const char* kPrevious = "uaro-graphics\\previous";
constexpr const char* kUpdateWork = "uaro-graphics\\update";
constexpr const char* kUpdateInfo = "uaro-graphics\\update.ini";
// 2.11: the game's desktop shortcuts the installer's «uaRO» replaced, kept for the removal (and its once-marker)
constexpr const char* kShortcuts = "uaro-graphics\\shortcuts";
// 3.0.1-beta.5: our own libraries in place of the client's old ones (core/client_libs.hpp) - the package's copies,
// put into the game's root; the game's own kept aside for their switches and the removal
constexpr const char* kClientLibs = "uaro-graphics\\clientlibs";
constexpr const char* kOriginals = "uaro-graphics\\original";
// 2.11: when the driver last sent a direct start to «Гра» (its time is the file's): no bouncing (core/launcher)
constexpr const char* kRedirected = "uaro-graphics\\cache\\launcher-redirect.txt";
// 2.12: the safe mode's (core/safe_mode): the failed starts in a row; why the game starts safe (the app shows it and
// removes it - «Звичайний режим»)
constexpr const char* kCardLevel = "uaro-graphics\\card-level.txt";  // 3.0.4-beta.35: the card judge's verdict
// 3.0.18-beta.230: the wings' garment sets from the uaRO HD client (tools/wings_pack.py, core/wings)
constexpr const char* kWings = "uaro-graphics\\wings.grf";
// 3.0.11-beta.61 (the autopilot): the first game's outcome (core/first_game) and the mark that the site took it
constexpr const char* kFirstGame = "uaro-graphics\\cache\\first-game.txt";
// 3.0.11-beta.303: the settings app's rows the player has had - those not in it are marked «нове» (core/settings
// navigation newRows)
constexpr const char* kKnownRows = "uaro-graphics\\cache\\known-rows.txt";
constexpr const char* kFirstGameSent = "uaro-graphics\\cache\\first-game.sent";
// 3.0.17-beta.1: the installed version's failed starts (core/start_failures) - the rollback offered after two
constexpr const char* kStartFailures = "uaro-graphics\\cache\\start-failures.txt";
// 3.0.17-beta.12: the rows Alt+D's «Нове» has shown (core/settings/navigation knownRowsText)
constexpr const char* kConsoleKnown = "uaro-graphics\\cache\\console-known.txt";
// 3.0.17-beta.11: the game's chat told «Alt+D» once (win/console_hint)
constexpr const char* kConsoleHint = "uaro-graphics\\cache\\console-hint.txt";
// 3.0.17-beta.10: the version the player saw «Гра» with - none: the first start's cards, another: «Оновлено до»
constexpr const char* kSeenVersion = "uaro-graphics\\cache\\seen-version.txt";
// 3.0.17-beta.7: [debug] record_scene's file (core/scene_record)
constexpr const char* kSceneRecord = "uaro-graphics\\scene-record.bin";
// 3.0.17-beta.4: the console's rows pinned to «Швидко» (core/console_pins)
constexpr const char* kConsolePins = "uaro-graphics\\cache\\console-pins.txt";
// 3.0.17-beta.18: uaRO HD's screen (core/settings/game_screen) - the driver gives it to the game, the shared registry
// key keeps the old client's
constexpr const char* kGameScreen = "uaro-graphics\\game-screen.txt";
// 3.0.18-beta.9: uaUI's theme (win/uaui) - when there, read in place of the built-in one and again when it changes
constexpr const char* kUiTheme = "uaro-graphics\\ui\\theme.rcss";
// 3.0.18-beta.13: uaUI's parts' colours over the theme chosen (core/ui_theme), read again when it changes
constexpr const char* kUiPalette = "uaro-graphics\\ui\\palette.txt";
constexpr const char* kFailedStarts = "uaro-graphics\\cache\\failed-starts.txt";
constexpr const char* kSafeModeNote = "uaro-graphics\\cache\\safe-mode.txt";
// 3.0.11-beta.56 (the owner: «сделать» - how many installs there are): this install's random number for the site's
// anonymous count (core/install_ping) and when it was last told
constexpr const char* kInstallId = "uaro-graphics\\install-id.txt";
constexpr const char* kPingSent = "uaro-graphics\\cache\\ping-sent.txt";
// 3.0.3-beta.2: where each character's game window was put (core/window_places), written by the games
constexpr const char* kWindowPlaces = "uaro-graphics\\uaro-windows.txt";
// 3.0.11-beta.184 (R-ANMTEJ): the camera's zoom and tilt as the player left them (core/camera_memory), by the games
constexpr const char* kCameraMemory = "uaro-graphics\\camera.txt";
// 3.0.13-beta.3: [interface] chat_log - the chat's lines, a file a day (YYYY-MM-DD.txt); never sent with a report
constexpr const char* kChatLogs = "uaro-graphics\\chat-logs";
// 3.0.15-beta.1: the skills' effects as the game draws them, in betas (core/effect_capture.hpp), sent with a report
constexpr const char* kEffectCapture = "uaro-graphics\\logs\\uaro-effects.txt";
// 2.13: «Звернення» (app/reports): the computer's random key - the site's reports belong to it, no login - and the
// reports the site did not take yet (queued-*.json, sent at the app's next start)
constexpr const char* kReports = "uaro-graphics\\reports";
constexpr const char* kComputerKey = "uaro-graphics\\reports\\computer.txt";
// 3.0.18-beta.189: a report's check shots (gone in 3.0.18-beta.248) - their folder, emptied and removed when a report
// starts
constexpr const char* kReportChecks = "uaro-graphics\\reports\\checks";
// 3.0.18-beta.193: the launcher's art and the look picker's pictures (core/launcher_art), the package's
constexpr const char* kLauncherArt = "uaro-graphics\\launcher";
// 3.0.9-beta.4: the developer's answers not seen yet, as the game's watcher last heard them (core/report_answers) -
// the driver shows them in the game
constexpr const char* kAnswers = "uaro-graphics\\reports\\answers.json";
// 3.0.11-beta.5: the game's reference (core/reference_pack) - the site's pack of its game database and pack.json, the
// manifest of the pack there; the game's watcher keeps it fresh
constexpr const char* kReference = "uaro-graphics\\reference";
constexpr const char* kReferenceManifest = "uaro-graphics\\reference\\pack.json";
// 3.0.11-beta.8: the server's maps cell by cell for the reference's routes (core/walk_grid, tools/walk_grids.py) -
// the package's file, not the watcher's
constexpr const char* kWalkable = "uaro-graphics\\walkable.bin";
// 3.0.11-beta.10: the players on each map now (core/reference_live) - the game's watcher writes it every 2 minutes
constexpr const char* kReferenceOnline = "uaro-graphics\\reference\\online.json";
// 3.0.11-beta.174: the site's event or news now in its three languages (core/reference_now) - the watcher asks it at
// the game's start, at most every 10 minutes
constexpr const char* kReferenceNow = "uaro-graphics\\reference\\now.json";
constexpr const char* kReferenceSkills = "uaro-graphics\\reference\\skilldesc.json";  // 3.0.18-beta.244
// 3.0.11-beta.11: the site's world map sheet (core/world_map) - the watcher fetches it at the game's start
constexpr const char* kReferenceWorld = "uaro-graphics\\reference\\worldmap.json";
// 3.0.11-beta.12: the players' public MVP timers and the site's market (core/reference_live) - with online.json
constexpr const char* kReferenceLive = "uaro-graphics\\reference\\live.json";
// 3.0.11-beta.16: the site's quests (core/reference_quests) - their manifest; the file beside it
constexpr const char* kReferenceQuests = "uaro-graphics\\reference\\quests.json";
constexpr const char* kReferenceWalkable = "uaro-graphics\\reference\\walkable.json";  // 3.0.18-beta.246: the site's
// 3.0.11-beta.17: the sections of «Довідник»'s cards the player folded (a number: refpages::Part bits)
constexpr const char* kReferenceFolded = "uaro-graphics\\reference-folded.txt";
// 3.0.11-beta.18: «Довідник»'s cards opened lately and the starred ones (a line a card: refpages::linkText)
constexpr const char* kReferenceRecent = "uaro-graphics\\reference-recent.txt";
constexpr const char* kReferenceFavourites = "uaro-graphics\\reference-favourites.txt";
// 3.0.11-beta.31: the new minimap's zoom and whether its details are folded («zoom N», «folded 0|1» lines)
constexpr const char* kMinimapState = "uaro-graphics\\minimap-state.txt";
// 3.0.11-beta.35: the buffs' column - its names shown, its favourites («names 0|1», «favourites 10 12» lines)
constexpr const char* kBuffsState = "uaro-graphics\\buffs-state.txt";
// 3.0.11-beta.36: the character block's view («view full|compact|icon»)
constexpr const char* kCharacterState = "uaro-graphics\\character-state.txt";
// 3.0.11-beta.42: the new windows «Речі» and «Характеристики» - their places («at 0.1 0.13» a share of the free room
// to the left and the top), «Речі»'s tab («tab 0|1|2»)
constexpr const char* kItemsState = "uaro-graphics\\items-state.txt";
// 3.0.11-beta.113: «Речі»'s starred things, a line a character (core/item_window starredOf)
constexpr const char* kItemsStarred = "uaro-graphics\\items-starred.txt";
// 3.0.11-beta.115: «Склад»'s place and tab, as «Речі»'s
constexpr const char* kStorageState = "uaro-graphics\\storage-state.txt";
constexpr const char* kCartState = "uaro-graphics\\cart-state.txt";  // 3.0.11-beta.154: «Візок»'s place and tab
constexpr const char* kTradeState = "uaro-graphics\\trade-state.txt";  // 3.0.11-beta.116: «Обмін»'s place
constexpr const char* kTrackerState = "uaro-graphics\\tracker-state.txt";  // 3.0.11-beta.70: the tracker's pins
// 3.0.11-beta.75: «Розміщення інтерфейсу» - the parts the player moved or sized (core/hud_layout)
constexpr const char* kHudLayout = "uaro-graphics\\hud-layout.txt";
constexpr const char* kHudLayouts = "uaro-graphics\\layouts";  // 3.0.11-beta.288: each character's own
// 3.0.11-beta.78: each character's own hotkeys bar (core/hotbar_bars)
constexpr const char* kHotbarBars = "uaro-graphics\\hotbar-bars.txt";
// 3.0.11-beta.148 (2.6.17, parked till R-4FBB8B): the moment before which no character selection goes (Windows'
// seconds since 1601)
constexpr const char* kMapEntry = "uaro-graphics\\map-entry.txt";
// 3.0.11-beta.79: the party's panel folded or not («folded 0|1»)
constexpr const char* kPartyState = "uaro-graphics\\party-state.txt";
// 3.0.11-beta.80: the party's members' looks as last seen («<account> <sex> <hair> <colour> <class> <level>» lines)
constexpr const char* kPartyLooks = "uaro-graphics\\party-looks.txt";
constexpr const char* kStatsState = "uaro-graphics\\stats-state.txt";
// 3.0.11-beta.296: «Планування»'s plans by character (core/stat_window plansText)
constexpr const char* kStatPlans = "uaro-graphics\\stat-plans.txt";
constexpr const char* kCharDiary = "uaro-graphics\\characters.txt";  // 3.0.11-beta.273: core/char_diary
constexpr const char* kPortraits = "uaro-graphics\\portraits";  // 3.0.11-beta.273: win/portrait
constexpr const char* kMvpTimers = "uaro-graphics\\mvp.txt";  // 3.0.11-beta.280: core/mvp_timers
constexpr const char* kStash = "uaro-graphics\\stash.txt";  // 3.0.11-beta.286: core/stash_book
// 3.0.11-beta.96: «Навички»'s place («at 0.2 0.15») and its tab («tab 2»)
constexpr const char* kSkillsState = "uaro-graphics\\skills-state.txt";
// 3.0.11-beta.241: the chat's size by its corner («width 640», «rows 10»)
constexpr const char* kChatState = "uaro-graphics\\chat-state.txt";
// 3.0.18-beta.120: our windows closed by ✕ (core/closed_windows)
constexpr const char* kClosedWindows = "uaro-graphics\\closed-windows.txt";
// 3.0.11-beta.97: «Спорядження»'s place («at 0.5 0.22»)
constexpr const char* kEquipState = "uaro-graphics\\equip-state.txt";
// 3.0.11-beta.100: the shop's place («at 0.5 0.3»)
constexpr const char* kShopState = "uaro-graphics\\shop-state.txt";
// 2.14: the client patcher (app/client_patch): the list of the game's files last put, and where they arrive first
constexpr const char* kClientPatchRecord = "uaro-graphics\\client-patch.txt";
constexpr const char* kClientPatchWork = "uaro-graphics\\update\\client";
// The package's templates: a settings file that is not there yet starts from one of them (never at the settings'
// own place - an older update.bat copies uaro-graphics\ of the archive over the installed one).
constexpr const char* kIniTemplate = "uaro-graphics\\defaults\\uaro-graphics.ini";
constexpr const char* kPadTemplate = "uaro-graphics\\defaults\\uaro-pad.ini";
// 3.0.11-beta.89 (the concept «Контроллер в интерфейсе uaRO», stage 5): the last controller the game saw - its unix
// time, a tab, its name (pad/win/runtime writes it, the app's page «Контролер» shows it)
constexpr const char* kPadSeen = "uaro-graphics\\pad-seen.txt";

const char* now(File f);     // where 2.8 keeps it
const char* before(File f);  // where 2.x kept it
bool isFolder(File f);       // the settings app's backups, the client's dgVoodoo
// 3.0.2-beta.8: the AI textures' shaders kept beside a renderer's own (shaders-64.bin: ai-shaders-64.bin); "" none
std::string aiShadersBeside(const std::string& shaderCache);
// «Що нового»: the package's notes for a language ("en", "uk", "ru")
std::string notes(const std::string& langToken);
std::string join(const std::string& gameDir, const std::string& relative);
// The path to use: now's when it is there or before's is not (a fresh install), else before's - a move that could
// not happen (a game under Program Files, a pack held open) leaves the file working where it is.
std::string locate(const std::string& gameDir, File f, const std::function<bool(const std::string&)>& exists);
// A path relative to the game's folder as 2.x wrote it (a backup's files.txt), where that file is now; any other
// path as it is. Windows' names: any case.
std::string relativeNow(const std::string& relative);

}  // namespace uagfx::layout
