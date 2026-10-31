#pragma once
#include <vector>

namespace uapad {

// 3.0.11-beta.93 (R-KMG8GE: B pressed on the game's equipment and options windows and on the big map went to the empty
// slot F3): the windows B closes - the big map and the game's own windows the tile opens - in the order they opened;
// B closes the last one still open. Each window by its number (the runtime's list).
class WindowOrder {
public:
    // Each tick, each window: open now or not
    void see(int window, bool open);
    // The window B closes: the last opened of those still open; -1 none
    int last() const { return order_.empty() ? -1 : order_.back(); }

private:
    std::vector<int> order_;  // the open ones, the oldest first
};

}  // namespace uapad
