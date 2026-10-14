#include "core/haze.hpp"

#include "core/outdoors.hpp"

namespace uagfx::haze {

Look lookOf(const std::string& map) {
    Look l;
    if (outdoors(map)) {
        l.haze = 0.3f;
        l.hazeColour[0] = 0.70f;
        l.hazeColour[1] = 0.76f;
        l.hazeColour[2] = 0.85f;
        return l;
    }
    if (map.find("dun") != std::string::npos && map.find('@') == std::string::npos) {
        l.haze = 0.35f;
        l.hazeColour[0] = 0.05f;
        l.hazeColour[1] = 0.05f;
        l.hazeColour[2] = 0.07f;
        l.hazeStart = 140.0f;
        l.hazeEnd = 700.0f;
        l.fog = 0.32f;
        l.fogColour[0] = 0.42f;
        l.fogColour[1] = 0.45f;
        l.fogColour[2] = 0.52f;
    }
    return l;
}

}  // namespace uagfx::haze
