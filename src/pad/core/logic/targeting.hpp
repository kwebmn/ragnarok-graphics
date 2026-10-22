#pragma once
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "pad/core/world/world.hpp"

namespace uapad {

// How far (in RO steps) a target may be: the right stick looks no further, and a target left further
// behind is dropped.
constexpr int kTargetReach = 15;

// Monsters we may target — within kTargetReach — nearest first: by straight-line distance, then RO steps,
// then id.
// Ids in `blacklistUntil` are skipped until nowMs reaches their deadline.
std::vector<uint32_t> targetCandidates(const WorldState& world, uint32_t nowMs,
                                       const std::unordered_map<uint32_t, uint32_t>& blacklistUntil);

// How well monster `id` matches the map direction (dirX, dirY) (+y north, any length): degrees off the
// direction plus 2 per cell of distance, lower is better. Nothing when it is more than 45 degrees off,
// more than kTargetReach away, on our own cell or unknown.
std::optional<double> aimScore(const WorldState& world, uint32_t nowMs, uint32_t id, double dirX, double dirY);

// The monster the player points at: the lowest aimScore (ties by id) among monsters that are not
// blacklisted, or 0.
uint32_t pickInDirection(const WorldState& world, uint32_t nowMs, double dirX, double dirY,
                         const std::unordered_map<uint32_t, uint32_t>& blacklistUntil);

// 0.7: the NPC to talk to - within kTargetReach, the nearest by straight-line distance, or with a direction
// (+y north, not zero) the one it points at best (aimScore); 0 when there is none.
uint32_t pickNpc(const WorldState& world, uint32_t nowMs, double dirX = 0, double dirY = 0);

// True when nothing of ours has landed on `target` for giveUpMs, counting from the start of the
// attack or from our last hit, whichever is later.
bool shouldGiveUp(const WorldState& world, uint32_t target, uint32_t attackSinceMs, uint32_t nowMs, uint32_t giveUpMs);

}  // namespace uapad
