#include "core/item_pick.hpp"

#include <algorithm>

namespace uagfx::itempick {

bool Picked::has(int index) const { return std::find(picked_.begin(), picked_.end(), index) != picked_.end(); }

void Picked::toggle(int index) {
    const auto it = std::find(picked_.begin(), picked_.end(), index);
    if (it != picked_.end()) picked_.erase(it);
    else picked_.push_back(index);
    last_ = index;
}

void Picked::range(const std::vector<int>& shown, int index) {
    const auto a = std::find(shown.begin(), shown.end(), last_), b = std::find(shown.begin(), shown.end(), index);
    if (b == shown.end()) return;
    if (a == shown.end()) {
        if (!has(index)) picked_.push_back(index);
        last_ = index;
        return;
    }
    const auto from = std::min(a, b), to = std::max(a, b);
    for (auto it = from; it <= to; ++it)
        if (!has(*it)) picked_.push_back(*it);
    last_ = index;
}

void Picked::all(const std::vector<int>& shown) {
    for (const int i : shown)
        if (!has(i)) picked_.push_back(i);
}

void Picked::keepOnly(const std::vector<int>& there) {
    picked_.erase(std::remove_if(picked_.begin(), picked_.end(),
                                 [&](int i) { return std::find(there.begin(), there.end(), i) == there.end(); }),
                  picked_.end());
}

void Picked::clear() {
    picked_.clear();
    last_ = -1;
}

}  // namespace uagfx::itempick
