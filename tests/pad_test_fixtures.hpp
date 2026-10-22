#pragma once
#include "pad/core/proto/c2s.hpp"
#include "pad/core/world/world.hpp"

// Worlds shared by the targeting, slot and controller tests.
namespace fx {
using namespace uapad;

constexpr uint32_t kSelf = 2000001;

// In game at (100, 100), nothing around.
inline WorldState world() {
    WorldState w;
    w.reset(kSelf);
    w.apply(EvAuthOk{Cell{100, 100}}, 0);
    return w;
}

// A standing monster (Poring by default).
inline void addMob(WorldState& w, uint32_t id, Cell at, uint16_t cls = 1002) {
    w.apply(EvActor{id, cls, 0, 200, at, at}, 0);
}

// Hotkey bar, learned skills and inventory used by the slot tests.
inline WorldState slotWorld() {
    WorldState w = world();
    EvHotkeys hk;
    hk.keys[0] = Hotkey{1, 5, 10};   // F1, X: Bash (enemy, range 1)
    hk.keys[1] = Hotkey{1, 28, 10};  // F2, Y: Heal (support), learned only up to lv 5
    hk.keys[2] = Hotkey{1, 12, 3};   // F3, B: Safety Wall (ground, in ground_self)
    hk.keys[3] = Hotkey{1, 21, 5};   // F4, RB: Thunderstorm (ground)
    hk.keys[4] = Hotkey{0, 501, 0};  // F5, RT: Red Potion
    hk.keys[5] = Hotkey{0, 502, 0};  // F6, LB: Orange Potion (not in inventory)
    hk.keys[6] = Hotkey{1, 19, 5};   // F7, D-pad up: Fire Bolt (enemy, range 9)
    //          [7]                     F8, D-pad right: empty
    hk.keys[8] = Hotkey{1, 117, 5};  // F9, D-pad down: Ankle Snare (ground, a trap, range 3)
    hk.keys[9] = Hotkey{1, 2, 1};    // second row: Sword Mastery (passive)
    w.apply(hk, 0);
    w.apply(EvSkills{{SkillInfo{5, 1, 10, 1}, SkillInfo{28, 16, 5, 9}, SkillInfo{12, 2, 10, 9},
                      SkillInfo{21, 2, 5, 9}, SkillInfo{19, 1, 10, 9}, SkillInfo{117, 2, 5, 3},
                      SkillInfo{2, 0, 10, 0}}},
            0);
    w.apply(EvInventory{{InvEntry{2, 501, 15, false}}}, 0);
    return w;
}

}  // namespace fx
