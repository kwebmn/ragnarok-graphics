// 3.0.11-beta.159 (task 9 of the remaining windows: the pet, Alt+J): the game's pet window (UI window 0x58) as a card
// (core/pet_card) - the pet's name and level, its fullness and closeness with the game's own five states each, its
// four commands the window's own (its message 0x27: feed, perform, the accessory off, back into the egg)
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/pet_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

// The game's thresholds (FUN_00464410): fullness 10 / 25 / 75 / 90, closeness 100 / 250 / 750 / 900
TEST(pet_card_states_as_the_game_has_them) {
    CHECK(petcard::hungerState(0) == 0 && petcard::hungerState(10) == 0 && petcard::hungerState(11) == 1);
    CHECK(petcard::hungerState(25) == 1 && petcard::hungerState(26) == 2 && petcard::hungerState(75) == 2);
    CHECK(petcard::hungerState(76) == 3 && petcard::hungerState(90) == 3 && petcard::hungerState(91) == 4);
    CHECK(petcard::intimacyState(100) == 0 && petcard::intimacyState(101) == 1 && petcard::intimacyState(250) == 1);
    CHECK(petcard::intimacyState(251) == 2 && petcard::intimacyState(750) == 2 && petcard::intimacyState(751) == 3);
    CHECK(petcard::intimacyState(900) == 3 && petcard::intimacyState(901) == 4 && petcard::intimacyState(1000) == 4);
}

// Its commands: the window's menu choices 0-3; the accessory off only with one worn
TEST(pet_card_commands_are_the_windows) {
    using C = petcard::Command;
    CHECK(petcard::kMenuMsg == 0x27 && petcard::kCloseButton == 0x87);
    CHECK(petcard::gameChoice(C::Feed) == 0 && petcard::gameChoice(C::Perform) == 1);
    CHECK(petcard::gameChoice(C::Unequip) == 2 && petcard::gameChoice(C::Egg) == 3);
    petcard::Info i;
    CHECK(!petcard::allowed(i, C::Unequip) && petcard::allowed(i, C::Feed));
    i.accessory = true;
    CHECK(petcard::allowed(i, C::Unequip));
}

// A card on the right: its title and ✕, the name and the level, the two bars, the four buttons; the mouse on each
TEST(pet_card_places_and_the_mouse) {
    const petcard::Layout L = petcard::layout(1920, 1080, 1.0f);
    CHECK(L.x > 960 && L.x + L.w <= 1920);
    CHECK(L.intimacyBar.y > L.hungerBar.y && L.buttons[0].y > L.intimacyBar.y);
    using P = petcard::Part;
    const petcard::Hit b = petcard::hitTest(L, mid(L.buttons[3]).first, mid(L.buttons[3]).second);
    CHECK(b.part == P::Button && b.index == 3);
    CHECK(petcard::hitTest(L, mid(L.close).first, mid(L.close).second).part == P::Close);
    CHECK(petcard::hitTest(L, -5, -5).part == P::None);
}

