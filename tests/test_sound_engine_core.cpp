#include <cmath>
#include <cstring>
#include <vector>

#include "core/sound_place.hpp"
#include "core/sound_space.hpp"
#include "core/wav.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::vector<uint8_t> wavFile(uint16_t channels, uint32_t rate, uint16_t bits, uint32_t samples, bool extra) {
    std::vector<uint8_t> f(12);
    std::memcpy(f.data(), "RIFF", 4);
    std::memcpy(f.data() + 8, "WAVE", 4);
    const auto chunk = [&](const char* id, const std::vector<uint8_t>& body) {
        const uint32_t n = uint32_t(body.size());
        f.insert(f.end(), id, id + 4);
        f.insert(f.end(), reinterpret_cast<const uint8_t*>(&n), reinterpret_cast<const uint8_t*>(&n) + 4);
        f.insert(f.end(), body.begin(), body.end());
        if (n & 1) f.push_back(0);
    };
    std::vector<uint8_t> fmt(16);
    const uint16_t pcm = 1, align = uint16_t(channels * bits / 8);
    const uint32_t bps = rate * align;
    std::memcpy(&fmt[0], &pcm, 2);
    std::memcpy(&fmt[2], &channels, 2);
    std::memcpy(&fmt[4], &rate, 4);
    std::memcpy(&fmt[8], &bps, 4);
    std::memcpy(&fmt[12], &align, 2);
    std::memcpy(&fmt[14], &bits, 2);
    chunk("fmt ", fmt);
    if (extra) chunk("LIST", std::vector<uint8_t>(7, 1));  // an odd one, padded
    chunk("data", std::vector<uint8_t>(samples * align, 0x40));
    const uint32_t riff = uint32_t(f.size() - 8);
    std::memcpy(f.data() + 4, &riff, 4);
    return f;
}

}  // namespace

TEST(sound_engine_reads_the_games_wav) {
    const std::vector<uint8_t> f = wavFile(1, 22050, 16, 1000, true);
    wav::Sound s;
    CHECK(wav::parse(f.data(), uint32_t(f.size()), s));
    CHECK(s.format == 1 && s.channels == 1 && s.rate == 22050 && s.bits == 16 && s.bytes == 2000);
    CHECK(wav::playable(s));
    CHECK(s.data[0] == 0x40);
    wav::Sound cut;
    CHECK(wav::parse(f.data(), uint32_t(f.size() - 500), cut) && cut.bytes == 1500);  // a short image: what is there
    const std::vector<uint8_t> st = wavFile(2, 44100, 8, 10, false);
    CHECK(wav::parse(st.data(), uint32_t(st.size()), s) && wav::playable(s) && s.bytes == 20);
    std::vector<uint8_t> bad = f;
    bad[8] = 'X';
    CHECK(!wav::parse(bad.data(), uint32_t(bad.size()), s));
    wav::Sound adpcm = s;
    adpcm.format = 0x11;
    CHECK(!wav::playable(adpcm));
}

TEST(sound_engine_places_a_sound_around_the_listener) {
    float az = 0, d = 0;
    // The screen's right +x, its far side +z.
    sound::heard(10, 0, 1, 0, 0, 1, az, d);
    CHECK(std::fabs(az - 90.0f) < 0.01f && std::fabs(d - 10.0f) < 0.01f);
    sound::heard(0, 10, 1, 0, 0, 1, az, d);
    CHECK(std::fabs(az) < 0.01f);  // ahead
    sound::heard(0, -10, 1, 0, 0, 1, az, d);
    CHECK(std::fabs(std::fabs(az) - 180.0f) < 0.01f);  // behind
    sound::heard(-10, 0, 1, 0, 0, 1, az, d);
    CHECK(std::fabs(az + 90.0f) < 0.01f);  // left
}

TEST(sound_engine_distance_and_muffle) {
    CHECK(sound::distanceGain(10, 40, 250) == 1.0f);
    CHECK(std::fabs(sound::distanceGain(80, 40, 250) - 0.5f) < 1e-4f);
    CHECK(sound::distanceGain(250, 40, 250) == 0.0f);
    CHECK(sound::distanceGain(240, 40, 250) < 0.2f * 40.0f / 240.0f);  // fading towards the edge
    for (float d = 1; d < 250; d += 5) CHECK(sound::distanceGain(d + 5, 40, 250) <= sound::distanceGain(d, 40, 250));
    CHECK(sound::muffleHz(10, 40, 250) == 20000.0f);
    CHECK(std::fabs(sound::muffleHz(250, 40, 250) - 3500.0f) < 1.0f);
}

TEST(sound_engine_pans_onto_any_speakers) {
    const std::vector<float> stereo = sound::speakers(0x3);
    float g[11];
    sound::pan(0, stereo, g);
    CHECK(std::fabs(g[0] - g[1]) < 1e-4f && std::fabs(g[0] * g[0] + g[1] * g[1] - 1.0f) < 1e-4f);
    sound::pan(30, stereo, g);
    CHECK(g[1] > 0.999f && g[0] < 0.001f);
    sound::pan(-90, stereo, g);  // past the left speaker: towards the back between them, mostly left
    CHECK(g[0] > g[1]);
    const std::vector<float> s51 = sound::speakers(0x3F);  // FL FR FC LFE BL BR
    CHECK(s51.size() == 6 && std::isnan(s51[3]));
    sound::pan(0, s51, g);
    CHECK(g[2] > 0.999f && g[3] == 0.0f);  // ahead: the centre
    sound::pan(180, s51, g);
    CHECK(std::fabs(g[4] - g[5]) < 1e-4f && g[4] > 0.6f && g[0] == 0.0f);  // behind: the back pair
    sound::pan(-100, s51, g);
    CHECK(g[0] > 0.0f && g[4] > 0.0f && g[1] == 0.0f);  // left side: front left and back left
}

TEST(sound_engine_rooms_by_the_map) {
    CHECK(sound::roomOf("prt_fild08") == sound::Room::Open);
    CHECK(sound::roomOf("prontera") == sound::Room::Town);
    CHECK(sound::roomOf("pay_dun01") == sound::Room::Cave);
    CHECK(sound::roomOf("prt_in") == sound::Room::Indoor);
    CHECK(sound::roomOf("1@gef") == sound::Room::Cave);
}

#include "core/config.hpp"

TEST(sound_engine_settings) {
    const Config d = parseConfig("");
    CHECK(!d.sound.engine && d.sound.reverb == 60);
    const Config c = parseConfig("[sound]\nengine = 1\nreverb = 20\n");
    CHECK(c.sound.engine && c.sound.reverb == 20);
}
