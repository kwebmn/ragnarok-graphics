#pragma once
#include <map>
#include <string>
#include <vector>

#include "core/sprite_file.hpp"

namespace uagfx::win::packframes {

// 3.0.11-beta.76 (monster pictures), 3.0.18-beta.114 (the character in «Спорядження»): the AI texture pack's pictures
// of a sprite's palette frames (`used`, by their numbers) - each its frame `factor` times bigger (the pack's cells are
// 4x the game's sprite cache cells). `want`: only that factor (0 - the first found); none - the switch off, the pack's
// directory not read yet, or no such frames
struct Frames {
    int factor = 0;
    std::map<int, spritefile::Image> big;
    const spritefile::Image* at(int index) const {
        const auto it = big.find(index);
        return it != big.end() ? &it->second : nullptr;
    }
};
Frames of(const std::string& spr, const spritefile::Sprite& sp, const std::vector<int>& used, int want = 0);
// The palette frames an action's frame draws
std::vector<int> usedBy(const spritefile::Sprite& sp, const spritefile::Act& ac, size_t action, size_t frame);

}  // namespace uagfx::win::packframes
