#include "core/console_tiles.hpp"

#include <cstdint>

namespace uagfx::consoletiles {

std::string gist(const std::string& hint, size_t most) {
    size_t end = hint.size();
    for (size_t i = 0; i < hint.size(); ++i) {
        if (hint[i] == '\n') {
            end = i;
            break;
        }
        if (hint[i] == '.' && (i + 1 == hint.size() || hint[i + 1] == ' ')) {
            end = i + 1;
            break;
        }
    }
    std::string s = hint.substr(0, end);
    // letters, not bytes: a UTF-8 letter's continuation bytes are 10xxxxxx
    size_t letters = 0, cut = std::string::npos;
    for (size_t i = 0; i < s.size(); ++i)
        if ((uint8_t(s[i]) & 0xC0) != 0x80 && ++letters > most) {
            cut = i;
            break;
        }
    if (cut == std::string::npos) return s;
    const size_t space = s.rfind(' ', cut);
    s = s.substr(0, space == std::string::npos || space == 0 ? cut : space);
    while (!s.empty() && (s.back() == ' ' || s.back() == ',')) s.pop_back();
    return s + "\xE2\x80\xA6";
}

}  // namespace uagfx::consoletiles
