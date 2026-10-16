#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::appear {

// 0.93: monsters, characters, NPCs and items fade in where they come into view (the game shows them at once). A
// sprite's draw is known by its place on the screen from frame to frame: one near where a sprite of the same kind
// was is that sprite (its age goes on); one with none near is new and fades in over kFadeMs.
// 1.0.2: they blinked now and then - a sprite is remembered for kMemoryMs after it was last drawn (a frame shown
// twice, a sprite skipped for a frame), and a new layer close by one already shown (a weapon drawn in an attack, a
// head turned) belongs to it: shown at once.
constexpr double kFadeMs = 250.0;
constexpr double kMemoryMs = 400.0;
constexpr float kNear = 96.0f;     // game pixels a sprite may move between two frames (the camera turning, a run)
constexpr float kLayer = 56.0f;    // ... and how close another layer of a shown one comes

class Tracker {
public:
    // The sprite of `kind` drawn now about (x, y): how much of it shows, 0..1.
    float alpha(uint8_t kind, float x, float y, double nowMs) {
        latest_ = nowMs;
        int best = -1;
        float bestD = kNear * kNear;
        bool shownNear = false;
        for (size_t i = 0; i < known_.size(); ++i) {
            const Seen& s = known_[i];
            if (s.kind != kind || nowMs - s.seen > kMemoryMs) continue;
            const float dx = s.x - x, dy = s.y - y, d = dx * dx + dy * dy;
            if (d < kLayer * kLayer && nowMs - s.born >= kFadeMs) shownNear = true;
            if (!s.taken && d < bestD) {
                bestD = d;
                best = int(i);
            }
        }
        double born = nowMs;
        if (best >= 0) {
            known_[size_t(best)].taken = true;
            born = known_[size_t(best)].born;
        } else if (quiet_ || shownNear) {
            born = nowMs - kFadeMs;  // after a pause what is there was there; a layer of one already shown
        }
        now_.push_back(Seen{kind, x, y, born, nowMs, false});
        const double t = (nowMs - born) / kFadeMs;
        return t >= 1.0 ? 1.0f : t <= 0.0 ? 0.0f : float(t);
    }
    // A frame shown: its sprites, and those not drawn in it lately, are the next one's known. `gapMs` since the one
    // before - over a second (a map loading, the game in the background) and nothing fades: it is shown as it is.
    void frame(double gapMs) {
        for (const Seen& s : known_)
            if (!s.taken && latest_ - s.seen <= kMemoryMs) now_.push_back(s);
        known_.swap(now_);
        now_.clear();
        for (Seen& s : known_) s.taken = false;
        quiet_ = gapMs > 1000.0;
    }

private:
    struct Seen {
        uint8_t kind;
        float x, y;
        double born, seen;
        bool taken;
    };
    std::vector<Seen> known_, now_;
    double latest_ = 0;
    bool quiet_ = true;
};

}  // namespace uagfx::appear
