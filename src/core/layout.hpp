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
    StudioShaders64, Pack, Backups, DgVoodoo
};
constexpr int kFileCount = 14;

constexpr const char* kOurs = "uaro-graphics";
constexpr const char* kLogs = "uaro-graphics\\logs";
constexpr const char* kCache = "uaro-graphics\\cache";
constexpr const char* kTextures = "uaro-graphics\\textures";

const char* now(File f);     // where 2.8 keeps it
const char* before(File f);  // where 2.x kept it
bool isFolder(File f);       // the settings app's backups, the client's dgVoodoo
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
