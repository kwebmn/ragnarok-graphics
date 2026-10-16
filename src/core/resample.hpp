#pragma once
#include <cmath>
#include <cstdint>

#include "core/wav.hpp"

namespace uagfx::sound {

// 1.2: a game's sound read out at another rate, as one channel (a stereo one mixed down), through a one-pole low-pass
// (the distance's muffle) - for the spatial objects, which take 48 kHz floats.
class Reader {
public:
    Reader() = default;
    Reader(const wav::Sound& s, uint32_t outRate) : s_(s), step_(double(s.rate) / double(outRate)), out_(outRate) {}
    void setCutoff(float hz) {
        const float a = 1.0f - std::exp(-2.0f * 3.14159265f * hz / float(out_));
        a_ = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
    }
    // Up to `n` samples into `dst`; how many there were (fewer at the end).
    uint32_t read(float* dst, uint32_t n) {
        const uint32_t frames = s_.blockAlign ? s_.bytes / s_.blockAlign : 0;
        uint32_t k = 0;
        for (; k < n; ++k) {
            const uint32_t i = uint32_t(pos_);
            if (i >= frames) break;
            const float t = float(pos_ - double(i));
            const float x = at(i) * (1.0f - t) + (i + 1 < frames ? at(i + 1) : 0.0f) * t;
            y_ += a_ * (x - y_);
            dst[k] = y_;
            pos_ += step_;
        }
        return k;
    }
    bool done() const { return s_.blockAlign == 0 || uint32_t(pos_) >= s_.bytes / s_.blockAlign; }

private:
    float at(uint32_t frame) const {
        const uint8_t* p = s_.data + size_t(frame) * s_.blockAlign;
        float sum = 0.0f;
        for (uint16_t c = 0; c < s_.channels; ++c) {
            if (s_.bits == 8) {
                sum += (float(p[c]) - 128.0f) / 128.0f;
            } else {
                int16_t v;
                std::memcpy(&v, p + c * 2, 2);
                sum += float(v) / 32768.0f;
            }
        }
        return sum / float(s_.channels);
    }
    wav::Sound s_{};
    double pos_ = 0.0, step_ = 1.0;
    uint32_t out_ = 48000;
    float a_ = 1.0f, y_ = 0.0f;
};

}  // namespace uagfx::sound
