#include "core/console_pins.hpp"

#include <algorithm>
#include <cctype>

namespace uagfx::consolepins {
namespace {

bool isKey(const std::string& s) {
    const size_t dot = s.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 == s.size() || s.size() > 64) return false;
    for (const char c : s)
        if (!(std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) ||
              c == '_' || c == '.'))
            return false;
    return true;
}

}  // namespace

std::vector<std::string> parse(const std::string& text) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at <= text.size()) {
        const size_t end = std::min(text.find('\n', at), text.size());
        std::string line = text.substr(at, end - at);
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) line.erase(0, 1);
        if (isKey(line) && !pinned(out, line)) out.push_back(line);
        at = end + 1;
    }
    while (out.size() > kMost) out.erase(out.begin());
    return out;
}

std::string format(const std::vector<std::string>& pins) {
    std::string out;
    for (const std::string& p : pins) out += p + "\r\n";
    return out;
}

std::vector<std::string> toggle(std::vector<std::string> pins, const std::string& key) {
    const auto it = std::find(pins.begin(), pins.end(), key);
    if (it != pins.end()) {
        pins.erase(it);
        return pins;
    }
    if (!isKey(key)) return pins;
    pins.push_back(key);
    while (pins.size() > kMost) pins.erase(pins.begin());
    return pins;
}

bool pinned(const std::vector<std::string>& pins, const std::string& key) {
    return std::find(pins.begin(), pins.end(), key) != pins.end();
}

}  // namespace uagfx::consolepins
