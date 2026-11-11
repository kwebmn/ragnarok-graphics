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
