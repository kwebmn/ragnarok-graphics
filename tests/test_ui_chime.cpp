// 3.0.11-beta.129 (the owner «делай»: a sound when 10 arrows are left and when none are): the interface's short chimes
// (core/ui_chime) - RIFF WAVE images the game's own sound player takes
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/ui_chime.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

uint32_t rd(const std::vector<uint8_t>& w, size_t at, int bytes) {
    uint32_t v = 0;
    for (int i = 0; i < bytes; ++i) v |= uint32_t(w[at + size_t(i)]) << (8 * i);
    return v;
}

}  // namespace

// 22050 Hz, 16-bit, mono; no click at its ends; at most four tenths of full loudness; none left: lower and longer
TEST(chime_is_a_short_soft_wave) {
    for (const chime::Kind k : {chime::Kind::AmmoLow, chime::Kind::AmmoOut}) {
        const std::vector<uint8_t> w = chime::wav(k);
        CHECK(w.size() > 44);
        if (w.size() <= 44) continue;
        CHECK(std::memcmp(w.data(), "RIFF", 4) == 0 && std::memcmp(w.data() + 8, "WAVEfmt ", 8) == 0);
        CHECK(rd(w, 4, 4) == w.size() - 8 && rd(w, 16, 4) == 16);
        CHECK(rd(w, 20, 2) == 1 && rd(w, 22, 2) == 1 && rd(w, 24, 4) == 22050 && rd(w, 34, 2) == 16);
        CHECK(std::memcmp(w.data() + 36, "data", 4) == 0 && rd(w, 40, 4) == w.size() - 44);
        const size_t count = (w.size() - 44) / 2;
        int peak = 0;
        for (size_t i = 0; i < count; ++i) peak = std::max(peak, std::abs(int(int16_t(rd(w, 44 + 2 * i, 2)))));
        CHECK(peak >= 32767 / 5 && peak <= 32767 * 2 / 5);
        CHECK(std::abs(int(int16_t(rd(w, 44, 2)))) < 300 && std::abs(int(int16_t(rd(w, w.size() - 2, 2)))) < 300);
        const double seconds = double(count) / 22050.0;
        if (k == chime::Kind::AmmoLow) CHECK(seconds >= 0.25 && seconds <= 0.45);
        else CHECK(seconds >= 0.45 && seconds <= 0.75);
    }
}
