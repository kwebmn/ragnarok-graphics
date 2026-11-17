#pragma once
#include <cstddef>
#include <string>

namespace uagfx::consoletiles {

// 3.0.17-beta.27 (the concept «простой старт», Task 8): «що це дає» - a hint's first sentence (to its first ". " or
// line end), at most `most` letters (UTF-8), cut at a word with «…». Under the console's chosen row and under a row of
// «Налаштування». (3.0.21-beta.7: «Швидко»'s tiles, which gave the file its name, are gone with the tab.)
std::string gist(const std::string& hint, size_t most);

}  // namespace uagfx::consoletiles
