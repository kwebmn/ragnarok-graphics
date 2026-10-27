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
constexpr const char* kFailedStarts = "uaro-graphics\\cache\\failed-starts.txt";
constexpr const char* kSafeModeNote = "uaro-graphics\\cache\\safe-mode.txt";
// 3.0.3-beta.2: where each character's game window was put (core/window_places), written by the games
constexpr const char* kWindowPlaces = "uaro-graphics\\uaro-windows.txt";
// 2.13: «Звернення» (app/reports): the computer's random key - the site's reports belong to it, no login - and the
// reports the site did not take yet (queued-*.json, sent at the app's next start)
constexpr const char* kReports = "uaro-graphics\\reports";
constexpr const char* kComputerKey = "uaro-graphics\\reports\\computer.txt";
// 3.0.9-beta.4: the developer's answers not seen yet, as the game's watcher last heard them (core/report_answers) -
// the driver shows them in the game
constexpr const char* kAnswers = "uaro-graphics\\reports\\answers.json";
// 2.14: the client patcher (app/client_patch): the list of the game's files last put, and where they arrive first
constexpr const char* kClientPatchRecord = "uaro-graphics\\client-patch.txt";
constexpr const char* kClientPatchWork = "uaro-graphics\\update\\client";
// The package's templates: a settings file that is not there yet starts from one of them (never at the settings'
// own place - an older update.bat copies uaro-graphics\ of the archive over the installed one).
constexpr const char* kIniTemplate = "uaro-graphics\\defaults\\uaro-graphics.ini";
constexpr const char* kPadTemplate = "uaro-graphics\\defaults\\uaro-pad.ini";

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
