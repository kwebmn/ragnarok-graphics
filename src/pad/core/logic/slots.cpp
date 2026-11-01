#include "pad/core/logic/slots.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/skill_area.hpp"
#include "pad/core/proto/c2s.hpp"

namespace uapad {
namespace {

constexpr uint32_t kInfEnemy = 1;
constexpr uint32_t kInfGround = 2;
constexpr uint32_t kInfSelf = 4;
constexpr uint32_t kInfSupport = 16;

SlotDecision deny(std::string why) {
    SlotDecision d;
    d.reason = std::move(why);
    return d;
}

SlotDecision send(Bytes packet, uint32_t newTarget = 0) {
    SlotDecision d;
    d.kind = SlotDecision::Kind::Send;
    d.packet = std::move(packet);
    d.newTarget = newTarget;
    return d;
}

bool listed(const std::vector<uint16_t>& ids, uint16_t id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

// `sk` at `lv` on `actor` (or its cell): sent within the skill's reach, else a walk up to it first.
SlotDecision castAt(const SkillInfo& sk, uint16_t lv, const WorldState& world, uint32_t actor, bool ground,
                    uint32_t newTarget, uint32_t nowMs) {
    auto pos = world.actorPos(actor, nowMs);
    if (!pos) return deny("target is gone");
    const int reach = std::max(1, int(sk.range));
    SlotDecision d;
    d.newTarget = newTarget;
    if (cellDistance(world.selfPos(nowMs), *pos) <= reach) {
        d.kind = SlotDecision::Kind::Send;
        d.packet = ground ? buildSkillToPos(sk.id, lv, *pos) : buildSkillToId(sk.id, lv, actor);
    } else {
        d.kind = SlotDecision::Kind::Approach;
        d.actor = actor;
        d.ground = ground;
        d.skill = sk.id;
        d.lv = lv;
        d.range = reach;
    }
    return d;
}

namespace area = uagfx::skillarea;

// 3.0.11-beta.142 (the owner's concept «маг, прист и хант на геймпаде», answer 3): a wall between us and the monster -
// two cells from us along the server's own side toward it (a diagonal one: the 5-cell staircase), one when it is two
// away, on it when it is next to us; no monster - two cells along our last step
SlotDecision wallBetween(const SkillInfo& sk, uint16_t lv, const WorldState& world, uint32_t at, uint32_t adopt,
                         uint32_t nowMs) {
    const Cell me = world.selfPos(nowMs);
    std::optional<Cell> pos = at != 0 ? world.actorPos(at, nowMs) : std::nullopt;
    if (pos && *pos == me) pos.reset();
    Cell way{0, 0};
    int dist = 3;
    if (pos) {
        way = Cell{pos->x - me.x, pos->y - me.y};
        dist = std::max(std::abs(way.x), std::abs(way.y));
        if (dist <= 1) return send(buildSkillToPos(sk.id, lv, *pos), adopt);
    } else if (auto last = world.lastMove()) {
        way = *last;
    } else {
        return deny("no direction to put wall " + std::to_string(sk.id) + " in");
    }
    const area::Cell one = area::step(area::dirOf(way.x, way.y));
    const int n = dist >= 3 ? 2 : 1;
    return send(buildSkillToPos(sk.id, lv, Cell{me.x + one.x * n, me.y + one.y * n}), pos ? adopt : 0);
}

// 3.0.11-beta.142 (answer 5): an area where it covers the most monsters - the cell of the monster within the skill's
// reach whose area (its radius about it) holds the most of them; a tie: the one nearest the target (us without one);
// none within reach: nothing
std::optional<Cell> crowdCell(uint16_t skill, int reach, const WorldState& world, uint32_t target, uint32_t nowMs) {
    const int r = area::crowdRadius(skill);
    const Cell me = world.selfPos(nowMs);
    std::vector<Cell> mobs;
    for (const auto& [id, a] : world.actors())
        if (world.isMonster(a))
            if (auto p = world.actorPos(id, nowMs)) mobs.push_back(*p);
    const std::optional<Cell> near = target != 0 ? world.actorPos(target, nowMs) : std::nullopt;
    std::optional<Cell> best;
    int bestCount = 0, bestTie = 0;
    for (const Cell& c : mobs) {
        if (cellDistance(me, c) > reach) continue;
        int count = 0;
        for (const Cell& m : mobs) count += std::max(std::abs(m.x - c.x), std::abs(m.y - c.y)) <= r ? 1 : 0;
        const int tie = cellDistance(near ? *near : me, c);
        if (!best || count > bestCount || (count == bestCount && tie < bestTie)) {
            best = c;
            bestCount = count;
            bestTie = tie;
        }
    }
    return best;
}

// What a learned skill does right now (see decideSlot).
SlotDecision decideSkill(const SkillInfo& sk, uint16_t lv, const WorldState& world, uint32_t target,
                         uint32_t nearest, const Config& cfg, uint32_t nowMs) {
    const uint16_t id = sk.id;
    const Cell me = world.selfPos(nowMs);
    const int reach = std::max(1, int(sk.range));

    auto castOn = [&](uint32_t actor, bool ground, uint32_t newTarget) {
        return castAt(sk, lv, world, actor, ground, newTarget, nowMs);
    };

    if (sk.inf & kInfGround) {
        if (listed(cfg.groundSelf, id)) return send(buildSkillToPos(id, lv, me));
        const uint32_t at = target != 0 ? target : nearest;
        const uint32_t adopt = target != 0 ? 0 : nearest;
        const area::Kind kind = area::kindOf(id);
        if (kind == area::Kind::Wall) return wallBetween(sk, lv, world, at, adopt, nowMs);
        if (kind == area::Kind::Crowd && at != 0)  // a monster offered: where the most of them are
            if (auto c = crowdCell(id, reach, world, at, nowMs)) return send(buildSkillToPos(id, lv, *c), adopt);
        if (listed(cfg.traps, id) || kind == area::Kind::Ahead) {  // 3.0.11-beta.142: Fire Pillar as a trap
            // The server refuses a trap with any character within 1 cell, us included: put it two steps
            // towards the monster (or along our last step), always within the trap range of 3.
            std::optional<Cell> dir;
            uint32_t from = 0;
            if (at != 0) {
                if (auto pos = world.actorPos(at, nowMs); pos && *pos != me) {
                    dir = Cell{pos->x - me.x, pos->y - me.y};
                    from = adopt;
                }
            }
            if (!dir) dir = world.lastMove();
            if (!dir) return deny("no direction to put trap " + std::to_string(id) + " in");
            return send(buildSkillToPos(id, lv, stepsToward(me, *dir, std::min(2, reach))), from);
        }
        if (at == 0) return send(buildSkillToPos(id, lv, me));
        return castOn(at, true, adopt);
    }
    if (sk.inf & kInfEnemy) {
        if (target != 0) return castOn(target, false, 0);
        if (nearest != 0) return castOn(nearest, false, nearest);
        return deny("no monster for skill " + std::to_string(id));
    }
    if (sk.inf & (kInfSelf | kInfSupport)) return send(buildSkillToId(id, lv, world.selfId()));
    return deny("skill " + std::to_string(id) + " is passive");
}

}  // namespace

bool choosesWhom(const SkillInfo& sk) { return (sk.inf & kInfSupport) && !(sk.inf & (kInfEnemy | kInfGround)); }

SlotDecision decideSupport(const Hotkey& key, const WorldState& world, uint32_t actor, uint32_t nowMs) {
    auto sk = world.skill(uint16_t(key.id));
    if (key.type == 0 || !sk || sk->lv == 0) return deny("skill " + std::to_string(key.id) + " is not learned");
    const uint16_t lv = key.lv == 0 ? sk->lv : std::min(key.lv, sk->lv);
    SlotDecision d = actor == world.selfId() ? send(buildSkillToId(sk->id, lv, actor))
                                             : castAt(*sk, lv, world, actor, false, 0, nowMs);
    if (d.kind != SlotDecision::Kind::Deny) {
        d.skill = sk->id;
        d.lv = lv;
    }
    return d;
}

SlotDecision decideSlot(const Hotkey& key, const WorldState& world, uint32_t target, uint32_t nearest,
                        const Config& cfg, uint32_t nowMs) {
    if (key.id == 0) return deny("empty slot");
    if (key.type == 0) {
        auto item = world.findUsable(uint16_t(key.id));
        if (!item) return deny("item " + std::to_string(key.id) + " is not in the inventory");
        return send(buildUseItem(item->index, world.selfId()));
    }
    auto sk = world.skill(uint16_t(key.id));
    if (!sk || sk->lv == 0) return deny("skill " + std::to_string(key.id) + " is not learned");
    const uint16_t lv = key.lv == 0 ? sk->lv : std::min(key.lv, sk->lv);
    SlotDecision d = decideSkill(*sk, lv, world, target, nearest, cfg, nowMs);
    if (d.kind != SlotDecision::Kind::Deny) {
        d.skill = sk->id;
        d.lv = lv;
    }
    return d;
}

}  // namespace uapad
