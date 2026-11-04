#pragma once
#include <set>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::skillwin {
struct Data;
}

namespace uagfx::skilltreeview {

// 3.0.11-beta.224 (the skills concept, stage 2: «Дерево»): a tab of «Навички» as a tree - its skills in columns by
// the depth of their requirements on the tab, a link from each requirement with the level it needs; the requirements
// from the jobs before as chips in a column of their own on the left. Boxes at the given scale from the content's top
// left (the window scrolls it).
struct Node {
    int index = 0;  // the skill's in its tab
    int col = 0, row = 0;
    panel::Box box;
};
struct Chip {  // a requirement from a job before
    int id = 0;
    std::string name;
    int level = 0;
    panel::Box box;
};
struct Link {
    int to = 0;    // a node's
    int from = 0;  // a node's, or a chip's
    bool chip = false;
    int level = 0;  // the level it needs
    bool met = false;
};
struct Plan {
    std::vector<Node> nodes;
    std::vector<Chip> chips;
    std::vector<Link> links;
    float w = 0, h = 0;
};

constexpr float kNodeW = 128, kNodeH = 40, kColStep = 152, kRowStep = 50, kChipW = 116, kChipH = 26, kChipStep = 34;
constexpr float kChipsW = 158, kMargin = 8, kPlus = 18;  // the chips: 8 + 116, and the elbow past them

Plan arrange(const skillwin::Data& d, int tab, float scale);

int levelOf(const skillwin::Data& d, int id);  // learned, on any tab; 0 none
// The levels still missing to learn `id` (its requirements' and theirs, each counted once at the most needed): what
// the path to it costs in points; 0 when it is open
int pathCost(const skillwin::Data& d, int id);
// The skills `id` needs, all the way down (ids)
std::set<int> chain(const skillwin::Data& d, int id);

}  // namespace uagfx::skilltreeview
