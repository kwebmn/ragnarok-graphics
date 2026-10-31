#include "core/window_stack.hpp"

#include <algorithm>

namespace uagfx::winstack {

void Stack::see(Win w, bool shown) {
    const auto it = std::find(order_.begin(), order_.end(), w);
    if (shown && it == order_.end())
        order_.push_back(w);
    else if (!shown && it != order_.end())
        order_.erase(it);
}

void Stack::raise(Win w) {
    const auto it = std::find(order_.begin(), order_.end(), w);
    if (it == order_.end()) return;
    order_.erase(it);
    order_.push_back(w);
}

std::optional<Win> Stack::top() const {
    if (order_.empty()) return std::nullopt;
    return order_.back();
}

std::vector<Win> Stack::topDown() const { return std::vector<Win>(order_.rbegin(), order_.rend()); }

bool Stack::marked(Win w) const { return order_.size() >= 2 && order_.back() == w; }

}  // namespace uagfx::winstack
