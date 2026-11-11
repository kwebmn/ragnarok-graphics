#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::itemelements {

// 3.0.18-beta.144 (the owner: element things such as a wind weapon in their own colour): a thing's element -
// RO's numbers (1 water, 2 earth, 3 fire, 4 wind, 5 poison, 6 holy, 7 shadow, 8 ghost, 9 undead; 0 none) - from the
// server's item_db (tools/item_elements.py: a weapon's bAtkEle, an armour's or an armour card's bDefEle), a forged
// thing's own (its second card place's low byte, 1-4); the colour its name and cell take
struct Entry {
    int id;
    int element;
    bool attack;  // a weapon's (else an armour's or a card's for one)
};
extern const Entry kTable[];
extern const size_t kTableSize;
int ofItem(int id);                      // the table's, 0 none
int ofThing(int id, const int cards[4]);  // forged > its own > a card's in it
uint32_t colour(int element);            // 0xRRGGBB; 0 none
const char* word(int element);           // "Wind", "Fire" ... ("" none)

}  // namespace uagfx::itemelements
