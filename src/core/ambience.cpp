#include "core/ambience.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

#include "core/sound_space.hpp"

namespace uagfx::ambience {

float gainAt(const rsw::Sound& s, float distance) {
    if (s.range <= 0.0f || !(s.volume > 0.0f)) return 0.0f;
    return s.volume * sound::distanceGain(distance, s.range * 0.25f, s.range);
}

std::vector<Heard> heard(const std::vector<rsw::Sound>& sounds, float x, float y, float z, float rx, float rz, float fx,
                         float fz, int most) {
    std::map<std::string, Heard> nearest;  // by file
    for (size_t i = 0; i < sounds.size(); ++i) {
        const rsw::Sound& s = sounds[i];
        Heard h;
        h.source = i;
        sound::heard(s.pos[0] - x, s.pos[2] - z, rx, rz, fx, fz, h.azimuth, h.distance);
        const float up = s.pos[1] - y;
        h.distance = std::sqrt(h.distance * h.distance + up * up);
        h.gain = gainAt(s, h.distance);
        if (h.gain <= 0.0f) continue;
        const auto it = nearest.find(s.file);
        if (it == nearest.end() || h.gain > it->second.gain) nearest[s.file] = h;
    }
    std::vector<Heard> out;
    for (const auto& n : nearest) out.push_back(n.second);
    std::sort(out.begin(), out.end(), [](const Heard& a, const Heard& b) { return a.gain > b.gain; });
    if (int(out.size()) > most) out.resize(size_t(std::max(most, 0)));
    return out;
}

}  // namespace uagfx::ambience
