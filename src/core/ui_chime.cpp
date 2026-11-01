#include "core/ui_chime.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::chime {
namespace {

constexpr int kRate = 22050;
constexpr double kPeak = 0.33;  // of full loudness: soft beside the game's own sounds

struct Note {
    double hz, at, length;  // its pitch, its start and its length (s)
};

// A bell-like note: the tone, its octave a quarter as loud, a twelfth a twelfth; up in 6 ms, falling away, the last
// 12 ms faded to silence (no click)
double note(const Note& n, double t) {
    const double u = t - n.at;
    if (u < 0 || u >= n.length) return 0;
    const double rise = std::min(1.0, u / 0.006), fade = std::min(1.0, (n.length - u) / 0.012);
    const double fall = std::exp(-u / (n.length / 3.2));
    const double w = 2 * 3.14159265358979323846 * n.hz * u;
    return (std::sin(w) + 0.25 * std::sin(2 * w) + 0.08 * std::sin(3 * w)) * rise * fall * fade;
}

void put16(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(uint8_t(v));
    out.push_back(uint8_t(v >> 8));
}

void put32(std::vector<uint8_t>& out, uint32_t v) {
    put16(out, v & 0xFFFF);
    put16(out, v >> 16);
}

}  // namespace

std::vector<uint8_t> wav(Kind kind) {
    const std::vector<Note> notes = kind == Kind::AmmoLow
                                        ? std::vector<Note>{{1046.5, 0, 0.16}, {784.0, 0.12, 0.22}}
                                        : std::vector<Note>{{1046.5, 0, 0.16}, {784.0, 0.14, 0.18}, {523.25, 0.28, 0.32}};
    double end = 0;
    for (const Note& n : notes) end = std::max(end, n.at + n.length);
    const size_t count = size_t(std::ceil(end * kRate));
    std::vector<double> v(count, 0.0);
    double most = 1e-9;
    for (size_t i = 0; i < count; ++i) {
        for (const Note& n : notes) v[i] += note(n, double(i) / kRate);
        most = std::max(most, std::fabs(v[i]));
    }
    std::vector<uint8_t> out;
    out.reserve(44 + 2 * count);
    const uint32_t data = uint32_t(2 * count);
    for (const char c : {'R', 'I', 'F', 'F'}) out.push_back(uint8_t(c));
    put32(out, 36 + data);
    for (const char c : {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '}) out.push_back(uint8_t(c));
    put32(out, 16);
    put16(out, 1);              // PCM
    put16(out, 1);              // mono
    put32(out, kRate);
    put32(out, kRate * 2);      // bytes a second
    put16(out, 2);              // a frame's bytes
    put16(out, 16);             // bits
    for (const char c : {'d', 'a', 't', 'a'}) out.push_back(uint8_t(c));
    put32(out, data);
    for (const double s : v) put16(out, uint32_t(uint16_t(int16_t(std::lround(s / most * kPeak * 32767)))));
    return out;
}

}  // namespace uagfx::chime
