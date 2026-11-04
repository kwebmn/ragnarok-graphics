#include "core/skill_plan.hpp"

#include <algorithm>

#include "core/skill_window.hpp"

namespace uagfx::skillplan {
namespace {

skillwin::Skill* find(skillwin::Data& d, int id) {
    for (skillwin::Tab& t : d.tabs)
        for (skillwin::Skill& k : t.skills)
            if (k.id == id) return &k;
    return nullptr;
}

const skillwin::Skill* find(const skillwin::Data& d, int id) {
    for (const skillwin::Tab& t : d.tabs)
        for (const skillwin::Skill& k : t.skills)
            if (k.id == id) return &k;
    return nullptr;
}

bool plannable(const skillwin::Skill& k) {  // a quest, wedding or Soul Link skill never comes by points
    return k.lock != skillwin::Lock::Quest && k.lock != skillwin::Lock::Wedding && k.lock != skillwin::Lock::SoulLink;
}

bool reqsPlanned(const skillwin::Data& d, const skillwin::Skill& k) {
    for (const auto& [need, lv] : k.req)
        if (planLevel(d, need) < lv) return false;
    return true;
}

}  // namespace

int extra(const skillwin::Skill& k) { return std::max(0, k.target - k.level); }

int planLevel(const skillwin::Data& d, int id) {
    const skillwin::Skill* k = find(d, id);
    return k ? std::max(k->level, k->target) : 0;
}

int cost(const skillwin::Data& d) {
    int n = 0;
    for (const skillwin::Tab& t : d.tabs)
        for (const skillwin::Skill& k : t.skills) n += extra(k);
    return n;
}

int left(const skillwin::Data& d) { return d.points - cost(d); }

bool canPlan(const skillwin::Data& d, const skillwin::Skill& k) {
    const int now = std::max(k.level, k.target);
    return plannable(k) && left(d) > 0 && (k.most <= 0 || now < k.most) && reqsPlanned(d, k);
}

bool up(skillwin::Data& d, int id) {
    skillwin::Skill* k = find(d, id);
    if (!k || !canPlan(d, *k)) return false;
    k->target = std::max(k->level, k->target) + 1;
    return true;
}

bool down(skillwin::Data& d, int id) {
    skillwin::Skill* k = find(d, id);
    if (!k || extra(*k) <= 0) return false;
    k->target = k->target - 1 > k->level ? k->target - 1 : 0;
    for (bool again = true; again;) {  // what needed it planned no more
        again = false;
        for (skillwin::Tab& t : d.tabs)
            for (skillwin::Skill& s : t.skills)
                if (extra(s) > 0 && !reqsPlanned(d, s)) {
                    s.target = 0;
                    again = true;
                }
    }
    return true;
}

int nextToApply(const skillwin::Data& d) {
    if (d.points <= 0) return -1;
    for (const skillwin::Tab& t : d.tabs)
        for (const skillwin::Skill& k : t.skills)
            if (extra(k) > 0 && k.lock == skillwin::Lock::None && k.up && (k.most <= 0 || k.level < k.most))
                return k.id;
    return -1;
}

void put(skillwin::Data& d, const Targets& t) {
    for (skillwin::Tab& tab : d.tabs)
        for (skillwin::Skill& k : tab.skills) {
            const auto at = t.find(k.id);
            k.target = 0;
            if (at == t.end() || at->second <= k.level || !plannable(k)) continue;
            k.target = k.most > 0 ? std::min(at->second, k.most) : at->second;
        }
}

Targets taken(const skillwin::Data& d) {
    Targets out;
    for (const skillwin::Tab& t : d.tabs)
        for (const skillwin::Skill& k : t.skills)
            if (extra(k) > 0) out[k.id] = k.target;
    return out;
}

}  // namespace uagfx::skillplan
