#include "core/cart_window.hpp"

namespace uagfx::cartwindow {

std::string title(const std::string& name, int count, int most) {
    if (most <= 0) return name;
    return name + " \xC2\xB7 " + std::to_string(count) + "/" + std::to_string(most);
}

}  // namespace uagfx::cartwindow
