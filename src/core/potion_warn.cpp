#include "core/potion_warn.hpp"

#include <algorithm>

namespace uagfx::potionwarn {

int Watch::step(const std::map<int, int>& counts, bool bagKnown) {
    if (!bagKnown) return 0;
    int fresh = 0;
    for (const auto& [id, n] : counts) {
        const auto was = last_.find(id);
        if (was != last_.end()) {
            if (n == 0 && was->second > 0) fresh = 3;
            else if (n > 0 && n <= kLow && was->second > kLow) fresh = std::max(fresh, 2);
        }
    }
    last_ = counts;
    return fresh;
}

}  // namespace uagfx::potionwarn
