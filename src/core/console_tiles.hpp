#pragma once
#include <string>
#include <vector>

namespace uagfx::consoletiles {

// 3.0.17-beta.27 (the concept «простой старт», Task 6): the console's «Швидко» - its switches as tiles, four a row;
// a row of them one console row (its keys joined by '|'), left and right move the choice within it
constexpr int kPerRow = 4;
std::vector<std::vector<std::string>> rows(const std::vector<std::string>& keys);
std::string join(const std::vector<std::string>& keys);
// 3.0.20-beta.6 (the owner: «добавь в консоль эту настройку чтобы во время игры можно было переключать»): «Швидко»'s
// switches - the rows switched most, «Моделі нашим рендером» while it is tried
const std::vector<const char*>& quickKeys();
// 3.0.20-beta.6: a section's rows as the console lists them («Усе», its search) - the schema's order, the developer's
// left out
std::vector<std::string> sectionRows(const char* section);
std::vector<std::string> split(const std::string& joined);
int move(int col, int step, int count);              // within 0..count-1, the edges kept (step 0: kept in it)
int at(int x, int left, int width, int count);       // the tile under x of a row `width` wide (kPerRow places)

// Task 8: «що це дає» - a hint's first sentence (to its first ". " or line end), at most `most` letters (UTF-8), cut
// at a word with «…»
std::string gist(const std::string& hint, size_t most);

}  // namespace uagfx::consoletiles
