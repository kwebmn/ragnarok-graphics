#pragma once
#include <string>

#include "core/config.hpp"

namespace uagfx::safe {

// 2.12: the safe mode - a game whose last two starts ended badly before their first picture (a crash, the renderer
// that could not start) starts on Direct3D 11 without the risky parts; [renderer] safe_mode = 1 asks for it. Only that
// start's settings change (in memory): the settings app explains it (uaro-graphics\cache\safe-mode.txt).
constexpr const char* kFirstPicture = "renderer: the first picture is on the screen";

// The last start by its log: Fine - kFirstPicture; Failed - a bad end (report::reasonOfLog) before it; Unknown -
// nothing of ours drawn (sent to «Гра», switched off, closed while it started, no game before): the count stays.
// 3.0.11-beta.59 (the autopilot): FailedEarly - a bad end within kEarlySeconds after the first picture (the log's
// HH:MM:SS times; past midnight a day added; a log without times: Fine as before) - a crash at the login or at a map's
// loading repeats at every start, and counts as a start that failed.
enum class Start { Unknown, Fine, Failed, FailedEarly };
constexpr int kEarlySeconds = 300;
Start judge(const std::string& previousLog);
bool startFailed(const std::string& previousLog);  // judge(...) == Failed or FailedEarly
int failedInRow(int before, Start last);
bool needed(int failedInRow, bool asked);
// Direct3D 11 (api = system stays), no 64-bit bridge, AI textures, effects, DLSS, frame generation, HDR, Remix or
// renderer thread.
void apply(Config& c);
// 3.0.11-beta.59: the automatic safe mode's note (layout::kSafeModeNote) - its first line "version <V>", the version
// that wrote it: a start of another version deletes it and tries the normal mode again; "" a note of 2.12's
std::string noteText(int inRow, const std::string& version);
std::string noteVersion(const std::string& note);

}  // namespace uagfx::safe
