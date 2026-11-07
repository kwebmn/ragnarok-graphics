#include "core/world_signs.hpp"

namespace uagfx::worldsigns {

uint32_t accent(Kind k) {
    switch (k) {
        case Kind::Shop: return 0xE6CB8A;
        case Kind::Room: return 0x6CB4E8;
        case Kind::Locked: return 0x9AA3B2;
    }
    return 0xE6CB8A;
}

std::string places(int users, int limit) {
    if (limit <= 0) return {};
    return std::to_string(users < 0 ? 0 : users) + "/" + std::to_string(limit);
}

std::string shortTitle(const std::string& utf8, size_t most) {
    size_t b = 0, e = utf8.size();
    while (b < e && (utf8[b] == ' ' || utf8[b] == '\t')) ++b;
    while (e > b && (utf8[e - 1] == ' ' || utf8[e - 1] == '\t')) --e;
    const std::string t = utf8.substr(b, e - b);
    size_t letters = 0, at = 0;
    while (at < t.size()) {
        if (letters == most) {
            std::string cut = t.substr(0, at);
            while (!cut.empty() && cut.back() == ' ') cut.pop_back();
            return cut + "\xE2\x80\xA6";
        }
        const unsigned char c = static_cast<unsigned char>(t[at]);
        at += c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        ++letters;
    }
    return t;
}

}  // namespace uagfx::worldsigns
