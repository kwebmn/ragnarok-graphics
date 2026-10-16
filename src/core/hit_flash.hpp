#pragma once
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "core/game_camera.hpp"

namespace uagfx::hit_flash {

// 0.96: a monster hit flashes white for a moment (effects.hit_flash). The game plays a hit's sound where it lands -
// a weapon's "_hit_..." or the monster's own "..._damage" - and the driver hears it: the place goes onto the screen
// and the monster drawn there is drawn once more, adding light, fading over kFlashMs.
constexpr double kFlashMs = 140.0;

// A sound file of a hit: data\wav\_hit_sword.wav, data\wav\poring_damage.wav.
inline bool hitSound(const char* path) {
    if (!path) return false;
    std::string name(path);
    const size_t slash = name.find_last_of("\\/");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return name.find("_hit") != std::string::npos || name.find("_damage") != std::string::npos;
}

// A point of the world onto the game's screen (its pixels) through the view matrix (world -> view, row vectors).
inline bool toScreen(const float view[12], const camera::Projection& p, float x, float y, float z, float& sx,
                     float& sy) {
    const float vx = x * view[0] + y * view[3] + z * view[6] + view[9];
    const float vy = x * view[1] + y * view[4] + z * view[7] + view[10];
    const float vz = x * view[2] + y * view[5] + z * view[8] + view[11];
    if (vz <= p.nearZ) return false;
    sx = vx / vz * p.f0 + p.cx;
    sy = vy / vz * p.f1 + p.cy;
    return true;
}

class Flashes {
public:
    void add(float sx, float sy, double nowMs) {
        hits_.push_back(Hit{sx, sy, nowMs});
        if (hits_.size() > 64) hits_.erase(hits_.begin());
    }
    // How bright the flash over a sprite of this box is now (0 none .. 1): a hit at its feet - across its width, from
    // a little above its bottom to a little below.
    float strength(float x0, float x1, float y0, float y1, double nowMs) {
        float best = 0.0f;
        for (size_t i = 0; i < hits_.size();) {
            const double age = nowMs - hits_[i].at;
            if (age > kFlashMs || age < 0) {
                hits_.erase(hits_.begin() + long(i));
                continue;
            }
            const Hit& h = hits_[i];
            const float pad = std::max((x1 - x0) * 0.15f, 6.0f);
            if (h.sx >= x0 - pad && h.sx <= x1 + pad && h.sy >= y0 + (y1 - y0) * 0.5f && h.sy <= y1 + 24.0f)
                best = std::max(best, float(1.0 - age / kFlashMs));
            ++i;
        }
        return best;
    }

private:
    struct Hit {
        float sx, sy;
        double at;
    };
    std::vector<Hit> hits_;
};

}  // namespace uagfx::hit_flash
