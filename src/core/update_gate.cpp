#include "core/update_gate.hpp"

namespace uagfx::update_gate {

Next next(bool gameRuns, bool appOpen, bool appAsked) {
    if (gameRuns) return Next::WaitGame;
    if (appOpen) return appAsked ? Next::WaitApp : Next::CloseApp;
    return Next::Install;
}

std::string logLine(const std::string& stamp, const std::string& words) {
    std::string one = words;
    for (char& c : one)
        if (c == '\r' || c == '\n') c = ' ';
    return stamp + " " + one + "\r\n";
}

std::string capped(const std::string& log, size_t most) {
    if (log.size() <= most) return log;
    const size_t from = log.size() - most / 2;
    const size_t line = log.find('\n', from > 0 ? from - 1 : 0);
    return line == std::string::npos ? std::string() : log.substr(line + 1);
}

}  // namespace uagfx::update_gate
