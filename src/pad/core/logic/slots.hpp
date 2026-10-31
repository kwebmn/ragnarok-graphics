#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pad/core/bytes.hpp"
#include "pad/core/config/config.hpp"
#include "pad/core/world/world.hpp"

namespace uapad {

struct SlotDecision {
    enum class Kind { Deny, Send, Approach };
    Kind kind = Kind::Deny;
    Bytes packet;            // Send: the request to send now
    uint32_t newTarget = 0;  // != 0: the controller should adopt this monster as its target
    uint32_t actor = 0;      // Approach: walk until within `range` of this actor, then cast
    bool ground = false;     // Approach: cast on the actor's cell instead of on the actor
    uint16_t skill = 0;      // Send or Approach of a skill: which skill and level
    uint16_t lv = 0;
    int range = 1;
    std::string reason;      // Deny: why, for the log
};

// What a hotkey slot does right now. `target`: the current live target or 0; `nearest`: the monster to
// use when there is no target (it becomes the target), or 0. `cfg` lists the skills cast under ourselves
// (ground_self) and the traps, which go two steps towards the target.
SlotDecision decideSlot(const Hotkey& key, const WorldState& world, uint32_t target, uint32_t nearest,
                        const Config& cfg, uint32_t nowMs);

// 3.0.11-beta.85 (the concept's «Выбор цели навыка»): a skill that may go on another player - support (inf 16), on
// neither an enemy nor the ground - asks whom while party members are in sight
bool choosesWhom(const SkillInfo& sk);
// That skill (`key`'s) on `actor`, a party member or the character: at once within its range, else a walk up
SlotDecision decideSupport(const Hotkey& key, const WorldState& world, uint32_t actor, uint32_t nowMs);

}  // namespace uapad
