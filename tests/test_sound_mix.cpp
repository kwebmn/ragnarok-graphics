#include "core/config.hpp"
#include "core/sound_mix.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(sound_rate_the_settings_or_the_games) {
    CHECK(sound::rateFor(44100, 22050) == 44100);
    CHECK(sound::rateFor(48000, 11025) == 48000);
    CHECK(sound::rateFor(0, 22050) == 22050);
}

TEST(sound_limiter_quieter_in_a_crowd_only) {
    CHECK(sound::crowdGain(0) == 1.0 && sound::crowdGain(sound::kFree) == 1.0);
    CHECK(sound::crowdGain(sound::kFree + 1) < 1.0);
    CHECK(sound::crowdGain(24) > 0.49 && sound::crowdGain(24) < 0.51);  // sqrt(6 / 24)
    CHECK(sound::crowdGain(1000) == sound::kFloor);
    for (int n = 1; n < 60; ++n) CHECK(sound::crowdGain(n + 1) <= sound::crowdGain(n));
    CHECK(sound::scaled(127, 1.0) == 127 && sound::scaled(127, 0.5) == 64 && sound::scaled(100, 0.0) == 0);
    CHECK(sound::scaled(200, 1.0) == 127 && sound::scaled(-5, 1.0) == 0);
}

TEST(sound_settings_read) {
    const Config d = parseConfig("");
    CHECK(d.sound.rate == 44100 && d.sound.voices && d.sound.background == 30 && d.sound.limiter);
    const Config c = parseConfig("[sound]\nrate = 0\nvoices = 0\nbackground = 100\nlimiter = 0\n");
    CHECK(c.sound.rate == 0 && !c.sound.voices && c.sound.background == 100 && !c.sound.limiter);
    CHECK(parseConfig("[sound]\nrate = 48000\n").sound.rate == 48000);
    CHECK(parseConfig("[sound]\nrate = 12345\n").sound.rate == 44100);  // not one of the rates: the default
    CHECK(parseConfig("[sound]\nbackground = 150\n").sound.background == 30);
}
