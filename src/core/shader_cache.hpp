#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::shader_cache {

// 0.39.5: the shaders compiled once and kept between runs - a start reads their code from a file instead of compiling
// them again (3.5 s of every start on a player's machine). The file holds the code of each entry point and profile,
// made for one key: the source's hash and what else decides the code; another key, a file cut short or damaged, and
// nothing of it is used.
struct Shader {
    std::string entry;
    std::string profile;
    std::vector<uint8_t> code;
};

uint64_t keyOf(const std::string& source);  // the source with the compiler's settings of this version

// 2.2.2: a shader compiled without the newest features (UAGFX_SAFE: the player's compiler would not take them) is
// kept with "+safe" after its profile, so that the next starts know it as well.
std::string profileOf(const std::string& profile, bool safe);
// Whether a kept shader is the one wanted (entry and profile); `safe`: compiled so.
bool matches(const Shader& s, const std::string& entry, const std::string& profile, bool& safe);
std::vector<uint8_t> pack(uint64_t key, const std::vector<Shader>& shaders);
// 3.0.6-beta.3: the optimiser levels are tried in turn (3, 1, none, then without the newest features); which of them
// to begin with for an entry - 1 for ray_glow_ps, whose level 3 fails inside FXC on every computer seen and whose level
// 1 made it each time (the same code, no failed attempt at a first start), 0 for the others
int firstTry(const char* entry);
std::vector<Shader> unpack(uint64_t key, const std::vector<uint8_t>& file);

}  // namespace uagfx::shader_cache
