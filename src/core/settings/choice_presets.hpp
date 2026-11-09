#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings::presets {

// 3.0.17-beta.2 (the concept «простой старт і налаштування»: «пресеты» for the players who will not sort hundreds of
// switches): three choices, each its own keys of uaro-graphics.ini so any mix works - «Картинка» (the effects'
// preset and tier), «Інтерфейс» (the new interface whole, its screen with the game's windows, or the game's own),
// «Текстури» (the HD texture pack, lighter, or the game's pictures). A choice writes only its keys; what the player
// changed by hand shows as «змінено N».

enum class Choice : int { Picture, Interface, Textures };
constexpr int kChoices = 3;
enum class Picture : int { Auto, Game, Better, Remaster };  // «Авто», «Як у грі», «Покращена», «Ремастер»
enum class Ui : int { New, NewScreen, Classic };            // «Новий», «Новий екран, вікна гри», «Класичний»
enum class Textures : int { Hd, HdLight, Original };        // «HD», «HD економно», «Оригінальні»

int variants(Choice c);
// The variant's keys: "section.key" -> the token it writes
std::vector<std::pair<std::string, std::string>> keysOf(Choice c, int variant);

struct Match {
    int variant = 0;  // the nearest
    int changed = 0;  // how many of its keys the settings have otherwise
};
Match match(Choice c, const Config& config);

Text headOf(Choice c);             // «Картинка», «Інтерфейс», «Текстури»
Text nameOf(Choice c, int variant);  // the variant's words

// uaro-graphics.ini's text with the variant's keys written (every other byte as it was)
std::string apply(const std::string& ini, Choice c, int variant);

}  // namespace uagfx::settings::presets
