#include <cstring>
#include <string>
#include <vector>

#include "core/rsw.hpp"
#include "sample_files.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(rsw_lights_and_effects_of_a_map) {
    // 0.25: the map file's point lights and effects, past its models and sounds.
    const std::vector<uint8_t> b = testfiles::rsw();
    rsw::World w;
    std::string error;
    CHECK(rsw::parse(b.data(), b.size(), w, error));
    CHECK(error.empty());
    CHECK_EQ(w.waterType, 4);  // 0.31: lava
    CHECK_EQ(w.lights.size(), size_t(1));
    CHECK_EQ(w.effects.size(), size_t(1));
    CHECK_EQ(w.models.size(), size_t(1));  // 2.0: the model's file and place
    CHECK(w.lightKnown && w.lightLongitude == 45 && w.lightLatitude == 45);  // 2.1.3: the map's sun
    // 3.0.7-beta.3: its colours, as uaRO.exe lights its ground's vertices with them
    CHECK(w.lightDiffuse[0] == 0.4f && w.lightDiffuse[1] == 0.6f && w.lightDiffuse[2] == 0.8f);
    CHECK(w.lightAmbient[0] == 0.2f && w.lightAmbient[1] == 0.3f && w.lightAmbient[2] == 0.1f);
    if (w.models.size() == 1) CHECK(w.models[0].file == "tree.rsm" && w.models[0].scale[2] == 1.0f);
    if (w.lights.size() == 1) {
        CHECK(w.lights[0].pos[0] == 10.0f && w.lights[0].pos[1] == -20.0f && w.lights[0].pos[2] == 30.0f);
        CHECK(w.lights[0].color[0] == 1.0f && w.lights[0].color[1] == 0.5f && w.lights[0].color[2] == 0.25f);
        CHECK(w.lights[0].range == 120.0f);
    }
    if (w.effects.size() == 1) {
        CHECK_EQ(w.effects[0].id, 690);
        CHECK(w.effects[0].pos[0] == -100.0f && w.effects[0].pos[1] == -12.0f && w.effects[0].pos[2] == 200.0f);
    }
}

TEST(rsw_refuses_what_is_not_a_map) {
    rsw::World w;
    std::string error;
    const uint8_t junk[8] = {'G', 'R', 'S', 'X', 2, 1, 0, 0};
    CHECK(!rsw::parse(junk, sizeof junk, w, error));
    CHECK(!error.empty());
    std::vector<uint8_t> b = testfiles::rsw();
    b.resize(b.size() - 30);  // cut inside the effect: what came before it stays
    CHECK(rsw::parse(b.data(), b.size(), w, error));
    CHECK_EQ(w.lights.size(), size_t(1));
    CHECK(w.effects.empty());
}

TEST(rsw_sounds_of_a_map) {
    // 3.0.4-beta.7 (the user's idea 6): its sound objects - birds, brooks, waves - uaRO.exe loads and never plays
    const std::vector<uint8_t> b = testfiles::rsw();
    rsw::World w;
    std::string error;
    CHECK(rsw::parse(b.data(), b.size(), w, error));
    CHECK_EQ(w.sounds.size(), size_t(1));
    if (w.sounds.size() != 1) return;
    const rsw::Sound& s = w.sounds[0];
    CHECK_EQ(s.file, std::string("s.wav"));
    CHECK(s.pos[0] == 0.0f && s.pos[1] == 0.0f && s.pos[2] == 0.0f);
    CHECK(s.volume == 1.0f && s.range == 50.0f && s.cycle == 4.0f);
}

TEST(rsw_water_of_a_map) {
    // 3.0.4-beta.20 (the own ground on every map): its water as the game lays it - the level, the waves, the pictures'
    // pace - for the water past the game's far plane, where the game lays none
    const std::vector<uint8_t> b = testfiles::rsw();
    rsw::World w;
    std::string error;
    CHECK(rsw::parse(b.data(), b.size(), w, error));
    CHECK(w.waterKnown);
    CHECK(w.waterLevel == -5.0f && w.waveHeight == 1.0f && w.waveSpeed == 2.0f && w.wavePitch == 50.0f);
    CHECK_EQ(w.waterAnim, 3);
}
