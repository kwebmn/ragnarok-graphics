#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace uagfx::sound {

// 1.1: the game's sounds placed around the player for the new engine (win/sound_engine): where a sound is from the
// player as the listener hears it, how loud and how muffled with the distance, and its share for each speaker.

// A sound at (dx, dz) from the player (the world's x and z) with the screen's right at (rx, rz) and its far side
// (where the camera looks, along the ground) at (fx, fz): its angle from straight ahead (degrees, + to the right)
// and its distance.
inline void heard(float dx, float dz, float rx, float rz, float fx, float fz, float& azimuth, float& distance) {
    const float rl = std::sqrt(rx * rx + rz * rz), fl = std::sqrt(fx * fx + fz * fz);
    distance = std::sqrt(dx * dx + dz * dz);
    if (rl < 1e-6f || fl < 1e-6f || distance < 1e-3f) {
        azimuth = 0.0f;
        return;
    }
    const float side = (dx * rx + dz * rz) / rl, ahead = (dx * fx + dz * fz) / fl;
    azimuth = std::atan2(side, ahead) * 57.29578f;
}

// How loud at `d` as the game has it - all of it up to `inner`, then inner / d - and, past three quarters of `outer`
// (its reach), fading to none at it (the game cut a sound off there).
inline float distanceGain(float d, float inner, float outer) {
    if (d >= outer) return 0.0f;
    float g = d <= inner ? 1.0f : inner / d;
    const float edge = outer * 0.75f;
    if (d > edge) g *= (outer - d) / (outer - edge);
    return g;
}

// A low-pass filter's cutoff (Hz) for a sound at `d`: open close by, muffled far off - the air takes the highs.
inline float muffleHz(float d, float inner, float outer) {
    if (d <= inner) return 20000.0f;
    const float t = std::clamp((d - inner) / std::max(outer - inner, 1.0f), 0.0f, 1.0f);
    return 20000.0f * std::pow(3500.0f / 20000.0f, t);
}

// The speakers of a channel mask (WAVEFORMATEXTENSIBLE order): each one's angle, or NAN for the LFE channel.
inline std::vector<float> speakers(uint32_t mask) {
    static const float kAngle[11] = {-30, 30, 0, NAN, -135, 135, -15, 15, 180, -90, 90};
    // FL, FR, FC, LFE, BL, BR, FLC, FRC, BC, SL, SR
    std::vector<float> out;
    for (int bit = 0; bit < 11; ++bit)
        if (mask >> bit & 1) out.push_back(kAngle[bit]);
    if (out.empty()) out = {-30, 30};
    return out;
}

// Each speaker's gain for a sound from `azimuth` (degrees): the two speakers either side of it share it at even
// power (a sound between them sounds from between them); one speaker alone - all of it; the LFE none.
inline void pan(float azimuth, const std::vector<float>& angles, float* gains) {
    const size_t n = angles.size();
    std::vector<size_t> ring;
    for (size_t i = 0; i < n; ++i) {
        gains[i] = 0.0f;
        if (!std::isnan(angles[i])) ring.push_back(i);
    }
    if (ring.empty()) return;
    if (ring.size() == 1) {
        gains[ring[0]] = 1.0f;
        return;
    }
    const auto wrap = [](float a) { return a - 360.0f * std::floor((a + 180.0f) / 360.0f); };  // (-180, 180]
    std::sort(ring.begin(), ring.end(), [&](size_t a, size_t b) { return angles[a] < angles[b]; });
    const float a = wrap(azimuth);
    for (size_t k = 0; k < ring.size(); ++k) {
        const size_t i = ring[k], j = ring[(k + 1) % ring.size()];
        const float from = angles[i];
        float span = angles[j] - from;
        if (span <= 0.0f) span += 360.0f;
        float into = a - from;
        if (into < 0.0f) into += 360.0f;
        if (into <= span) {
            const float t = into / span;
            gains[i] = std::cos(t * 1.5707963f);
            gains[j] = std::sin(t * 1.5707963f);
            return;
        }
    }
}

}  // namespace uagfx::sound
