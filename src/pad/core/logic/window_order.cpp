#include "pad/core/logic/window_order.hpp"

#include <algorithm>

namespace uapad {

void WindowOrder::see(int window, bool open) {
    const auto it = std::find(order_.begin(), order_.end(), window);
    if (open && it == order_.end())
        order_.push_back(window);
    else if (!open && it != order_.end())
        order_.erase(it);
}

}  // namespace uapad
