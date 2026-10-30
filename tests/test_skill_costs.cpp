// 3.0.11-beta.45: the server's SP prices by level (core/skill_costs, tools/skill_costs.py)
#include "core/skill_costs.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(skill_costs_by_level) {
    CHECK(skillcost::sp(5, 1) == 8 && skillcost::sp(5, 5) == 8 && skillcost::sp(5, 6) == 15);  // SM_BASH
    CHECK(skillcost::sp(5, 10) == 15);
    CHECK(skillcost::sp(6, 1) == 4 && skillcost::sp(6, 10) == 13);  // SM_PROVOKE
    CHECK(skillcost::sp(8, 1) == 10 && skillcost::sp(8, 10) == 10);  // SM_ENDURE: one value for all its levels
    CHECK(skillcost::sp(5, 0) == -1 && skillcost::sp(1, 1) == -1 && skillcost::sp(65000, 1) == -1);
}
