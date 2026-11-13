#pragma once
#include <string>
#include <vector>

namespace uagfx::itemdesc {

// 3.0.18-beta.253 (R-WP985Q, the owner on an item's card in «Довідник»: «какие-то символы непонятные в карточке и
// описание мне кажется можно было сделать лучше»): the game's description of an item (data\idnum2itemdesctable.txt,
// the lines as the game keeps them - cut at about 25 letters, colour marks ^RRGGBB) read for the card: its facts apart
// («Weapon Class: ^777777Mace^000000», a value over several lines one line), the rest as paragraphs - the lines of a
// sentence joined, an effect («VIT +1», «Max HP +100») its own, a white «_» or an empty line a gap.
struct Fact {
    std::string key;    // as the game writes it («Weapon Class», «Required Level»)
    std::string value;  // on one line, its colour marks left out
};
struct Read {
    std::vector<Fact> facts;
    std::vector<std::string> paragraphs;
};
Read read(const std::vector<std::string>& lines);

// What a fact's key says (the card's own label for it); Unknown - one shown as the game writes it
enum class Key : int {
    Unknown = -1, Weight, Class, Attack, Defense, WeaponLevel, RequiredLevel, Jobs, Location, Slots, Element, CompoundOn
};
Key keyOf(const std::string& key);

}  // namespace uagfx::itemdesc
