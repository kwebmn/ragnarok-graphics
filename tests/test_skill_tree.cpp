// 3.0.11-beta.96 («Навички», the concept's skills window): the server's skill tree (core/skill_tree, generated from
// db/skill_tree.txt) - a skill's most levels for a job, the jobs a character's tabs come from, the tab a skill sits
// on (the job it first comes with) and what it needs
#include <algorithm>
#include <string>
#include <vector>

#include "core/skill_info.hpp"
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

// 3.0.11-beta.223: every skill of a job's tree in the server's order, and what the server never sends by the
// requirements (core/skill_info from db/skill_db.txt)
TEST(skill_tree_a_jobs_skills_and_how_they_are_learned) {
    const std::vector<int> knight = skilltree::skills(7);
    CHECK(knight.size() == 26 && knight.front() == 1);
    CHECK(std::find(knight.begin(), knight.end(), 62) != knight.end());  // Bowling Bash
    CHECK(skilltree::skills(31000).empty());
    CHECK_EQ(skillinfo::type(5), 1);   // Bash: on an enemy
    CHECK_EQ(skillinfo::type(2), 0);   // Sword Mastery: passive
    CHECK_EQ(skillinfo::type(13000), -1);
    CHECK(skillinfo::quest(1001) && skillinfo::quest(142) && !skillinfo::quest(5));  // Charge Attack, First Aid
    CHECK(skillinfo::wedding(410) && skillinfo::soulLink(495));  // Call Baby, Onehand Quicken
    // 3.0.11-beta.228: the server's names, for the skills the client's table has none of
    CHECK(std::string(skillinfo::name(681)) == "Increase Weight Limit R");
    CHECK(std::string(skillinfo::name(13000)).empty());
}

// 3.0.18-beta.244 («Довідник» › «Навички»): the server's skill_db facts for a skill's card - its AEGIS name, most
// level, element (-1 the weapon's) and kind; the jobs the tree has, in order
TEST(skill_info_card_facts_and_the_jobs) {
    CHECK_EQ(std::string(skillinfo::aegis(5)), std::string("SM_BASH"));
    CHECK_EQ(skillinfo::most(5), 10);
    CHECK_EQ(skillinfo::element(5), -1);  // the weapon's
    CHECK_EQ(std::string(skillinfo::kind(5)), std::string("weapon"));
    CHECK_EQ(std::string(skillinfo::aegis(19)), std::string("MG_FIREBOLT"));
    CHECK_EQ(skillinfo::element(19), 3);  // fire
    CHECK_EQ(std::string(skillinfo::kind(19)), std::string("magic"));
    CHECK_EQ(skillinfo::idOf("MG_FIREBOLT"), 19);
    CHECK_EQ(skillinfo::idOf("NO_SUCH_SKILL"), 0);
    CHECK_EQ(std::string(skillinfo::aegis(99999)), std::string());
    const std::vector<int> jobs = skilltree::jobs();
    CHECK(!jobs.empty() && jobs.front() == 0);  // Novice first
    CHECK(std::find(jobs.begin(), jobs.end(), 7) != jobs.end());
    CHECK(std::find(jobs.begin(), jobs.end(), 4008) != jobs.end());
    CHECK(std::is_sorted(jobs.begin(), jobs.end()));
}
