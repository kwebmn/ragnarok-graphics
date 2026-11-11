// 3.0.18-beta.174 (R-ZGN2US, the beta tester's crash at a map's entry: a read at 0x5 in uaRO.exe 0x4ffb7e): the game's
// FUN_00628e50 takes a worn item's places by the bits 0-9 of its mask into a local of three - more than three of them
// wrote past it over its own stack (the 6th bit's index, 5, over its argument), none of them (only bits it does not
// know, not the arrows' 0x8000) put the item at place -1 before its array. The driver lets it take the rest alone.
#include "core/equip_exe.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(equip_exe_lets_the_game_take_items_of_up_to_three_known_places_or_the_arrows) {
    CHECK(equip_exe::wearable(0x0001));   // the lower head
    CHECK(equip_exe::wearable(0x0301));   // a headgear on three places (upper, middle, lower)
    CHECK(equip_exe::wearable(0x0022));   // a weapon held in both hands
    CHECK(equip_exe::wearable(0x8000));   // the arrows' place (15)
    CHECK(equip_exe::wearable(0x8001));   // the arrows' bit with a known place: by the known one, as the game does
    CHECK(!equip_exe::wearable(0x000F));  // four places: past its local of three
    CHECK(!equip_exe::wearable(0x003F));  // six: the crash's - the 6th index (5) over its argument
    CHECK(!equip_exe::wearable(0x03FF));  // all ten
    CHECK(!equip_exe::wearable(0x0400));  // a place it does not know (a costume's): place -1, before its array
    CHECK(!equip_exe::wearable(0x8400));  // the same with the arrows' bit
    CHECK(!equip_exe::wearable(0));       // nothing worn: never given to it (its caller's own check), refused too
}
