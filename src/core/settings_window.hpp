#pragma once
#include <string>
#include <vector>

#include "core/settings/strings.hpp"

namespace uagfx::settingswin {

// 3.0.18-beta.122 (the concept «Ігровий інтерфейс», Claude Doc 3zHfHfPfhA5nCjqGU5xb1Z, stages 5-6; the owner: «можна
// заменять но чтобы там были те возможности которые там сейчас есть»): «Налаштування» - the game's options window
// (Alt+O) in a new look with ours beside its own. Its sections: «Гра» (the game's six: music, sounds, the three snaps,
// opaque windows - keys "game.0".."game.5"), «Картинка» (the three choices of the simple start - "picture.0".."2"),
// then the settings app's «Інтерфейс» sections (their keys of uaro-graphics.ini's schema)
enum class Kind : uint8_t { Game, Picture, Keys };
struct Section {
    settings::Text name;
    Kind kind;
    std::vector<std::string> keys;
};
const std::vector<Section>& sections();
// The search: each section's rows matching every word of the query (labels and hints in the language; the game's own
// and the choices by their names), the sections in order; an empty query finds nothing
struct Found {
    int section = 0;
    std::string key;
};
std::vector<Found> find(const std::string& query, settings::Lang l);

}  // namespace uagfx::settingswin
