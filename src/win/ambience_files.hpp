#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/rsw.hpp"

namespace uagfx::win::ambience_files {

// 3.0.4-beta.7 (the user's idea 6): a map's own sounds read on a thread of its own - its .rsw's sound objects and
// their files from data\wav\ (win/game_files.hpp: the data folder, updates\, DATA.INI's archives). The sound engine
// asks at a map's change and takes the result when it is there.
struct MapSounds {
    std::string map;
    std::vector<rsw::Sound> sounds;
    std::map<std::string, std::vector<uint8_t>> files;  // by the sound's file name, lower case: its RIFF WAVE image
};

void ask(const std::string& gameDir, const std::string& map);
bool take(MapSounds& out);  // the latest one read, once

}  // namespace uagfx::win::ambience_files
