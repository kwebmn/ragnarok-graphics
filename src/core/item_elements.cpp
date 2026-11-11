#include "core/item_elements.hpp"

#include <algorithm>

namespace uagfx::itemelements {

int ofItem(int id) {
    const Entry* end = kTable + kTableSize;
    const Entry* e = std::lower_bound(kTable, end, id, [](const Entry& a, int b) { return a.id < b; });
    return e != end && e->id == id ? e->element : 0;
}

int ofThing(int id, const int cards[4]) {
    if (cards && cards[0] == 0xFF) {  // forged: its own element (1-4) first
        const int forged = cards[1] & 0xFF;
        if (forged >= 1 && forged <= 4) return forged;
    }
    if (const int own = ofItem(id)) return own;
    if (cards && cards[0] != 0xFF && cards[0] != 0xFE && cards[0] != 0xFF00)
        for (int i = 0; i < 4; ++i)
            if (cards[i] > 0)
                if (const int e = ofItem(cards[i])) return e;
    return 0;
}

uint32_t colour(int element) {
    // 3.0.18-beta.149 (the owner: wind as «желтые молнии»): wind lightning yellow, holy near white beside it
    static const uint32_t kColours[10] = {0,        0x5AB4FF, 0xD2A15C, 0xFF6B45, 0xFFE14A,
                                          0xB98AF0, 0xFFF8E6, 0x9C8CFF, 0xC8D4E4, 0x8FB3A6};
    return element >= 1 && element <= 9 ? kColours[element] : 0;
}

const char* word(int element) {
    static const char* const kWords[10] = {"",     "Water", "Earth",  "Fire",  "Wind",
                                           "Poison", "Holy", "Shadow", "Ghost", "Undead"};
    return element >= 1 && element <= 9 ? kWords[element] : "";
}

}  // namespace uagfx::itemelements
