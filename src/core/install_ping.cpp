#include "core/install_ping.hpp"

#include <cstdio>

namespace uagfx::ping {

bool due(long long now, long long last) { return last <= 0 || now - last >= kEverySeconds || now < last; }

bool validId(const std::string& id) {
    if (id.size() != 16) return false;
    for (const char c : id)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

std::string newId(uint64_t x) {
    for (int i = 0; i < 4; ++i) {  // splitmix64: the bits spread
        x += 0x9e3779b97f4a7c15ull;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        x ^= x >> 31;
    }
    char id[24];
    std::snprintf(id, sizeof id, "%016llx", static_cast<unsigned long long>(x));
    return id;
}

std::string idOf(const std::string& text) {
    std::string line = text.substr(0, text.find_first_of("\r\n"));
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
    return validId(line) ? line : std::string();
}

std::string body(const std::string& id, const std::string& version, const std::string& channel) {
    std::string v;
    for (const char c : version)
        if ((c >= '0' && c <= '9') || c == '.' || c == '-' || (c >= 'a' && c <= 'z')) v += c;
    if (v.size() > 32) v.resize(32);
    const std::string ch = channel == "beta" ? "beta" : "release";
    return "{\"id\":\"" + (validId(id) ? id : std::string()) + "\",\"version\":\"" + v + "\",\"channel\":\"" + ch +
           "\"}";
}

}  // namespace uagfx::ping
