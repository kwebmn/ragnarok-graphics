#include "core/skill_costs.hpp"

#include <algorithm>
#include <iterator>

namespace uagfx::skillcost {
namespace {

#include "core/skill_costs.inc"

}  // namespace

int sp(int id, int level) {
    if (level < 1 || level > 100) return -1;
    const auto it = std::lower_bound(std::begin(kSkillCosts), std::end(kSkillCosts), id,
                                     [](const SkillCost& c, int want) { return int(c.id) < want; });
    if (it == std::end(kSkillCosts) || int(it->id) != id || !it->levels) return -1;
    return int(it->sp[std::min(level, int(it->levels)) - 1]);
}

}  // namespace uagfx::skillcost
