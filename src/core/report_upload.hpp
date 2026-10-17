#pragma once
#include <string>
#include <utility>
#include <vector>

namespace uagfx::report {

// 0.51: reports to the developers - uaro.kiev.ua's POST /api/graphics/reports (the site's admin lists them for 90
// days). The settings app sends one at a press (Diagnostics), and, with [report] auto = 1, its watcher sends one when
// a game ended badly (reasonOfLog).
constexpr const wchar_t* kHost = L"uaro.kiev.ua";
constexpr const wchar_t* kPath = L"/api/graphics/reports";

struct Upload {
    std::string reason = "manual";  // manual, start-failed, crash, gpu-lost
    bool automatic = false;
    std::string version, gpu, windows, api, comment;
    std::string report;                                       // the Diagnostics page's report
    std::vector<std::pair<std::string, std::string>> files;   // name -> text (logs, the ini)
    // 0.52: name -> a BMP's bytes - the pictures the texture pack has none of (uaro-graphics\missing\, 0.49.1)
    std::vector<std::pair<std::string, std::string>> images;
    std::string dump;  // 0.52: uaro-graphics\crash.dmp's bytes (win/crash.cpp), "" none
    // 1.8.1: name -> a JPEG's bytes - the player's screenshots of the last day (app/shots.hpp), when asked
    std::vector<std::pair<std::string, std::string>> shots;
};
constexpr size_t kMostShots = 6;
constexpr size_t kMostDump = 2u << 20;  // a bigger one stays at home
constexpr size_t kMostImages = 64;
std::string base64(const std::string& bytes);

// The request's JSON (the site's fields; text made valid UTF-8).
std::string uploadJson(const Upload& u);
// Bytes that are no UTF-8 (a path in the system's code page) as '?': the site reads JSON as UTF-8 only.
std::string validUtf8(const std::string& text);
// The last `most` bytes of a log from the start of a line, a line saying how much was left out first.
std::string tailOf(const std::string& text, size_t most);
// The site's answer {"code":"R-XXXXXX"}: the code.
bool codeOf(const std::string& answer, std::string& code);

// What went wrong in a game, from its uaro-graphics.log after it ended: "start-failed" (the uaRO renderer did not
// start), "gpu-lost" (the graphics card was lost), "crash" (the renderer ran and the game ended without closing it),
// "" when nothing did.
std::string reasonOfLog(const std::string& log);
// The graphics API the game's last start drew with, from its log: "d3d12", "d3d11", "system" (the system's
// DirectDraw: the renderer could not start), "" unknown.
std::string apiOfLog(const std::string& log);

}  // namespace uagfx::report
