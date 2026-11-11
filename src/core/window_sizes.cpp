#include "core/window_sizes.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace uagfx::windowsizes {

float pulled(float from, float w0, float h0, float w, float h) {
    if (w0 <= 0 || h0 <= 0) return std::clamp(from, kLeast, kMost);
    const float share = (w / w0 + h / h0) / 2;
    return std::clamp(from * share, kLeast, kMost);
}

float Sizes::of(const std::string& window) const {
    const auto it = sizes_.find(window);
    return it == sizes_.end() ? 1.0f : it->second;
}

void Sizes::set(const std::string& window, float size) { sizes_[window] = std::clamp(size, kLeast, kMost); }

std::string Sizes::text() const {
    std::string out;
    char line[96];
    for (const auto& [name, size] : sizes_) {
        std::snprintf(line, sizeof line, "%s %.3f\n", name.c_str(), double(size));
        out += line;
    }
    return out;
}

Sizes Sizes::parse(const std::string& text) {
    Sizes s;
    std::istringstream in(text);
    std::string name, value;
    while (in >> name >> value) {
        char* end = nullptr;
        const float v = std::strtof(value.c_str(), &end);
        if (end && *end == 0 && v > 0) s.set(name, v);
    }
    return s;
}

}  // namespace uagfx::windowsizes
