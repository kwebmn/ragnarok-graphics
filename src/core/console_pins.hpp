#pragma once
#include <string>
#include <vector>

namespace uagfx::consolepins {

// 3.0.17-beta.4 (the concept «простой старт і налаштування»: «закріпити ★» in the game's quick settings): the
// console's rows the player pinned to «Швидко» - uaro-graphics\cache\console-pins.txt, a "section.key" a line, the
// order they were pinned in, kMost at most (the oldest go)

constexpr size_t kMost = 24;

std::vector<std::string> parse(const std::string& text);  // a line that is not "section.key" is left out
std::string format(const std::vector<std::string>& pins);
std::vector<std::string> toggle(std::vector<std::string> pins, const std::string& key);
bool pinned(const std::vector<std::string>& pins, const std::string& key);

}  // namespace uagfx::consolepins
