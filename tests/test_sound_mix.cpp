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

TEST(sound_pan_by_the_screens_right) {
    // 0.92: the screen's right along the world's x: a sound at +x is on the right, at -x on the left, ahead central
    CHECK(sound::panOf(100, 0, 1, 0, 100) == 127);
    CHECK(sound::panOf(-100, 0, 1, 0, 100) == 1);
    CHECK(sound::panOf(0, 100, 1, 0, 100) == 64);
    CHECK(sound::panOf(0.2f, 0.1f, 1, 0, 100) == 64);  // at the player
    CHECK(sound::panOf(100, 0, 1, 0, 60) == 102);      // 60 % of the way
    CHECK(sound::panOf(0, 100, 0, -1, 100) == 1);      // the camera turned: the right is -z
    CHECK(sound::panOf(100, 0, 1, 0, 0) == 64);
}

TEST(sound_fades_cross_at_even_power) {
    CHECK(sound::fadeIn(0) == 0.0 && sound::fadeOut(0) == 1.0);
    CHECK(sound::fadeIn(sound::kFadeMs) > 0.999 && sound::fadeOut(sound::kFadeMs) < 0.001);
    const double a = sound::fadeIn(700), b = sound::fadeOut(700);
    CHECK(a * a + b * b > 0.999 && a * a + b * b < 1.001);
    CHECK(sound::fadeIn(-5) == 0.0 && sound::fadeOut(99999) < 0.001);
}

TEST(sound_quietest_sample_stopped_first) {
    const int v[] = {90, 20, -1, 50};
    CHECK(sound::quietest(v, 4) == 1);
    const int none[] = {-1, -1};
    CHECK(sound::quietest(none, 2) == -1);
    const Config c = parseConfig("[sound]\nstereo = 0\nsteal = 0\ncrossfade = 0\nstereo_flip = 1\n");
    CHECK(c.sound.stereo == 0 && !c.sound.steal && !c.sound.crossfade && c.sound.stereoFlip);
    const Config d = parseConfig("");
    CHECK(d.sound.stereo == 60 && d.sound.steal && d.sound.crossfade && !d.sound.stereoFlip);
}
