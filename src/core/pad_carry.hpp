#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::padcarry {

// 3.0.11-beta.121 (the concept «Концепция окон и инвентаря», stage 5 - question 7 «ок»: «X — «Взяти», крестовина —
// к цели (по подсвеченным местам, и в другое окно тоже), A — «Покласти», B — отмена»; the owner 31.10 «приступай»):
// the controller carries a thing from window to window - what is carried, the places that take it, the D-pad's way
// from place to place. No Windows here (win/pad_carry).

enum class From : uint8_t { Bag, Skill, Storage };
struct Thing {
    From from = From::Bag;
    int index = 0;          // the bag's or the storage's place
    int id = 0;             // the item's number (a skill's: 0)
    int type = 0;           // the item's kind (ITEM_INFO +0)
    uint32_t location = 0;  // where a gear goes (0: none)
    int count = 0;
    std::string value;      // what a hotbar cell keeps: the item's number, «##AEGIS» for a skill
    int level = 0;          // a skill's level used
};

enum class To : uint8_t { Slot, Cell, Bag, Storage, Trade };
struct Place {
    To to = To::Cell;
    int index = 0;                     // the equipment's slot (core/equip_window Slot), the hotbar's slot
    float x = 0, y = 0, w = 0, h = 0;  // in the target
};

// Whether a place takes the thing: a gear its slots (as the equipment's drop), a hotbar cell anything of the bag or a
// skill, the bag the storage's things, the storage and the trade the bag's things
bool takes(const Thing& t, const Place& p);
std::vector<Place> placesFor(const Thing& t, const std::vector<Place>& all);  // those, in the order given

enum class Dir : uint8_t { Up, Down, Left, Right };
// The D-pad's next place: the nearest whose middle lies that way - no more across than twice along (the way along
// counted once, across it three times); `from` itself when there is none
int nextPlace(const std::vector<Place>& places, int from, Dir d);
int nearestPlace(const std::vector<Place>& places, float x, float y);  // to a point (where it was taken); -1 none

}  // namespace uagfx::padcarry
