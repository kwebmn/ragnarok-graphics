#pragma once
#include <string>
#include <vector>

namespace uagfx::questprogress {

// 3.0.19-beta.4 (the owner: «давай» - a quest's step in the game's «Довідник»; the testers: «щоб відстежувалось від
// серва де ти, на чому застряг»): the site reads the character's quest variable on the server (KielHyreQuest in
// global_reg_value) and says the step and whom to go to - GET /api/client/quests/progress?char=<name> with the
// device's token (only the site account's own characters)
struct State {
    std::string slug;
    int value = 0, step = 0, steps = 0;
    bool started = false, done = false;
    std::string npc, map;  // whom to go to next ("" none: finished)
    int x = -1, y = -1;
};
// The site's answer ({"char": …, "quests": [{slug, value, started, done, step, steps, npc: {name, map, x, y} | null}]})
// into the states; false: not one (an error's answer, not JSON)
bool parse(const std::string& json, std::vector<State>& out);
const State* find(const std::vector<State>& states, const std::string& slug);  // null: none for it

}  // namespace uagfx::questprogress
