#include "core/camera_memory.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace uagfx::cameramemory {
namespace {

bool number(const std::string& word, float& out) {
    if (word.empty()) return false;
    char* end = nullptr;
    const float v = std::strtof(word.c_str(), &end);
    if (end != word.c_str() + word.size() || !std::isfinite(v)) return false;
    out = v;
    return true;
}

bool held(const Pose& p) { return p.tilt >= -90.0f && p.tilt <= 0.0f && p.zoom >= 100.0f && p.zoom <= 5000.0f; }

bool near(float a, float b) { return std::fabs(a - b) <= 0.5f; }

}  // namespace

std::string format(const Memory& m) {
    char text[96];
    std::snprintf(text, sizeof text, "outdoors %.1f %.1f\nbuildings %.1f %.1f\n", m.outdoors.tilt, m.outdoors.zoom,
                  m.buildings.tilt, m.buildings.zoom);
    return text;
}

std::optional<Memory> parse(const std::string& text) {
    Memory m;
    bool outdoors = false, buildings = false;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream words(line);
        std::string name, tilt, zoom, more;
        if (!(words >> name)) continue;
        Pose p;
        if (!(words >> tilt >> zoom) || (words >> more) || !number(tilt, p.tilt) || !number(zoom, p.zoom) ||
            !held(p))
            return std::nullopt;
        if (name == "outdoors") {
            m.outdoors = p;
            outdoors = true;
        } else if (name == "buildings") {
            m.buildings = p;
            buildings = true;
        }
    }
    if (!outdoors || !buildings) return std::nullopt;
    return m;
}

Memory withNow(Memory slots, Pose now, bool building) {
    (building ? slots.buildings : slots.outdoors) = now;
    return slots;
}

bool differs(const Memory& a, const Memory& b) {
    return !near(a.outdoors.tilt, b.outdoors.tilt) || !near(a.outdoors.zoom, b.outdoors.zoom) ||
           !near(a.buildings.tilt, b.buildings.tilt) || !near(a.buildings.zoom, b.buildings.zoom);
}

}  // namespace uagfx::cameramemory
