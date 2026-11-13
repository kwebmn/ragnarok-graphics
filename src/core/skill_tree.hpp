#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace uagfx::skilltree {

// 3.0.11-beta.96 («Навички», the concept's skills window: tabs by job, a skill's level out of its most): the server's
// skill tree - db/skill_tree.txt (tools/skill_tree.py writes core/skill_tree.inc): each job's skills in the server's
// order, their most levels and the skills they need. The client tells a skill's learned level only (0x010f).
struct Entry {
    uint16_t job, id;
    uint8_t most;
    uint16_t need[5][2];  // a needed skill and its level; 0 none
};

int maxLevel(int job, int id);  // 0: not in that job's tree
// The jobs a character of `job` passed through, its first job first (a Lord Knight: Novice, Swordman, Knight, itself);
// a job not known: itself
std::vector<int> chain(int job);
// The first of `jobs` whose tree has the skill (the tab it sits on); -1 none
int homeJob(const std::vector<int>& jobs, int id);
int order(int job, int id);  // its place in that job's tree; a large number when not there
std::vector<std::pair<int, int>> needs(int job, int id);  // the skills it needs and their levels
// 3.0.11-beta.223 (the skills concept, stage 1): every skill of a job's tree, in the server's order (its earlier
// jobs' too)
std::vector<int> skills(int job);
// 3.0.11-beta.226 (stage 4: «Наступна»): the jobs a character of `job` may change to next, as the game's NPCs give
// them (a first job's two, a second job's transcendent one; an Archer's Bard or Dancer by `male`); none past them
std::vector<int> nextJobs(int job, bool male);
// 3.0.18-beta.244 («Довідник» › «Навички»): the jobs the tree has, by their numbers
std::vector<int> jobs();

}  // namespace uagfx::skilltree
