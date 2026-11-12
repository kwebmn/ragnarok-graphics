#include <string>
#include <vector>

#include "core/equip_sets.hpp"
#include "test.hpp"

using namespace uagfx;
namespace es = uagfx::equipsets;
namespace ew = uagfx::equipwin;

namespace {
ew::Worn worn(int index, int id, int refine = 0, int card = 0) {
    ew::Worn w;
    w.index = index, w.id = id, w.refine = refine, w.cards[0] = card;
    return w;
}
}  // namespace

// 3.0.18-beta.154 (the owner: ideas from the later clients - their Equip Switch): a set of what is worn kept (each
// slot's thing by its number, refine and cards - the bag's places change), put on again: what is worn already left,
// each other thing found in the bag (the same refine and cards first) and worn where it was
TEST(equip_sets_kept_and_put_on) {
    ew::Worn now[ew::kSlots];
    now[ew::Weapon] = worn(5, 1201, 7, 4001);
    now[ew::Armor] = worn(6, 2301, 4);
    const es::Set set = es::capture(now);
    CHECK_EQ(set.things.size(), size_t(2));
    // later: another armour worn, the knife in the bag
    ew::Worn later[ew::kSlots];
    later[ew::Armor] = worn(6, 2301, 4);
    later[ew::Weapon] = worn(9, 1101, 0);
    std::vector<es::Owned> bag;
    bag.push_back(es::Owned{11, 1201, 0, {0, 0, 0, 0}, false});  // the same knife, no refine: not the one
    bag.push_back(es::Owned{12, 1201, 7, {4001, 0, 0, 0}, false});
    const std::vector<es::Step> steps = es::plan(set, bag, later);
    CHECK_EQ(steps.size(), size_t(1));  // the armour worn already
    CHECK(steps.size() == 1 && steps[0].index == 12 && steps[0].at == ew::kBits[ew::Weapon]);
    // the exact one gone: the same number and refine still serves
    bag.erase(bag.begin() + 1);
    bag.push_back(es::Owned{13, 1201, 7, {0, 0, 0, 0}, false});
    const std::vector<es::Step> near = es::plan(set, bag, later);
    CHECK(near.size() == 1 && near[0].index == 13);
    // kept as words, read back
    const es::Set back = es::Set::parse(set.text());
    CHECK_EQ(back.things.size(), size_t(2));
    CHECK(es::plan(back, bag, later).size() == 1);
    CHECK(es::Set::parse("").things.empty());
}

// 3.0.18-beta.219 (the owner: «так и не понял как кнопкой 1 2 3 пользоваться, и оно как-то не функциональное» - a
// click on an empty set did nothing, keeping one was the right button's alone): a set's click says itself
TEST(equip_sets_click_keeps_puts_on_or_says_worn) {
    ew::Worn now[ew::kSlots];
    now[ew::Weapon] = worn(5, 1201, 7, 4001);
    now[ew::Armor] = worn(6, 2301, 4);
    const es::Set empty;
    CHECK(es::clickOf(empty, now) == es::Click::Keep);  // empty: a click keeps what is worn
    CHECK(!es::wornNow(empty, now));
    const es::Set kept = es::capture(now);
    CHECK(es::wornNow(kept, now));
    CHECK(es::clickOf(kept, now) == es::Click::AlreadyOn);  // all of it worn: nothing to do, said so
    ew::Worn later[ew::kSlots];
    later[ew::Armor] = worn(6, 2301, 4);
    later[ew::Weapon] = worn(9, 1101, 0);
    CHECK(!es::wornNow(kept, later));
    CHECK(es::clickOf(kept, later) == es::Click::PutOn);
    ew::Worn refined[ew::kSlots];  // the same knife at another refine: not this set's
    refined[ew::Armor] = worn(6, 2301, 4);
    refined[ew::Weapon] = worn(5, 1201, 6, 4001);
    CHECK(es::clickOf(kept, refined) == es::Click::PutOn);
}

// What a set holds but neither is worn nor in the bag (sold, stored) - told after it is put on
TEST(equip_sets_missing_things_told) {
    ew::Worn now[ew::kSlots];
    now[ew::Weapon] = worn(5, 1201, 7, 4001);
    now[ew::Armor] = worn(6, 2301, 4);
    now[ew::Shoes] = worn(7, 2401, 0);
    const es::Set kept = es::capture(now);
    ew::Worn later[ew::kSlots];
    later[ew::Armor] = worn(6, 2301, 4);  // worn: not missing
    std::vector<es::Owned> bag;
    bag.push_back(es::Owned{12, 1201, 7, {0, 0, 0, 0}, false});  // the knife (its card gone): in the bag, not missing
    const std::vector<es::Thing> gone = es::missing(kept, bag, later);
    CHECK_EQ(gone.size(), size_t(1));
    CHECK(gone.size() == 1 && gone[0].id == 2401 && gone[0].slot == ew::Shoes);  // the shoes: nowhere
    bag.push_back(es::Owned{14, 2401, 0, {0, 0, 0, 0}, false});
    CHECK(es::missing(kept, bag, later).empty());
}

// 3.0.18-beta.220 (the owner on «should a set take off what it has not?»: «снимать»): a set put on takes off first what
// is worn where it has nothing - a two-hander's other half left alone; «worn» means just that set
TEST(equip_sets_take_off_what_the_set_has_not) {
    ew::Worn saved[ew::kSlots];
    saved[ew::Weapon] = worn(5, 1201, 7);
    const es::Set set = es::capture(saved);  // the knife alone
    ew::Worn now[ew::kSlots];
    now[ew::Weapon] = worn(5, 1201, 7);
    now[ew::Garment] = worn(8, 2501, 0);  // a manteau put on since
    CHECK(!es::wornNow(set, now));  // the manteau too: not just the set
    CHECK(es::clickOf(set, now) == es::Click::PutOn);
    const std::vector<es::Step> steps = es::plan(set, {}, now);
    CHECK_EQ(steps.size(), size_t(1));
    CHECK(steps.size() == 1 && steps[0].off && steps[0].slot == ew::Garment);
    ew::Worn bow[ew::kSlots];  // a two-handed bow: its other half at the shield's slot
    bow[ew::Weapon] = worn(9, 1705, 0);
    bow[ew::Shield] = worn(9, 1705, 0);
    bow[ew::Shield].spill = true;
    const es::Set bowSet = es::capture(bow);
    CHECK(es::plan(bowSet, {}, bow).empty());  // its half is not taken off
    CHECK(es::wornNow(bowSet, bow));
    ew::Worn arrows[ew::kSlots];  // the bow and arrows on, the set the knife alone: both taken off, the knife worn
    arrows[ew::Weapon] = bow[ew::Weapon];
    arrows[ew::Shield] = bow[ew::Shield];
    arrows[ew::Ammo] = worn(11, 1750, 0);
    std::vector<es::Owned> bag;
    bag.push_back(es::Owned{5, 1201, 7, {0, 0, 0, 0}, false});
    const std::vector<es::Step> swap = es::plan(set, bag, arrows);
    CHECK_EQ(swap.size(), size_t(2));
    CHECK(swap.size() == 2 && swap[0].off && swap[0].slot == ew::Ammo);  // the take-offs first
    CHECK(swap.size() == 2 && !swap[1].off && swap[1].index == 5 && swap[1].slot == ew::Weapon);  // the bow replaced
}
