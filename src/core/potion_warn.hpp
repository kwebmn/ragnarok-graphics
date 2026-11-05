#pragma once
#include <map>

namespace uagfx::potionwarn {

// 3.0.11-beta.281 (the owner's feature 16): the healing items on the hotbar watched as the arrows are (.129) - one
// falling to kLow or fewer a soft chime, to none a lower one; its count red on its cell from kLow down. An empty bag
// (a map loading) tells nothing; an item new on the bar or bought up again starts afresh.
constexpr int kLow = 10;
class Watch {
public:
    // counts: the bar's healing items, id -> how many in the bag. 0 nothing, 2 one fell to kLow or fewer, 3 one ran
    // out (3 wins)
    int step(const std::map<int, int>& counts, bool bagKnown);

private:
    std::map<int, int> last_;
};
inline bool low(int count) { return count > 0 && count <= kLow; }

}  // namespace uagfx::potionwarn
