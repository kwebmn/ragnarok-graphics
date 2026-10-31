// 3.0.11-beta.121 (the concept «Концепция окон и инвентаря», stage 5; the owner «приступай»): the controller carries a
// thing between the windows (core/pad_carry) - which places take what, the D-pad's way from place to place
#include <vector>

#include "core/equip_window.hpp"
#include "core/pad_carry.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

padcarry::Place place(padcarry::To to, int index, float x, float y) {
    padcarry::Place p;
    p.to = to;
    p.index = index;
    p.x = x;
    p.y = y;
    p.w = 40;
    p.h = 40;
    return p;
}

}  // namespace

TEST(pad_carry_what_takes_what) {
    padcarry::Thing sword;  // a weapon of the bag
    sword.from = padcarry::From::Bag;
    sword.location = 0x0002;
    sword.value = "1101";
    CHECK(padcarry::takes(sword, place(padcarry::To::Slot, equipwin::Weapon, 0, 0)));
    CHECK(!padcarry::takes(sword, place(padcarry::To::Slot, equipwin::Shoes, 0, 0)));
    CHECK(padcarry::takes(sword, place(padcarry::To::Cell, 3, 0, 0)));
    CHECK(padcarry::takes(sword, place(padcarry::To::Storage, 0, 0, 0)));
    CHECK(padcarry::takes(sword, place(padcarry::To::Trade, 0, 0, 0)));
    CHECK(!padcarry::takes(sword, place(padcarry::To::Bag, 0, 0, 0)));  // already in it
    padcarry::Thing potion = sword;  // a thing to use: not worn anywhere
    potion.location = 0;
    CHECK(!padcarry::takes(potion, place(padcarry::To::Slot, equipwin::Weapon, 0, 0)));
    padcarry::Thing skill;
    skill.from = padcarry::From::Skill;
    skill.value = "##AL_HEAL";
    CHECK(padcarry::takes(skill, place(padcarry::To::Cell, 0, 0, 0)));
    CHECK(!padcarry::takes(skill, place(padcarry::To::Storage, 0, 0, 0)));
    CHECK(!padcarry::takes(skill, place(padcarry::To::Slot, equipwin::Weapon, 0, 0)));
    padcarry::Thing stored;  // from the storage: into the bag alone
    stored.from = padcarry::From::Storage;
    stored.location = 0x0002;
    CHECK(padcarry::takes(stored, place(padcarry::To::Bag, 0, 0, 0)));
    CHECK(!padcarry::takes(stored, place(padcarry::To::Cell, 0, 0, 0)));
    CHECK(!padcarry::takes(stored, place(padcarry::To::Slot, equipwin::Weapon, 0, 0)));
    const std::vector<padcarry::Place> all{place(padcarry::To::Slot, equipwin::Shoes, 0, 0),
                                           place(padcarry::To::Cell, 0, 0, 0), place(padcarry::To::Bag, 0, 0, 0)};
    CHECK(padcarry::placesFor(sword, all).size() == 1 && padcarry::placesFor(stored, all).size() == 1);
}

TEST(pad_carry_the_dpads_way) {
    // a hotbar's row at the bottom, an equipment's column up left, a storage far right
    const std::vector<padcarry::Place> places{
        place(padcarry::To::Cell, 0, 500, 800),  place(padcarry::To::Cell, 1, 550, 800),
        place(padcarry::To::Cell, 2, 600, 800),  place(padcarry::To::Slot, 0, 300, 300),
        place(padcarry::To::Slot, 1, 300, 360),  place(padcarry::To::Storage, 0, 1200, 400)};
    CHECK(padcarry::nextPlace(places, 0, padcarry::Dir::Right) == 1);
    CHECK(padcarry::nextPlace(places, 1, padcarry::Dir::Left) == 0);
    CHECK(padcarry::nextPlace(places, 0, padcarry::Dir::Left) == 0);  // nothing that way: stays
    CHECK(padcarry::nextPlace(places, 0, padcarry::Dir::Up) == 4);    // the nearer slot, up and a little left
    CHECK(padcarry::nextPlace(places, 4, padcarry::Dir::Up) == 3);
    CHECK(padcarry::nextPlace(places, 4, padcarry::Dir::Right) == 5);  // across into the storage
    CHECK(padcarry::nextPlace(places, 3, padcarry::Dir::Down) == 4);
    CHECK(padcarry::nearestPlace(places, 560, 790) == 1);
    CHECK(padcarry::nearestPlace({}, 0, 0) == -1);
}
