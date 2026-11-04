#pragma once
#include <map>

namespace uagfx::skillwin {
struct Data;
struct Skill;
}  // namespace uagfx::skillwin

namespace uagfx::skillplan {

// 3.0.11-beta.225 (the skills concept, stage 3: «План»): points set on skills without asking the server - a skill's
// planned level (Skill::target, 0 none) above the learned one; a requirement counts by its planned level; taking a
// level off takes off the planned levels that needed it. «Застосувати» asks the server the raises one by one, each
// after the one before is learned (nextToApply). The plan is kept per character (win/skill_window) as targets by id.
using Targets = std::map<int, int>;  // a skill's id to its planned level

int extra(const skillwin::Skill& k);                   // the levels planned above the learned
int planLevel(const skillwin::Data& d, int id);        // learned, or planned when higher; 0 none
int cost(const skillwin::Data& d);                     // all the planned levels: the points they take
int left(const skillwin::Data& d);                     // the free points the plan leaves
bool canPlan(const skillwin::Data& d, const skillwin::Skill& k);  // one more level fits (its requirements planned)
bool up(skillwin::Data& d, int id);                    // one level more in the plan
bool down(skillwin::Data& d, int id);                  // one level less, and what needed it
int nextToApply(const skillwin::Data& d);              // a planned skill the server lets raise now; -1 none

void put(skillwin::Data& d, const Targets& t);  // onto the skills, learned levels dropping out
Targets taken(const skillwin::Data& d);         // what is still planned

}  // namespace uagfx::skillplan
