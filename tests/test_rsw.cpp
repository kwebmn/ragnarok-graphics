#include <cstring>
#include <string>
#include <vector>

#include "core/rsw.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A map file of version 2.1 with one object of each kind.
struct Writer {
    std::vector<uint8_t> b;
    void bytes(const void* p, size_t n) { b.insert(b.end(), static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + n); }
    void i32(int32_t v) { bytes(&v, 4); }
    void f32(float v) { bytes(&v, 4); }
    void text(const char* s, size_t n) {
        std::vector<char> z(n, 0);
        std::memcpy(z.data(), s, std::min(n, std::strlen(s)));
        bytes(z.data(), n);
    }
};

std::vector<uint8_t> sample() {
    Writer w;
    w.bytes("GRSW", 4);
    const uint8_t version[2] = {2, 1};
    w.bytes(version, 2);
    for (int i = 0; i < 4; ++i) w.text("x", 40);  // ini, gnd, gat, src
    w.f32(-5.0f);                                  // water: level, type, wave height, speed, pitch, animation
    w.i32(4);
    w.f32(1.0f);
    w.f32(2.0f);
    w.f32(50.0f);
    w.i32(3);
    w.i32(45);  // light: longitude, latitude, diffuse, ambient, opacity
    w.i32(45);
    for (int i = 0; i < 6; ++i) w.f32(0.5f);
    w.f32(1.0f);
    for (int i = 0; i < 4; ++i) w.i32(0);  // the ground box
    w.i32(4);                              // objects
    w.i32(1);                              // a model
    w.text("tree", 40);
    w.i32(0);
    w.f32(1.0f);
    w.i32(0);
    w.text("tree.rsm", 80);
    w.text("node", 80);
    for (int i = 0; i < 9; ++i) w.f32(1.0f);
    w.i32(2);  // a light
    w.text("lamp", 80);
    w.f32(10.0f);
    w.f32(-20.0f);
    w.f32(30.0f);
    w.f32(1.0f);
    w.f32(0.5f);
    w.f32(0.25f);
    w.f32(120.0f);
    w.i32(3);  // a sound
    w.text("s", 80);
    w.text("s.wav", 80);
    for (int i = 0; i < 3; ++i) w.f32(0.0f);
    w.f32(1.0f);
    w.i32(10);
    w.i32(10);
    w.f32(50.0f);
    w.f32(4.0f);  // its cycle (2.0 and later)
    w.i32(4);     // an effect
    w.text("torch", 80);
    w.f32(-100.0f);
    w.f32(-12.0f);
    w.f32(200.0f);
    w.i32(690);
    w.f32(0.0f);
    for (int i = 0; i < 4; ++i) w.f32(float(i));
    return w.b;
}

}  // namespace

TEST(rsw_lights_and_effects_of_a_map) {
    // 0.25: the map file's point lights and effects, past its models and sounds.
    const std::vector<uint8_t> b = sample();
    rsw::World w;
    std::string error;
    CHECK(rsw::parse(b.data(), b.size(), w, error));
    CHECK(error.empty());
    CHECK_EQ(w.waterType, 4);  // 0.31: lava
    CHECK_EQ(w.lights.size(), size_t(1));
    CHECK_EQ(w.effects.size(), size_t(1));
    CHECK_EQ(w.models.size(), size_t(1));  // 2.0: the model's file and place
    CHECK(w.lightKnown && w.lightLongitude == 45 && w.lightLatitude == 45);  // 2.1.3: the map's sun
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
    std::vector<uint8_t> b = sample();
    b.resize(b.size() - 30);  // cut inside the effect: what came before it stays
    CHECK(rsw::parse(b.data(), b.size(), w, error));
    CHECK_EQ(w.lights.size(), size_t(1));
    CHECK(w.effects.empty());
}
