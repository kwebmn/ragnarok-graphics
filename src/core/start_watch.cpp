#include "core/start_watch.hpp"

#include <cstdio>

namespace uagfx::startwatch {

namespace {
const Module* moduleOf(const std::vector<Module>& mods, uint32_t address) {
    for (const Module& m : mods)
        if (address >= m.base && address - m.base < m.size) return &m;
    return nullptr;
}
}  // namespace

std::string placeOf(const std::vector<Module>& mods, uint32_t address) {
    char text[32];
    if (const Module* m = moduleOf(mods, address)) {
        std::snprintf(text, sizeof text, "+0x%X", unsigned(address - m->base));
        return m->name + text;
    }
    std::snprintf(text, sizeof text, "0x%08X", unsigned(address));
    return text;
}

std::string where(const std::vector<Module>& mods, uint32_t at, const uint32_t* stack, size_t words,
                  const std::function<bool(uint32_t)>& afterCall, size_t most) {
    std::string out = placeOf(mods, at);
    size_t told = 0;
    for (size_t i = 0; i < words && told < most; ++i) {
        const uint32_t v = stack[i];
        if (!moduleOf(mods, v) || !afterCall(v)) continue;
        out += " <- " + placeOf(mods, v);
        ++told;
    }
    return out;
}

}  // namespace uagfx::startwatch
