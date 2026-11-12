#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/equip_window.hpp"

namespace uagfx::equipsets {

// 3.0.18-beta.154 (the owner: ideas from the later clients - their Equip Switch): «Спорядження»'s
// sets - what is worn kept (each slot's thing by its number, refine and cards: the bag's places change), put on again
// by one press: what is worn already left, each other found in the bag (the same refine and cards first, else the
// same number and refine) and worn at its slot (the game mode's 0x14 / 100, one a frame - win/equip_window)
struct Thing {
    int slot = 0, id = 0, refine = 0;
    int cards[4] = {};
};
struct Set {
    std::vector<Thing> things;
    std::string text() const;  // «slot id refine c1 c2 c3 c4» a line
    static Set parse(const std::string& text);
};
Set capture(const equipwin::Worn now[equipwin::kSlots]);

struct Owned {  // a thing in the bag, not worn
    int index = 0, id = 0, refine = 0;
    int cards[4] = {};
    bool worn = false;
};
struct Step {
    int index = 0;  // the bag's place
    uint32_t at = 0;  // where (the slot's bit)
    int slot = 0;
    bool off = false;  // 3.0.18-beta.220: taken off (the slot's thing), not worn
};
std::vector<Step> plan(const Set& set, const std::vector<Owned>& bag, const equipwin::Worn now[equipwin::kSlots]);
constexpr int kSets = 3;

// 3.0.18-beta.219 (the owner: «так и не понял как кнопкой 1 2 3 пользоваться, и оно как-то не функциональное» - a
// click on an empty set did nothing, keeping one was the right button's alone): a set's click says itself - an empty
// one keeps what is worn, a kept one puts itself on, one all worn already says so
enum class Click : uint8_t { Keep, PutOn, AlreadyOn };
bool wornNow(const Set& set, const equipwin::Worn now[equipwin::kSlots]);  // all of it worn now (an empty one: no)
Click clickOf(const Set& set, const equipwin::Worn now[equipwin::kSlots]);
// The set's things neither worn nor in the bag (sold, stored, dropped) - told after it is put on
std::vector<Thing> missing(const Set& set, const std::vector<Owned>& bag, const equipwin::Worn now[equipwin::kSlots]);

}  // namespace uagfx::equipsets
