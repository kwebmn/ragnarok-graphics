#include <cmath>
#include <vector>

#include "core/actor_shadows.hpp"
#include "test.hpp"

using namespace uagfx;
using actor_shadows::Blob;
using actor_shadows::Layer;
using actor_shadows::ShadowQuad;

namespace {

// A sprite's layer: an upright quad from (x0, top) to (x1, bottom), game pixels, at view depth z - its top as deep,
// unless topZ says otherwise.
Layer layer(float x0, float top, float x1, float bottom, float z, uint32_t texture = 7, float topZ = -1.0f) {
    Layer l;
    const float xy[4][2] = {{x0, top}, {x1, top}, {x0, bottom}, {x1, bottom}};
    const float uv[4][2] = {{0.25f, 0.5f}, {0.5f, 0.5f}, {0.25f, 0.75f}, {0.5f, 0.75f}};
    for (int i = 0; i < 4; ++i) {
        l.corner[i][0] = xy[i][0];
        l.corner[i][1] = xy[i][1];
        l.uv[i][0] = uv[i][0];
        l.uv[i][1] = uv[i][1];
    }
    l.bottomZ = z;
    l.topZ = topZ < 0.0f ? z : topZ;
    l.alpha = 1.0f;
    l.texture = texture;
    return l;
}

// A layer of a character whose feet are at feetY: its depth there `atFeet`, its top nearer by 0.16 units a pixel (as
// uaRO.exe tilts a sprite), drawn for the character of the game's seed `owner`.
Layer standing(float x0, float top, float x1, float bottom, float feetY, float atFeet, uint8_t owner) {
    Layer l = layer(x0, top, x1, bottom, atFeet + (bottom - feetY) * 0.16f, 7, atFeet + (top - feetY) * 0.16f);
    l.owner = owner;
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
    CHECK(q[0].feetX == 200.0f && q[0].feetY == 300.0f);
}

TEST(actor_shadows_know_a_tall_sprites_depth_at_its_feet) {
    // 0.39.1: uaRO.exe tilts a sprite's depth with its height (0.16 units a pixel), its top nearer than its feet: the
    // depth at the feet is found along that tilt, not as the corners' mean - tall characters got no shadow.
    std::vector<Blob> blobs = {Blob{200.0f, 300.0f, 20.0f, 8.0f, 150.0f}};
    std::vector<Layer> layers = {
        layer(180.0f, 200.0f, 220.0f, 302.0f, 150.3f, 7, 150.3f - 102.0f * 0.16f),  // a body, 102 pixels tall
        layer(185.0f, 190.0f, 215.0f, 240.0f, 150.0f - 60.0f * 0.16f, 8,
              150.0f - 110.0f * 0.16f),  // a head on it: 60-110 pixels above the feet
        layer(185.0f, 190.0f, 215.0f, 240.0f, 120.0f, 9, 112.0f),  // another's head, much nearer
    };
    const float dir[2] = {0.8f, 0.6f};
    const std::vector<ShadowQuad> q = actor_shadows::cast(blobs, layers, {}, dir, 0.5f);
    CHECK_EQ(q.size(), size_t(2));
    // A body a little lower than the feet (its frame's margin) still stands on them.
    std::vector<Layer> low = {layer(180.0f, 200.0f, 220.0f, 320.0f, 150.0f, 7, 150.0f - 120.0f * 0.16f)};
    CHECK_EQ(actor_shadows::cast(blobs, low, {}, dir, 0.5f).size(), size_t(1));
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

TEST(actor_shadows_know_a_characters_layers_by_its_shadow) {
    // 2.6.3: uaRO.exe draws each clip of a character nearer than its feet by the count of clips drawn before it
    // (FUN_005ae540: 9e-6 more 1/z each at 300 units away - 0.8 units) and by the screen's height (3.6e-5 x the
    // sprite's scale: 4.7 units at 1080p): body 3.8 units nearer than its round shadow, head 4.6, a hat 5.4, a weapon
    // 6.3. Within 5 units of the feet two of them stood on them, and the head went when the weapon was drawn first (a
    // turn while walking). The game asks for a character's shadow right before its sprites: they carry its seed.
    std::vector<Blob> blobs = {Blob{200.0f, 300.0f, 20.0f, 8.0f, 300.0f, 41}};
    std::vector<Layer> layers = {
        standing(180.0f, 200.0f, 220.0f, 302.0f, 300.0f, 300.0f - 3.8f, 41),  // body
        standing(185.0f, 190.0f, 215.0f, 240.0f, 300.0f, 300.0f - 4.6f, 41),  // head
        standing(185.0f, 180.0f, 215.0f, 215.0f, 300.0f, 300.0f - 5.4f, 41),  // hat
        standing(170.0f, 210.0f, 215.0f, 290.0f, 300.0f, 300.0f - 6.3f, 41),  // weapon
    };
    const float dir[2] = {0.8f, 0.6f};
    const std::vector<ShadowQuad> all = actor_shadows::cast(blobs, layers, {}, dir, 0.5f);
    CHECK_EQ(all.size(), size_t(4));
    for (const ShadowQuad& s : all) CHECK(s.owned);
    // The weapon drawn first: the others one clip further - all still the character's.
    std::vector<Layer> turned = {
        standing(170.0f, 210.0f, 215.0f, 290.0f, 300.0f, 300.0f - 3.8f, 41),
        standing(180.0f, 200.0f, 220.0f, 302.0f, 300.0f, 300.0f - 4.6f, 41),
        standing(185.0f, 190.0f, 215.0f, 240.0f, 300.0f, 300.0f - 5.4f, 41),
    };
    CHECK_EQ(actor_shadows::cast(blobs, turned, {}, dir, 0.5f).size(), size_t(3));
    // Not a sprite drawn over everything (the game's nearest depth, 10): no character stands there.
    std::vector<Layer> over = {standing(180.0f, 200.0f, 220.0f, 302.0f, 300.0f, 10.0f, 41)};
    CHECK_EQ(actor_shadows::cast(blobs, over, {}, dir, 0.5f).size(), size_t(0));
}

TEST(actor_shadows_take_a_layer_to_its_own_characters_feet) {
    // 2.6.3: two characters, the second just behind the first (10 pixels up, 3 units deeper); the second's head along
    // its tilt is at the first one's depth there - by depth alone it stood on the first one's feet.
    std::vector<Blob> blobs = {Blob{200.0f, 300.0f, 20.0f, 8.0f, 300.0f, 41},
                               Blob{205.0f, 290.0f, 20.0f, 8.0f, 303.0f, 77}};
    std::vector<Layer> layers = {standing(190.0f, 180.0f, 220.0f, 230.0f, 290.0f, 303.0f - 4.6f, 77)};
    const float dir[2] = {0.0f, 1.0f};
    const std::vector<ShadowQuad> q = actor_shadows::cast(blobs, layers, {}, dir, 0.5f);
    CHECK_EQ(q.size(), size_t(1));
    if (q.size() == 1) CHECK(q[0].feetX == 205.0f && q[0].feetY == 290.0f);
    // Without its character's shadow - none of that seed, or its feet not under it (another character after one with
    // a shadow) - a layer stands on the feet at its depth, as before.
    std::vector<Layer> lone = {standing(185.0f, 190.0f, 215.0f, 240.0f, 300.0f, 300.0f - 2.0f, 99),
                               standing(185.0f, 190.0f, 215.0f, 240.0f, 300.0f, 300.0f - 2.0f, 77)};
    std::vector<Blob> first = {blobs[0], Blob{600.0f, 290.0f, 20.0f, 8.0f, 303.0f, 77}};
    const std::vector<ShadowQuad> r = actor_shadows::cast(first, lone, {}, dir, 0.5f);
    CHECK_EQ(r.size(), size_t(2));
    if (r.size() == 2) CHECK(r[0].feetX == 200.0f && r[1].feetX == 200.0f && !r[0].owned && !r[1].owned);
    // By depth, the nearest of the feet under it.
    std::vector<Blob> two = {Blob{200.0f, 300.0f, 20.0f, 8.0f, 300.0f}, Blob{200.0f, 300.0f, 20.0f, 8.0f, 303.0f}};
    std::vector<Layer> unknown = {standing(185.0f, 190.0f, 215.0f, 240.0f, 300.0f, 302.5f, 0)};
    const std::vector<ShadowQuad> n = actor_shadows::cast(two, unknown, {}, dir, 0.5f);
    CHECK_EQ(n.size(), size_t(1));
    if (n.size() == 1) CHECK(n[0].viewZ == 303.0f);
}
