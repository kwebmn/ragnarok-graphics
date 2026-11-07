#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/game_camera.hpp"

namespace uagfx::hit_flash {

// 0.96: a monster hit flashes white for a moment (effects.hit_flash). The game plays a hit's sound where it lands -
// a weapon's "_hit_..." or the monster's own "..._damage" - and the driver hears it: the place goes onto the screen
// and the monster drawn there is drawn once more, adding light, fading over kFlashMs.
constexpr double kFlashMs = 140.0;
// 3.0.16-beta.7 (the owner: «делай все» - a death's flash): a monster's death - its «..._die» sound (the game's
// monsters' own: caramel_die.wav, wraith_die.wav) - flashes brighter and longer, the dying sprite fading under it
constexpr double kDeathMs = 420.0, kDeathHoldMs = 70.0;
constexpr float kDeathPeak = 1.18f;  // the draw adds 0.85 of it: a death's whole white (a hit's 85 %)

enum class Sound : uint8_t { Other, Hit, Death };

inline std::string soundName(const char* path) {
    std::string name(path ? path : "");
    const size_t slash = name.find_last_of("\\/");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return name;
}

// What a sound file is: a hit's (data\wav\_hit_sword.wav, data\wav\poring_damage.wav), a death's (..._die.wav)
inline Sound soundKind(const char* path) {
    if (!path) return Sound::Other;
    const std::string name = soundName(path);
    if (name.find("_hit") != std::string::npos || name.find("_damage") != std::string::npos) return Sound::Hit;
    const size_t die = name.rfind("_die");
    if (die != std::string::npos && die > 0 && (die + 4 == name.size() || name[die + 4] == '.')) return Sound::Death;
    return Sound::Other;
}

inline bool hitSound(const char* path) { return soundKind(path) == Sound::Hit; }

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

// A monster's sprite on the screen (the game's pixels): its box, the feet at y1.
struct Box {
    float x0, x1, y0, y1;
};

// 2.7.10 (the tester at prt_fild08 200 200, a hill: no flash seen): the game plays a hit's sound at the monster's x
// and z but at the player's height (348 of its 366 calls pass 0 for it) - on a slope the point lands tens of pixels
// above or below the monster's feet and no sprite took it. The monsters drawn last frame, and the hit given to the
// one whose box takes its x, the feet nearest it - within kReach (about 24 units of height at the usual distance).
class Monsters {
public:
    static constexpr float kReach = 240.0f;
    void note(const Box& b) {
        if (now_.size() < 256) now_.push_back(b);
    }
    void frame() {
        last_.swap(now_);
        now_.clear();
    }
    // The point moved onto the feet of that monster; false (and the point as it was): none.
    bool aim(float& sx, float& sy) const {
        const Box* best = nullptr;
        float gap = kReach;
        for (const Box& b : last_) {
            const float pad = std::max((b.x1 - b.x0) * 0.15f, 6.0f);
            if (sx < b.x0 - pad || sx > b.x1 + pad) continue;
            const float d = std::fabs(sy - b.y1);
            if (d <= gap) {
                gap = d;
                best = &b;
            }
        }
        if (!best) return false;
        sx = (best->x0 + best->x1) * 0.5f;
        sy = best->y1;
        return true;
    }

private:
    std::vector<Box> now_, last_;
};

class Flashes {
public:
    // 3.0.16-beta.7: a death's flash - longer, whole for its first kDeathHoldMs
    void add(float sx, float sy, double nowMs, bool death = false) {
        hits_.push_back(Hit{sx, sy, nowMs, death});
        if (hits_.size() > 64) hits_.erase(hits_.begin());
    }
    // How bright the flash over a sprite of this box is now (0 none .. 1): a hit at its feet - across its width, from
    // a little above its bottom to a little below.
    float strength(float x0, float x1, float y0, float y1, double nowMs) {
        float best = 0.0f;
        for (size_t i = 0; i < hits_.size();) {
            const double age = nowMs - hits_[i].at;
            const double life = hits_[i].death ? kDeathMs : kFlashMs;
            if (age > life || age < 0) {
                hits_.erase(hits_.begin() + long(i));
                continue;
            }
            const Hit& h = hits_[i];
            const float pad = std::max((x1 - x0) * 0.15f, 6.0f);
            if (h.sx >= x0 - pad && h.sx <= x1 + pad && h.sy >= y0 + (y1 - y0) * 0.5f && h.sy <= y1 + 24.0f)
                best = std::max(best, h.death ? kDeathPeak * (age < kDeathHoldMs
                                                                   ? 1.0f
                                                                   : float(1.0 - (age - kDeathHoldMs) /
                                                                                     (kDeathMs - kDeathHoldMs)))
                                              : float(1.0 - age / kFlashMs));
            ++i;
        }
        return best;
    }

private:
    struct Hit {
        float sx, sy;
        double at;
        bool death = false;
    };
    std::vector<Hit> hits_;
};

}  // namespace uagfx::hit_flash
