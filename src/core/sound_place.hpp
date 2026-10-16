#pragma once
#include <string>

#include "core/outdoors.hpp"

namespace uagfx::sound {

// 1.1: what the place sounds like (the new engine's reverberation): a field hardly answers, a town's walls a little,
// a house's room near and short, a dungeon or a cave long.
enum class Room { Open, Town, Indoor, Cave };

inline Room roomOf(const std::string& map) {
    if (map.empty()) return Room::Open;
    const auto has = [&](const char* s) { return map.find(s) != std::string::npos; };
    if (has("dun") || has("cave") || has("anthell") || has("gld") || has("gl_") || has("tha_t") || has("ein_d") ||
        has("abyss") || has("mag_d") || has("ice_") || has("thor_") || has("juperos") || has("lhz_d") || has("@"))
        return Room::Cave;
    if (has("_in") || has("in_") || has("_room") || has("que_") || has("job_") || has("cas") || has("prt_church"))
        return Room::Indoor;
    if (!outdoors(map)) return Room::Indoor;
    if (has("_fild") || has("_fd") || has("field") || has("beach") || has("mjolnir") || has("_cas0")) return Room::Open;
    return Room::Town;
}

}  // namespace uagfx::sound
