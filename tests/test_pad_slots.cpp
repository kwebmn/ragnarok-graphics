#include "pad/core/logic/slots.hpp"
#include "pad_test_fixtures.hpp"
#include "test.hpp"

using namespace uapad;

static SlotDecision slot(const WorldState& w, int index, uint32_t target, uint32_t nearest,
                         const Config& cfg = Config{}) {
    return decideSlot(w.hotkey(index), w, target, nearest, cfg, 0);
}

// The cell of a 0x0113 request; (-1, -1) for anything else.
static Cell cellOf(const SlotDecision& d) {
    if (d.packet.size() != 22 || rd16(d.packet.data(), 0) != 0x0113) return Cell{-1, -1};
    return Cell{int(rd16(d.packet.data(), 12)), int(rd16(d.packet.data(), 20))};
}

TEST(slots_item_uses_inventory_index) {
    WorldState w = fx::slotWorld();
    SlotDecision d = slot(w, 4, 0, 0);
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK_EQ(rd16(d.packet.data(), 0), 0x009f);
    CHECK_EQ(rd16(d.packet.data(), 4), 2);
    CHECK_EQ(rd32(d.packet.data(), 10), fx::kSelf);
}

TEST(slots_missing_item_empty_unlearned_passive_are_denied) {
    WorldState w = fx::slotWorld();
    CHECK(slot(w, 5, 0, 0).kind == SlotDecision::Kind::Deny);   // Orange Potion not in inventory
    CHECK(slot(w, 7, 0, 0).kind == SlotDecision::Kind::Deny);   // empty slot
    CHECK(slot(w, 9, 0, 0).kind == SlotDecision::Kind::Deny);   // passive skill
    CHECK(!slot(w, 9, 0, 0).reason.empty());
    w.setHotkey(8, Hotkey{1, 46, 5});                           // Double Strafe: not learned
    CHECK(slot(w, 8, 0, 0).kind == SlotDecision::Kind::Deny);
}

TEST(slots_support_skill_on_self_with_clamped_level) {
    WorldState w = fx::slotWorld();
    SlotDecision d = slot(w, 1, 0, 0);  // Heal lv 10 on the bar, learned lv 5
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK_EQ(rd16(d.packet.data(), 0), 0x0072);
    CHECK_EQ(rd16(d.packet.data(), 6), 5);
    CHECK_EQ(rd16(d.packet.data(), 10), 28);
    CHECK_EQ(rd32(d.packet.data(), 21), fx::kSelf);
    CHECK_EQ(d.newTarget, 0u);
}

TEST(slots_level_zero_on_bar_means_learned_level) {
    WorldState w = fx::slotWorld();
    w.setHotkey(8, Hotkey{1, 28, 0});
    SlotDecision d = slot(w, 8, 0, 0);
    CHECK_EQ(rd16(d.packet.data(), 6), 5);
}

TEST(slots_enemy_skill_on_target_in_range) {
    WorldState w = fx::slotWorld();
    fx::addMob(w, 110000001, Cell{105, 100});
    SlotDecision d = slot(w, 6, 110000001, 0);  // Fire Bolt range 9, distance 5
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK_EQ(rd16(d.packet.data(), 0), 0x0072);
    CHECK_EQ(rd16(d.packet.data(), 6), 5);
    CHECK_EQ(rd16(d.packet.data(), 10), 19);
    CHECK_EQ(rd32(d.packet.data(), 21), 110000001u);
    CHECK_EQ(d.newTarget, 0u);
}

TEST(slots_enemy_skill_without_target_takes_nearest) {
    WorldState w = fx::slotWorld();
    fx::addMob(w, 110000001, Cell{105, 100});
    SlotDecision d = slot(w, 6, 0, 110000001);
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK_EQ(d.newTarget, 110000001u);
    CHECK(slot(w, 6, 0, 0).kind == SlotDecision::Kind::Deny);  // no monster at all
}

TEST(slots_enemy_skill_out_of_range_approaches) {
    WorldState w = fx::slotWorld();
    fx::addMob(w, 110000001, Cell{105, 100});
    SlotDecision d = slot(w, 0, 110000001, 0);  // Bash range 1, distance 5
    CHECK(d.kind == SlotDecision::Kind::Approach);
    CHECK_EQ(d.actor, 110000001u);
    CHECK(!d.ground);
    CHECK_EQ(d.skill, 5);
    CHECK_EQ(d.lv, 10);
    CHECK_EQ(d.range, 1);
}

TEST(slots_ground_skill_cells) {
    WorldState w = fx::slotWorld();
    SlotDecision d = slot(w, 3, 0, 0);  // Thunderstorm without a target: under ourselves
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK_EQ(rd16(d.packet.data(), 0), 0x0113);
    CHECK_EQ(rd16(d.packet.data(), 9), 21);
    CHECK_EQ(rd16(d.packet.data(), 12), 100);
    CHECK_EQ(rd16(d.packet.data(), 20), 100);
    fx::addMob(w, 110000001, Cell{104, 102});
    d = slot(w, 3, 110000001, 0);  // with a target: on its cell
    CHECK_EQ(rd16(d.packet.data(), 12), 104);
    CHECK_EQ(rd16(d.packet.data(), 20), 102);
    d = slot(w, 2, 110000001, 0);  // Safety Wall is in ground_self: under ourselves anyway
    CHECK_EQ(rd16(d.packet.data(), 9), 12);
    CHECK_EQ(rd16(d.packet.data(), 12), 100);
    CHECK_EQ(rd16(d.packet.data(), 20), 100);
}

TEST(slots_ground_skill_out_of_range_approaches) {
    WorldState w = fx::slotWorld();
    fx::addMob(w, 110000001, Cell{112, 100});  // distance 12, Thunderstorm range 9
    SlotDecision d = slot(w, 3, 110000001, 0);
    CHECK(d.kind == SlotDecision::Kind::Approach);
    CHECK(d.ground);
    CHECK_EQ(d.range, 9);
}

TEST(slots_ground_skill_takes_nearest) {
    WorldState w = fx::slotWorld();
    fx::addMob(w, 110000001, Cell{104, 102});
    SlotDecision d = slot(w, 3, 0, 110000001);  // Thunderstorm without a target: on the nearest monster
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK(cellOf(d) == (Cell{104, 102}));
    CHECK_EQ(d.newTarget, 110000001u);  // which becomes the target
    d = slot(w, 3, 0, 0);               // no monsters at all: under ourselves
    CHECK(cellOf(d) == (Cell{100, 100}));
    CHECK_EQ(d.newTarget, 0u);
}

TEST(slots_trap_two_steps_toward_target) {
    WorldState w = fx::slotWorld();
    fx::addMob(w, 110000001, Cell{105, 103});
    SlotDecision d = slot(w, 8, 110000001, 0);  // Ankle Snare
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK_EQ(rd16(d.packet.data(), 0), 0x0113);
    CHECK_EQ(rd16(d.packet.data(), 9), 117);
    CHECK(cellOf(d) == (Cell{102, 101}));  // two steps towards (105, 103), never on a monster or on us
    CHECK_EQ(d.newTarget, 0u);
    fx::addMob(w, 110000002, Cell{100, 92});
    d = slot(w, 8, 0, 110000002);  // no target: towards the nearest monster, which becomes the target
    CHECK(cellOf(d) == (Cell{100, 98}));
    CHECK_EQ(d.newTarget, 110000002u);
    fx::addMob(w, 110000003, Cell{140, 100});
    d = slot(w, 8, 110000003, 0);  // far away: no walking, the trap still goes next to us
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK(cellOf(d) == (Cell{102, 100}));
    Config noTraps;
    noTraps.traps.clear();  // not listed as a trap: an ordinary ground skill aimed at the target's cell
    d = slot(w, 8, 110000001, 0, noTraps);
    CHECK(d.kind == SlotDecision::Kind::Approach);  // 5 cells away, range 3
    CHECK(d.ground);
    CHECK_EQ(d.actor, 110000001u);
}

TEST(slots_trap_without_monsters_uses_last_step) {
    WorldState w = fx::slotWorld();
    SlotDecision d = slot(w, 8, 0, 0);  // no monster and no step on this map yet: nowhere to put it
    CHECK(d.kind == SlotDecision::Kind::Deny);
    CHECK(!d.reason.empty());
    w.apply(EvSelfWalk{Cell{100, 100}, Cell{97, 104}}, 0);  // a step to the north-west
    d = slot(w, 8, 0, 0);
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK(cellOf(d) == (Cell{98, 102}));
    CHECK_EQ(d.newTarget, 0u);
}

// 3.0.11-beta.142 (the owner's concept «маг, прист и хант на геймпаде», answers 3 and 5): a mage's bar - Fire Wall,
// Storm Gust, Fire Pillar - on the second row
static WorldState mageWorld() {
    WorldState w = fx::slotWorld();
    w.setHotkey(10, Hotkey{1, 18, 10});  // Fire Wall
    w.setHotkey(11, Hotkey{1, 89, 10});  // Storm Gust
    w.setHotkey(12, Hotkey{1, 80, 10});  // Fire Pillar
    w.apply(EvSkills{{SkillInfo{5, 1, 10, 1}, SkillInfo{28, 16, 5, 9}, SkillInfo{12, 2, 10, 9},
                      SkillInfo{21, 2, 5, 9}, SkillInfo{19, 1, 10, 9}, SkillInfo{117, 2, 5, 3},
                      SkillInfo{2, 0, 10, 0}, SkillInfo{18, 2, 10, 9}, SkillInfo{89, 2, 10, 9},
                      SkillInfo{80, 2, 10, 9}}},
            0);
    return w;
}

// Fire Wall by a press: two cells from us toward the monster, along the server's own side (a diagonal one - the
// 5-cell staircase); one cell when it is two away; on it when it is next to us; no monster - two cells along our last
// step; nothing known - nowhere
TEST(slots_fire_wall_between_us_and_the_monster) {
    WorldState w = mageWorld();
    fx::addMob(w, 110000001, Cell{106, 100});
    CHECK(cellOf(slot(w, 10, 110000001, 0)) == (Cell{102, 100}));
    fx::addMob(w, 110000002, Cell{102, 100});
    CHECK(cellOf(slot(w, 10, 110000002, 0)) == (Cell{101, 100}));
    fx::addMob(w, 110000003, Cell{101, 101});
    CHECK(cellOf(slot(w, 10, 110000003, 0)) == (Cell{101, 101}));
    fx::addMob(w, 110000004, Cell{105, 103});  // up-right by the server's rule: the diagonal
    CHECK(cellOf(slot(w, 10, 110000004, 0)) == (Cell{102, 102}));
    fx::addMob(w, 110000005, Cell{112, 100});  // beyond the reach: the wall still goes in front of us
    SlotDecision far = slot(w, 10, 110000005, 0);
    CHECK(far.kind == SlotDecision::Kind::Send && cellOf(far) == (Cell{102, 100}));
    SlotDecision adopt = slot(w, 10, 0, 110000001);  // no target: toward the nearest, which becomes the target
    CHECK(cellOf(adopt) == (Cell{102, 100}) && adopt.newTarget == 110000001u);
    WorldState empty = mageWorld();
    CHECK(slot(empty, 10, 0, 0).kind == SlotDecision::Kind::Deny);
    empty.apply(EvSelfWalk{Cell{100, 100}, Cell{97, 104}}, 0);  // a step to the north-west
    CHECK(cellOf(slot(empty, 10, 0, 0)) == (Cell{98, 102}));
}

// Storm Gust by a press: on the monster whose cell's area (radius 5) holds the most of them within its reach, not on
// the lone target; a tie - the one nearest the target; none within reach - toward the target as before
TEST(slots_crowd_skill_where_the_most_monsters_are) {
    WorldState w = mageWorld();
    fx::addMob(w, 110000001, Cell{104, 100});  // the target, alone
    fx::addMob(w, 110000002, Cell{100, 106});
    fx::addMob(w, 110000003, Cell{101, 107});
    fx::addMob(w, 110000004, Cell{99, 107});
    SlotDecision d = slot(w, 11, 110000001, 0);
    CHECK(d.kind == SlotDecision::Kind::Send);
    CHECK_EQ(rd16(d.packet.data(), 9), 89);
    const Cell at = cellOf(d);
    CHECK(at == (Cell{100, 106}) || at == (Cell{101, 107}) || at == (Cell{99, 107}));
    WorldState one = mageWorld();
    fx::addMob(one, 110000001, Cell{104, 100});
    fx::addMob(one, 110000002, Cell{94, 100});
    CHECK(cellOf(slot(one, 11, 110000001, 0)) == (Cell{104, 100}));  // a tie: the target's own
    WorldState far = mageWorld();
    fx::addMob(far, 110000001, Cell{112, 100});
    CHECK(slot(far, 11, 110000001, 0).kind == SlotDecision::Kind::Approach);
}

// Fire Pillar by a press: like a trap, two steps toward the monster
TEST(slots_fire_pillar_ahead_like_a_trap) {
    WorldState w = mageWorld();
    fx::addMob(w, 110000001, Cell{105, 103});
    CHECK(cellOf(slot(w, 12, 110000001, 0)) == (Cell{102, 101}));
}
