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
std::vector<uint8_t> pack(uint64_t key, const std::vector<Shader>& shaders);
std::vector<Shader> unpack(uint64_t key, const std::vector<uint8_t>& file);

}  // namespace uagfx::shader_cache
