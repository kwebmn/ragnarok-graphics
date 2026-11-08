#pragma once
#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

namespace uagfx::upkeep {

// 3.0.16-beta.13 (the owner's picks 19, 20 of «20 идей»), from uAthena's clif.c / pet.c / mercenary.c:
// - broken gear: a piece broken gets attribute 1, is taken off and the equipment list 0x00a4 comes again - <len>.W,
//   20 bytes each from 4: <index>.W <item>.W at +2 ... <attribute>.B at +10;
// - the pet's fullness (0-100; hungry at 25 and less, very at 10): 0x01a2 <hungry>.W at 29 (its status), 0x01a4
//   <type>.B 2 <unit>.L 3 <value>.L 7 - type 0 it came out, 2 its fullness (to its owner alone);
// - the homunculus's: 0x022e <flags>.B at 26 (bit 1 put to rest, bit 2 dead) <hunger>.W at 29; 0x0230 <type>.W 2
//   (0x200 the hunger) <unit>.L 4 <value>.L 8;
// - 0x0080 <unit>.L <type>.B: its unit gone - not known; a map change (0x0091, 0x0092) forgets both till told again.
class Watch {
public:
    // A packet the character got; true when something here changed
    bool packet(uint16_t id, const uint8_t* p, size_t size);
    const std::set<int>& broken() const { return broken_; }  // the broken pieces' items
    std::vector<int> takeFresh();  // broken since the first list (once each)
    int pet() const { return pet_; }      // < 0 none out
    int homun() const { return homun_; }  // < 0 none out

private:
    std::set<int> broken_;
    std::vector<int> fresh_;
    bool listed_ = false;
    int pet_ = -1, homun_ = -1;
    uint32_t petUnit_ = 0, homunUnit_ = 0;
};

// How hungry: 0 fine, 1 hungry (25 or less), 2 very (10 or less)
int hungerLevel(int fullness);

}  // namespace uagfx::upkeep
