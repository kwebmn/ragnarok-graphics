#include "pad/core/logic/targeting.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace uapad {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kAimConeDeg = 45.0;
constexpr double kAimPerCell = 2.0;

bool blacklisted(const std::unordered_map<uint32_t, uint32_t>& blacklistUntil, uint32_t id, uint32_t nowMs) {
    auto bl = blacklistUntil.find(id);
    return bl != blacklistUntil.end() && int32_t(nowMs - bl->second) < 0;
}

}  // namespace

std::vector<uint32_t> targetCandidates(const WorldState& world, uint32_t nowMs,
                                       const std::unordered_map<uint32_t, uint32_t>& blacklistUntil) {
    const Cell me = world.selfPos(nowMs);
    std::vector<std::tuple<int, int, uint32_t>> scored;  // squared straight-line distance, RO steps, id
    for (const auto& entry : world.actors()) {
        const uint32_t id = entry.first;
        if (!world.isMonster(entry.second)) continue;
        if (blacklisted(blacklistUntil, id, nowMs)) continue;
        auto pos = world.actorPos(id, nowMs);
        if (!pos) continue;
        const int steps = cellDistance(me, *pos);
        if (steps > kTargetReach) continue;  // the server shows monsters well off the screen (area_size 35)
        const int dx = pos->x - me.x;
        const int dy = pos->y - me.y;
        scored.emplace_back(dx * dx + dy * dy, steps, id);
    }
    std::sort(scored.begin(), scored.end());
    std::vector<uint32_t> ids;
    ids.reserve(scored.size());
    for (const auto& s : scored) ids.push_back(std::get<2>(s));
    return ids;
}

std::optional<double> aimScore(const WorldState& world, uint32_t nowMs, uint32_t id, double dirX, double dirY) {
    auto pos = world.actorPos(id, nowMs);
    const double len = std::hypot(dirX, dirY);
    if (!pos || len == 0) return std::nullopt;
    const Cell me = world.selfPos(nowMs);
    const int reach = cellDistance(me, *pos);
    if (reach == 0 || reach > kTargetReach) return std::nullopt;
    const double dx = pos->x - me.x;
    const double dy = pos->y - me.y;
    const double dist = std::hypot(dx, dy);
    const double cosOff = std::clamp((dx * dirX + dy * dirY) / (dist * len), -1.0, 1.0);
    const double off = std::acos(cosOff) * 180.0 / kPi;
    if (off > kAimConeDeg) return std::nullopt;
    return off + kAimPerCell * dist;
}

uint32_t pickInDirection(const WorldState& world, uint32_t nowMs, double dirX, double dirY,
                         const std::unordered_map<uint32_t, uint32_t>& blacklistUntil) {
    uint32_t best = 0;
    double bestScore = 0;
    for (const auto& entry : world.actors()) {
        const uint32_t id = entry.first;
        if (!world.isMonster(entry.second) || blacklisted(blacklistUntil, id, nowMs)) continue;
        auto score = aimScore(world, nowMs, id, dirX, dirY);
        if (!score) continue;
        if (best == 0 || *score < bestScore || (*score == bestScore && id < best)) {
            best = id;
            bestScore = *score;
        }
    }
    return best;
}

uint32_t pickNpc(const WorldState& world, uint32_t nowMs, double dirX, double dirY) {
    const Cell me = world.selfPos(nowMs);
    const bool aimed = dirX != 0 || dirY != 0;
    uint32_t best = 0;
    double bestScore = 0;
    for (const auto& entry : world.actors()) {
        const uint32_t id = entry.first;
        if (!world.isNpc(entry.second)) continue;
        auto pos = world.actorPos(id, nowMs);
        if (!pos || cellDistance(me, *pos) > kTargetReach) continue;
        std::optional<double> score = std::hypot(double(pos->x - me.x), double(pos->y - me.y));
        if (aimed) score = aimScore(world, nowMs, id, dirX, dirY);
        if (!score) continue;
        if (best == 0 || *score < bestScore || (*score == bestScore && id < best)) {
            best = id;
            bestScore = *score;
        }
    }
    return best;
}

bool shouldGiveUp(const WorldState& world, uint32_t target, uint32_t attackSinceMs, uint32_t nowMs, uint32_t giveUpMs) {
    uint32_t ref = attackSinceMs;
    if (auto hit = world.lastHitMs(target); hit && int32_t(*hit - ref) > 0) ref = *hit;
    return nowMs - ref >= giveUpMs;
}

}  // namespace uapad
