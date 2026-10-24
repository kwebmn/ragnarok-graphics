#include <string>
#include <vector>

#include "core/ambience.hpp"
#include "core/config.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.7 (the user's idea 6): the map's own sounds around the character - birds, brooks, waves, the wind
namespace {
rsw::Sound at(const char* file, float x, float z, float range = 100.0f, float volume = 1.0f) {
    rsw::Sound s;
    s.file = file;
    s.pos[0] = x;
    s.pos[2] = z;
    s.range = range;
    s.volume = volume;
    return s;
}
// the character at the middle, the camera looking north: right +x, ahead +z
std::vector<ambience::Heard> around(const std::vector<rsw::Sound>& sounds, int most = ambience::kMost) {
    return ambience::heard(sounds, 0, 0, 0, 1, 0, 0, 1, most);
}
}  // namespace

TEST(ambience_within_its_reach_alone) {
    const auto h = around({at("bird.wav", 30, 0), at("brook.wav", 150, 0)});
    CHECK_EQ(h.size(), size_t(1));
    if (h.size() != 1) return;
    CHECK_EQ(h[0].source, size_t(0));
    CHECK(h[0].gain > 0.5f);
    CHECK(h[0].azimuth > 89.0f && h[0].azimuth < 91.0f);  // to the right
}

TEST(ambience_a_shores_waves_sound_once) {
    const auto h = around({at("waves.wav", 0, 80), at("waves.wav", 0, 20), at("waves.wav", -60, 0)});
    CHECK_EQ(h.size(), size_t(1));
    if (h.size() == 1) CHECK_EQ(h[0].source, size_t(1));  // the nearest of them
}

TEST(ambience_the_loudest_few) {
    std::vector<rsw::Sound> s;
    const char* files[] = {"a.wav", "b.wav", "c.wav", "d.wav"};
    for (int i = 0; i < 4; ++i) s.push_back(at(files[i], 0, float(10 + 20 * i)));
    const auto h = around(s, 2);
    CHECK_EQ(h.size(), size_t(2));
    if (h.size() == 2) CHECK(h[0].source == 0 && h[1].source == 1);  // the nearest two, loudest first
}

TEST(ambience_quieter_far_and_by_its_volume) {
    const rsw::Sound s = at("bird.wav", 0, 0, 100.0f, 0.5f);
    CHECK(ambience::gainAt(s, 10.0f) == 0.5f);  // a quarter of its reach: whole
    CHECK(ambience::gainAt(s, 60.0f) < ambience::gainAt(s, 30.0f));
    CHECK(ambience::gainAt(s, 100.0f) == 0.0f);
    rsw::Sound none = s;
    none.range = 0.0f;
    CHECK(ambience::gainAt(none, 0.0f) == 0.0f);  // a sound with no reach is never heard
}

TEST(ambience_settings) {
    // [sound] ambience - off unless turned on (new things ship off); ambience_volume 0-100 %; both apply at once
    CHECK(!parseConfig("").sound.ambience);
    CHECK_EQ(parseConfig("").sound.ambienceVolume, 60);
    const Config c = parseConfig("[sound]\nambience = 1\nambience_volume = 35\n");
    CHECK(c.sound.ambience && c.sound.ambienceVolume == 35);
    CHECK_EQ(parseConfig("[sound]\nambience_volume = 300\n").sound.ambienceVolume, 60);
    for (const char* k : {"sound.ambience", "sound.ambience_volume"}) {
        const settings::IniSetting* s = settings::findSetting(k);
        CHECK(s != nullptr && s->page == settings::Page::Sound);
        CHECK(appliesNow(k));
    }
    const LiveUpdate u = liveUpdate(parseConfig(""), c);
    CHECK(u.next.sound.ambience && u.next.sound.ambienceVolume == 35 && u.waiting.empty());
}
