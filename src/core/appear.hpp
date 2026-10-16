#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::appear {

// 0.93: monsters, characters, NPCs and items fade in where they come into view (the game shows them at once). A
// sprite's draw is known by its place on the screen from frame to frame: one near where a sprite of the same kind
// was in the last frame is that sprite (its age goes on); one with none near is new and fades in over kFadeMs.
constexpr double kFadeMs = 250.0;
constexpr float kNear = 96.0f;  // game pixels a sprite may move between two frames (the camera turning, a run)

class Tracker {
public:
    // The sprite of `kind` drawn now about (x, y): how much of it shows, 0..1.
    float alpha(uint8_t kind, float x, float y, double nowMs) {
        int best = -1;
        float bestD = kNear * kNear;
        for (size_t i = 0; i < last_.size(); ++i) {
            const Seen& s = last_[i];
            if (s.taken || s.kind != kind) continue;
            const float dx = s.x - x, dy = s.y - y, d = dx * dx + dy * dy;
            if (d < bestD) {
                bestD = d;
                best = int(i);
            }
        }
        double born = nowMs;
        if (best >= 0) {
            last_[size_t(best)].taken = true;
            born = last_[size_t(best)].born;
        } else if (quiet_) {
            born = nowMs - kFadeMs;  // the first frame after a pause: what is there was there
        }
        now_.push_back(Seen{kind, x, y, born, false});
        const double t = (nowMs - born) / kFadeMs;
        return t >= 1.0 ? 1.0f : t <= 0.0 ? 0.0f : float(t);
    }
    // A frame shown: its sprites are the next one's last. `gapMs` since the one before - over a second (a map
    // loading, the game in the background) and nothing fades: it is shown as it is.
    void frame(double gapMs) {
        last_.swap(now_);
        now_.clear();
        quiet_ = gapMs > 1000.0;
    }

private:
    struct Seen {
        uint8_t kind;
        float x, y;
        double born;
        bool taken;
    };
    std::vector<Seen> last_, now_;
    bool quiet_ = true;
};

}  // namespace uagfx::appear
