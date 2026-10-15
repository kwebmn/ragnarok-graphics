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
};

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
// The graphics API the game's last start drew with, from its log: "d3d12", "d3d11", "dgvoodoo", "" unknown.
std::string apiOfLog(const std::string& log);

}  // namespace uagfx::report
