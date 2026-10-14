#include <cmath>
#include <vector>

#include "core/actor_shadows.hpp"
#include "test.hpp"

using namespace uagfx;
using actor_shadows::Blob;
using actor_shadows::Layer;
using actor_shadows::ShadowQuad;

namespace {

// A sprite's layer: an upright quad from (x0, top) to (x1, bottom), game pixels, at view depth z.
Layer layer(float x0, float top, float x1, float bottom, float z, uint32_t texture = 7) {
    Layer l;
    const float xy[4][2] = {{x0, top}, {x1, top}, {x0, bottom}, {x1, bottom}};
    const float uv[4][2] = {{0.25f, 0.5f}, {0.5f, 0.5f}, {0.25f, 0.75f}, {0.5f, 0.75f}};
    for (int i = 0; i < 4; ++i) {
        l.corner[i][0] = xy[i][0];
        l.corner[i][1] = xy[i][1];
        l.uv[i][0] = uv[i][0];
        l.uv[i][1] = uv[i][1];
    }
    l.viewZ = z;
    l.alpha = 1.0f;
    l.texture = texture;
    return l;
}

}  // namespace

TEST(actor_shadows_lay_a_characters_layers_on_the_ground) {
    // 0.32: the game's shadow under a character (its feet at 200, 300) and the character's body above it: the body's
    // shadow lies from the feet along the light's direction on the screen, as long as half its height.
    std::vector<Blob> blobs = {Blob{200.0f, 300.0f, 20.0f, 8.0f, 150.0f}};
    std::vector<Layer> layers = {layer(180.0f, 200.0f, 220.0f, 300.0f, 150.5f)};
    const float dir[2] = {0.8f, 0.6f};
    const std::vector<ShadowQuad> q = actor_shadows::cast(blobs, layers, {}, dir, 0.5f);
    CHECK_EQ(q.size(), size_t(1));
    if (q.size() != 1) return;
    // The feet stay; the top - 100 pixels up - lies 50 along the direction from the feet.
    CHECK(std::fabs(q[0].corner[2][0] - 180.0f) < 1e-3f && std::fabs(q[0].corner[2][1] - 300.0f) < 1e-3f);
    CHECK(std::fabs(q[0].corner[0][0] - (180.0f + 40.0f)) < 1e-3f);
    CHECK(std::fabs(q[0].corner[0][1] - (300.0f + 30.0f)) < 1e-3f);
    CHECK(q[0].uv[0][0] == 0.25f && q[0].uv[3][1] == 0.75f);  // its own picture
    CHECK_EQ(q[0].texture, 7u);
    CHECK(std::fabs(q[0].viewZ - 150.0f) < 1e-3f);  // at the feet's depth
}

TEST(actor_shadows_take_only_layers_of_a_character_at_its_place) {
    std::vector<Blob> blobs = {Blob{200.0f, 300.0f, 20.0f, 8.0f, 150.0f}, Blob{400.0f, 250.0f, 20.0f, 8.0f, 90.0f}};
    std::vector<Layer> layers = {
        layer(185.0f, 220.0f, 215.0f, 260.0f, 151.0f),  // a head above the first character's feet
        layer(380.0f, 150.0f, 420.0f, 250.0f, 90.2f),   // the second character
        layer(600.0f, 150.0f, 640.0f, 250.0f, 90.0f),   // far to the side: none's
        layer(185.0f, 200.0f, 215.0f, 300.0f, 60.0f),   // in front, much nearer: another's, not this one's
    };
    const float dir[2] = {0.0f, 1.0f};
    const std::vector<ShadowQuad> q = actor_shadows::cast(blobs, layers, {}, dir, 0.5f);
    CHECK_EQ(q.size(), size_t(2));
    // Each character's own direction when it has one (away from a torch near it).
    const std::vector<std::array<float, 2>> dirs = {{1.0f, 0.0f}, {0.0f, 1.0f}};
    const std::vector<ShadowQuad> r = actor_shadows::cast(blobs, layers, dirs, dir, 0.5f);
    CHECK_EQ(r.size(), size_t(2));
    if (r.size() == 2) {
        // The head: from 80 above the feet to 40 above them - along +x, at the feet's line.
        CHECK(std::fabs(r[0].corner[0][1] - 300.0f) < 1e-3f && std::fabs(r[0].corner[0][0] - (185.0f + 40.0f)) < 1e-3f);
    }
}
