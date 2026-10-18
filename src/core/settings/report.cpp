#include "core/settings/report.hpp"

#include <cctype>

#include "core/settings/ini_edit.hpp"

namespace uagfx::settings {

std::string vramText(unsigned long long mb) {
    const std::string text = std::to_string(mb) + " MB";
    // 0.51.1: exactly DXGI's cap - the settings app reads the driver's own number where it can (app/system.cpp)
    return mb == kVramSeenAtMost ? "at least " + text + " (a 32-bit program sees no more)" : text;
}

unsigned long long realVramMb(unsigned long long dxgiMb, unsigned long long registryMb) {
    return dxgiMb == kVramSeenAtMost && registryMb > dxgiMb ? registryMb : dxgiMb;
}

std::string maskUserPaths(const std::string& text) {
    std::string out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t hit = std::string::npos;
        for (size_t i = pos; i + 7 <= text.size(); ++i) {
            if ((text[i] == '\\' || text[i] == '/') && (text[i + 6] == '\\' || text[i + 6] == '/')) {
                std::string word = text.substr(i + 1, 5);
                for (char& ch : word) ch = char(std::tolower(static_cast<unsigned char>(ch)));
                if (word == "users") {
                    hit = i;
                    break;
                }
            }
        }
        if (hit == std::string::npos) {
            out += text.substr(pos);
            break;
        }
        const size_t nameStart = hit + 7;
        size_t nameEnd = nameStart;
        while (nameEnd < text.size() && text[nameEnd] != '\\' && text[nameEnd] != '/' && text[nameEnd] != '\n' &&
               text[nameEnd] != '\r')
            ++nameEnd;
        out += text.substr(pos, nameStart - pos) + "...";
        pos = nameEnd;
    }
    return out;
}

std::string makeReport(const ReportFacts& f) {
    std::string r = "uaRO Graphics report\r\n";
    r += "Settings app: " + f.utilityVersion + "\r\n";
    r += "Installed driver: " + (f.driverVersion.empty() ? std::string("unknown") : f.driverVersion) + "\r\n";
    r += "Windows: " + f.windows + "\r\n";
    r += "Graphics: " + f.gpu + "\r\n";
    for (const std::string& m : f.monitors) r += "Monitor: " + m + "\r\n";
    r += "Screen: " + f.screen + "\r\n";
    for (const std::string& m : f.inputModules) r += "Input: " + m + "\r\n";
    for (const std::string& c : f.compatFlags) r += "Compatibility: " + c + "\r\n";
    for (const std::string& p : f.problems) r += "Problem: " + p + "\r\n";
    r += "\r\nuaro-graphics.ini:\r\n";
    for (const auto& kv : readIni(f.iniText)) r += "  " + kv.first + " = " + kv.second + "\r\n";
    r += "\r\nuaro-graphics.log";
    if (!f.log.found) {
        r += ": none yet\r\n";
    } else {
        r += " (" + std::to_string(f.log.errorCount) + " errors):\r\n";
        for (const std::string& l : f.log.tail) r += "  " + l + "\r\n";
    }
    return maskUserPaths(r);
}

}  // namespace uagfx::settings
