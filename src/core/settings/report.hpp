#pragma once
#include <string>
#include <vector>

#include "core/settings/log_digest.hpp"

namespace uagfx::settings {

// What the forum report says (stage 2 spec §7). It carries no login, character or server names: there is no place
// for them here.
struct ReportFacts {
    std::string utilityVersion;
    std::string driverVersion;               // the installed build, "" when unknown
    std::string windows;                     // "10.0 build 26200"
    std::string gpu;                         // "NVIDIA GeForce RTX 5060 Ti, 16311 MB, Direct3D 12: OK"
    std::vector<std::string> monitors;       // one line each
    std::string iniText;                     // uaro-graphics.ini: only "section.key = value" goes into the report
    std::string screen;                      // what the game gets: "window 3360x1890, interface 175 %, game 1920x1080"
    std::vector<std::string> inputModules;   // "dinput.dll: uaRO Pad"
    std::vector<std::string> compatFlags;    // "HKCU: RUNASADMIN"
    std::vector<std::string> problems;       // red and yellow lines of the Diagnostics page
    LogDigest log;
};
// DXGI tells a 32-bit program no more than 3072 MB of video memory: from there on the number is the least the card
// has, not its size.
constexpr unsigned long long kVramSeenAtMost = 3072;
std::string vramText(unsigned long long mb);  // "2048 MB"; "at least 3072 MB (a 32-bit program sees no more)"
// The text, in English for the developers; Windows user names in paths are hidden.
std::string makeReport(const ReportFacts& f);
std::string maskUserPaths(const std::string& text);  // "\Users\<name>\" -> "\Users\...\"

}  // namespace uagfx::settings
