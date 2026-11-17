#pragma once
#include <set>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settingswin {

// 3.0.18-beta.122 (the concept «Ігровий інтерфейс», Claude Doc 3zHfHfPfhA5nCjqGU5xb1Z, stages 5-6; the owner: «можна
// заменять но чтобы там были те возможности которые там сейчас есть»): «Налаштування» - the game's options window
// (Alt+O) in a new look with ours beside its own.
// 3.0.21-beta.3 (the concept «Настройки uaRO Graphics», Claude Doc JzcE4xZDus82grUxsQjgy7; the owner: «Вариант 4.
// Каждому месту своё дело», step 3): the player's one place in the game - every row that applies at once, read from
// the catalogue (core/settings/catalogue), no list of its own. The list on the left: «Як виглядає гра» first (the
// simple start's three choices - keys "picture.0".."2" - and the game's own six: music, sounds, the three snaps,
// opaque windows - "game.0".."game.5": nothing the game's Alt+O had is lost), then the catalogue's areas that hold
// rows living in the game, each with their count. The pane: an entry's groups one under another - a group's name,
// its basic rows, then its details (the parts, the sliders, each window's own switch).
// 3.0.21-beta.11 (the owner: «раскрытие в настройках лучше убрать, пускай сразу будет все вместе, можно както
// выделить внутри что это подгрупа, но раскрытие делать не стоит»): the details stand at once under a small heading
// of their own - no line that opens or folds them (3.0.21-beta.3's «Ще N налаштувань ›»).
enum class Kind : uint8_t { Game, Picture, Keys };

struct Nav {
    settings::Text name;
    int area = -1;  // the catalogue's area; -1 the first entry («Як виглядає гра»); kChangedArea the last, «Змінені»
    int rows = 0;   // the rows it holds
};
// 3.0.21-beta.10: the list's last entry, «Змінені» - the rows off their defaults (changedLines; lines() gives none)
constexpr int kChangedArea = -2;
const std::vector<Nav>& nav();

// The first entry's two groups (the catalogue's groups are 0 and up)
constexpr int kLookGroup = -1, kGameGroup = -2;

struct Line {
    enum class What : uint8_t { Row, Sub };  // Sub: the small heading over a group's details («Додатково»)
    What what = What::Row;
    Kind kind = Kind::Keys;
    std::string key;         // Row: "interface.minimap", "game.0", "picture.1"
    int group = kLookGroup;  // the group it is in
    int entry = 0;           // the list's entry its group is in (the search's lines name it)
    settings::Text groupName = settings::Text::LookHead;
    bool heads = false;      // its group's first line: the group's name goes over it
    bool part = false;       // a part of a row over it
    bool detail = false;     // a row of the group's inner part: after the small heading
};
// An entry's lines: each group's basic rows, then - under the small heading - its details; a group of details alone
// has no heading
std::vector<Line> lines(int entry);
// The search: every row of every entry matching each word of the query (labels and hints in the language; the game's
// own and the choices by their names), details too, under their groups' names; an empty query finds nothing
std::vector<Line> find(const std::string& query, settings::Lang l);

// 3.0.21-beta.10 (the owner after R-MSAM96 - eight sliders to bring back by hand: «надо кнопку по умолчанию сделать.
// чтобы не искать»): the window knows what is not as shipped.
// The settings as shipped, as the window shows them: the driver's own defaults but «Якість ефектів» (the shipped
// file says auto), each effect's switch as that set gives it on a card of this level (core/effect_groups shown)
Config defaults(int cardLevel);
// A row of the window off its default (`shown`: the settings as the window shows them). An effect's own switch
// counts only while «Якість ефектів» is «Свої»: a set decides it otherwise, and the set's row is the one off
bool changed(const std::string& key, const Config& shown, const Config& defaults);
// A group's rows off their defaults with the values that bring them back, in the group's order
struct Reset {
    std::string key, token;
};
std::vector<Reset> resetsOf(int group, const Config& shown, const Config& defaults);
// «Змінені»: every row off its default, under its group's name (as the search's lines)
std::vector<Line> changedLines(const Config& shown, const Config& defaults);
// What an entry says over its rows: «3D-режим» in the usual view - its rows do nothing now. Text::Count: nothing
settings::Text noteOf(int entry, bool view3d);

}  // namespace uagfx::settingswin
