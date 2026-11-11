#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace uagfx::cardnames {

// 3.0.18-beta.112 (the owner: «там вещи подписывались типа Saharic Jur»): a thing's name with its cards as the game
// names it - each card's word (cardprefixnametable.txt: «4001#Lucky#») before the name, «Double», «Triple»,
// «Quadruple» for the same card twice and more; a card of cardpostfixnametable.txt («4014#») after it: «Clip of
// Counter»
struct Tables {
    std::map<int, std::string> words;
    std::set<int> after;
};
Tables read(const std::string& prefixText, const std::string& postfixText);
// `name` without its refine; `cards` its four card places (0 none; a forged or named thing's makers are not cards:
// 255, 254, 0xFF00 in the first place - its name as it is)
std::string named(const std::string& name, const int cards[4], const Tables& t);
// 3.0.18-beta.118: a thing's four card places from its ITEM_INFO (72 bytes: +0x1C, ints)
void cardsOf(const uint8_t* itemInfo, int cards[4]);
// Its name with its cards when it is identified (ITEM_INFO +0x3C), else `name` as it is
std::string namedFromInfo(const std::string& name, const uint8_t* itemInfo, const Tables& t);

}  // namespace uagfx::cardnames
