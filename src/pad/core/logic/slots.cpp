#include "pad/core/logic/slots.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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
        if (listed(cfg.traps, id)) {
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
