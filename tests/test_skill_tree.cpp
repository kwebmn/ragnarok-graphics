// 3.0.11-beta.96 («Навички», the concept's skills window): the server's skill tree (core/skill_tree, generated from
// db/skill_tree.txt) - a skill's most levels for a job, the jobs a character's tabs come from, the tab a skill sits
// on (the job it first comes with) and what it needs
#include <vector>

#include "core/skill_tree.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(skill_tree_most_levels) {
    CHECK_EQ(skilltree::maxLevel(1, 5), 10);      // a Swordman's Bash
    CHECK_EQ(skilltree::maxLevel(4008, 355), 5);  // a Lord Knight's Aura Blade
    CHECK_EQ(skilltree::maxLevel(4008, 5), 10);   // and the Bash it kept
    CHECK_EQ(skilltree::maxLevel(2, 5), 0);       // a Magician has none
}

TEST(skill_tree_a_characters_jobs_and_tabs) {
    CHECK(skilltree::chain(4008) == std::vector<int>({0, 1, 7, 4008}));  // Novice, Swordman, Knight, Lord Knight
    CHECK(skilltree::chain(9) == std::vector<int>({0, 2, 9}));
    CHECK(skilltree::chain(0) == std::vector<int>({0}));
    CHECK(skilltree::chain(31000) == std::vector<int>({31000}));  // not known: itself
    const std::vector<int> lk = skilltree::chain(4008);
    CHECK_EQ(skilltree::homeJob(lk, 142), 0);     // First Aid - the Novice's
    CHECK_EQ(skilltree::homeJob(lk, 5), 1);       // Bash - the Swordman's
    CHECK_EQ(skilltree::homeJob(lk, 355), 4008);  // Aura Blade - the Lord Knight's
    CHECK_EQ(skilltree::homeJob(lk, 13), -1);     // Soul Strike: none of them
    CHECK(skilltree::order(1, 5) < skilltree::order(1, 6));  // the server's order: Bash before Provoke
}

TEST(skill_tree_what_a_skill_needs) {
    const std::vector<std::pair<int, int>> magnum = skilltree::needs(1, 7);  // Magnum Break: Bash 5
    CHECK(magnum.size() == 1 && magnum[0].first == 5 && magnum[0].second == 5);
    CHECK(skilltree::needs(1, 5).empty());
    CHECK(skilltree::needs(4008, 359).empty());  // Frenzy's «50, 0» is no need
}
