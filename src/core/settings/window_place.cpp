#include "core/settings/window_place.hpp"

#include <sstream>

namespace uagfx::settings {

std::string placeToken(const WindowPlace& p) {
    return std::to_string(p.left) + " " + std::to_string(p.top) + " " + std::to_string(p.right) + " " +
           std::to_string(p.bottom) + " " + (p.maximized ? "1" : "0");
}

std::optional<WindowPlace> parsePlace(const std::string& token) {
    std::istringstream in(token);
    long long v[5];
    for (long long& x : v)
        if (!(in >> x)) return std::nullopt;
    std::string more;
    if (in >> more) return std::nullopt;
    const long long w = v[2] - v[0];
    const long long h = v[3] - v[1];
    if (w < 200 || w > 20000 || h < 200 || h > 20000 || v[4] < 0 || v[4] > 1) return std::nullopt;
    for (int i = 0; i < 4; ++i)
        if (v[i] < -100000 || v[i] > 100000) return std::nullopt;
    return WindowPlace{int(v[0]), int(v[1]), int(v[2]), int(v[3]), v[4] == 1};
}

}  // namespace uagfx::settings
