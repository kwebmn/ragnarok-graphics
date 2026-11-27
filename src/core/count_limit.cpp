#include "core/count_limit.hpp"

#include <algorithm>

namespace uagfx::countlimit {

int most(int stack, int unitWeight, int weightFree, int placesFree, bool stacksThere) {
    if (stack <= 0) return 0;
    if (placesFree == 0 && !stacksThere) return 0;  // a new place wanted, none free
    long long m = stack;
    if (unitWeight > 0 && weightFree != -1) m = std::min<long long>(m, std::max(0, weightFree) / unitWeight);
    return int(std::max(0LL, m));
}

}  // namespace uagfx::countlimit
