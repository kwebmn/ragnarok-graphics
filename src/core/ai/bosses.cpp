#include "core/ai/bosses.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>

namespace uagfx::ai {
namespace {

const char* const kBosses[] = {
#include "core/ai/boss_sprites.inc"
};

}  // namespace

bool bossSprite(const std::string& name) {
    std::string lower = name;
    for (char& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
    return std::binary_search(std::begin(kBosses), std::end(kBosses), lower.c_str(),
                              [](const char* a, const char* b) { return std::strcmp(a, b) < 0; });
}

}  // namespace uagfx::ai
