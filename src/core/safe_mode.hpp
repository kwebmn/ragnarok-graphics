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
enum class Start { Unknown, Fine, Failed };
Start judge(const std::string& previousLog);
bool startFailed(const std::string& previousLog);  // judge(...) == Failed
int failedInRow(int before, Start last);
bool needed(int failedInRow, bool asked);
// Direct3D 11 (api = system stays), no 64-bit bridge, AI textures, effects, DLSS, frame generation, HDR, Remix or
// renderer thread.
void apply(Config& c);

}  // namespace uagfx::safe
