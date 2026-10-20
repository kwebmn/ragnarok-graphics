#include <cmath>
#include <vector>

#include "core/actor_shadows.hpp"
#include "core/config.hpp"
#include "core/game_camera.hpp"
#include "core/map_axes.hpp"
#include "core/rsw.hpp"
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

TEST(actor_shadows_keep_the_ground_flat_under_the_feet) {
    // 2.6.3: shadow_ps takes the ground's slope on the screen from the scene's depth 12 pixels below the feet. Near a
    // ledge that depth fell on the wall below it (deeper and deeper down the screen): the ground below the feet looked
    // far too deep and the shadow falling there was cut - it blinked as a character walked along the ledge
    // (Aldebaran, report R-9AFFAR). A post in front did the same to the shadow above the feet. Never deeper than the
    // flat ground through the feet now.
    const camera::Projection p = camera::projectionFor(1920, 1080);
    const float up[3] = {0.0f, -0.70710678f, -0.70710678f};  // the camera 45 degrees down
    const float feetZ = 300.0f, below = p.cy + 40.0f, above = p.cy - 40.0f;
    const float flatBelow = feetZ / (1.0f + 40.0f / p.f1), flatAbove = feetZ / (1.0f - 40.0f / p.f1);
    // off the wall below a ledge: +0.6 a pixel - 324 units 40 pixels down, the ground there 297
    CHECK(std::fabs(actor_shadows::groundDepth(p, up, p.cx, p.cy, feetZ, 0.6f, p.cx, below) - flatBelow) < 0.01f);
    // off a post in front: -0.6 a pixel - 324 units 40 pixels up, the ground there 303
    CHECK(std::fabs(actor_shadows::groundDepth(p, up, p.cx, p.cy, feetZ, -0.6f, p.cx, above) - flatAbove) < 0.01f);
    // a slope nearer than the flat ground (a hill rising there) stays
    CHECK(std::fabs(actor_shadows::groundDepth(p, up, p.cx, p.cy, feetZ, 0.02f, p.cx, above) - 299.2f) < 0.01f);
    // no camera known: the slope alone, as before
    const float none[3] = {0.0f, 0.0f, 0.0f};
    CHECK(std::fabs(actor_shadows::groundDepth(p, none, p.cx, p.cy, feetZ, 0.6f, p.cx, below) - 324.0f) < 0.01f);
    // the feet's own pixel: their depth
    CHECK(std::fabs(actor_shadows::groundDepth(p, up, p.cx + 10.0f, p.cy, feetZ, 0.6f, p.cx + 10.0f, p.cy) - feetZ) <
          0.01f);
}

TEST(actor_shadows_cast_one_shadow_for_each_light_of_a_character) {
    // 2.6.14: a character between two torches throws two shadows, each as dark as its light makes it; a character no
    // light reaches (underground, no torch near) throws none but the game's round one
    std::vector<Blob> blobs = {Blob{200.0f, 300.0f, 20.0f, 8.0f, 150.0f}, Blob{400.0f, 300.0f, 20.0f, 8.0f, 150.0f}};
    std::vector<Layer> layers = {layer(180.0f, 200.0f, 220.0f, 300.0f, 150.5f),
                                 layer(380.0f, 200.0f, 420.0f, 300.0f, 150.5f)};
    const std::vector<std::vector<actor_shadows::Cast>> casts = {
        {actor_shadows::Cast{{1.0f, 0.0f}, 0.5f, 0.9f}, actor_shadows::Cast{{0.0f, 1.0f}, 0.25f, 0.4f}}, {}};
    const std::vector<ShadowQuad> q = actor_shadows::cast(blobs, layers, casts);
    CHECK_EQ(q.size(), size_t(2));
    if (q.size() != 2) return;
    // the first along x, half the height (100 pixels up -> 50), 0.9 dark; the second down, a quarter, 0.4
    CHECK(std::fabs(q[0].corner[0][0] - 230.0f) < 1e-3f && std::fabs(q[0].corner[0][1] - 300.0f) < 1e-3f);
    CHECK(std::fabs(q[0].alpha - 0.9f) < 1e-4f);
    CHECK(std::fabs(q[1].corner[0][0] - 180.0f) < 1e-3f && std::fabs(q[1].corner[0][1] - 325.0f) < 1e-3f);
    CHECK(std::fabs(q[1].alpha - 0.4f) < 1e-4f);
    CHECK(q[0].feetX == 200.0f && q[1].feetX == 200.0f);  // both the first character's
}

TEST(actor_shadows_lit_by_the_two_nearest_lights_within_their_reach) {
    // 2.6.14: a torch 12 units above the floor, the feet 6 away: away from it, half the usual length (6 / 12), as dark
    // as can be (well within its reach); farther off longer and fainter; past its reach nothing
    using actor_shadows::Light;
    const std::vector<Light> one = {Light{0.0f, -12.0f, 0.0f, 40.0f, 0}};
    std::vector<actor_shadows::Lit> lit = actor_shadows::litBy(one, 6.0f, 0.0f, 0.0f, 0.0, 0.0f);
    CHECK_EQ(lit.size(), size_t(1));
    if (lit.size() != 1) return;
    CHECK(std::fabs(lit[0].awayX - 1.0f) < 1e-4f && std::fabs(lit[0].awayZ) < 1e-4f);
    CHECK(std::fabs(lit[0].length - 0.5f) < 1e-3f);
    CHECK(std::fabs(lit[0].weight - 1.0f) < 1e-4f);
    const std::vector<actor_shadows::Lit> far = actor_shadows::litBy(one, 0.0f, 0.0f, 30.0f, 0.0, 0.0f);
    CHECK_EQ(far.size(), size_t(1));
    if (far.size() == 1) {
        CHECK(std::fabs(far[0].awayZ - 1.0f) < 1e-4f);
        CHECK(std::fabs(far[0].length - 2.2f) < 1e-3f);  // 30 / 12 at most 2.2
        CHECK(far[0].weight > 0.05f && far[0].weight < 0.5f);
    }
    CHECK(actor_shadows::litBy(one, 41.0f, 0.0f, 0.0f, 0.0, 0.0f).empty());  // past its reach
    CHECK(actor_shadows::litBy(one, 0.5f, 0.0f, 0.0f, 0.0, 0.0f).empty());   // right under it: no way to fall
    // three lights about: the two nearest, the nearest first
    const std::vector<Light> three = {Light{-20.0f, -12.0f, 0.0f, 40.0f, 0}, Light{8.0f, -12.0f, 0.0f, 40.0f, 0},
                                      Light{0.0f, -12.0f, 10.0f, 40.0f, 0}};
    lit = actor_shadows::litBy(three, 0.0f, 0.0f, 0.0f, 0.0, 0.0f);
    CHECK_EQ(lit.size(), size_t(2));
    if (lit.size() == 2) {
        CHECK(std::fabs(lit[0].awayX + 1.0f) < 1e-4f);  // from the one 8 away
        CHECK(std::fabs(lit[1].awayZ + 1.0f) < 1e-4f);  // from the one 10 away
    }
}

TEST(actor_shadows_tremble_with_a_flame_a_lamp_steady) {
    // 2.6.14: a torch's shadow darkens and pales with its flame (its seed), a lamp's stays; never past 0-1
    using actor_shadows::Light;
    const std::vector<Light> torch = {Light{0.0f, -12.0f, 0.0f, 40.0f, 12345u}};
    const std::vector<Light> lamp = {Light{0.0f, -12.0f, 0.0f, 40.0f, 0}};
    float low = 2.0f, high = -1.0f;
    bool steady = true, within = true;
    const float lampAtFirst = actor_shadows::litBy(lamp, 20.0f, 0.0f, 0.0f, 0.0, 0.35f)[0].weight;
    for (int i = 0; i < 200; ++i) {
        const double t = i * 0.05;
        const std::vector<actor_shadows::Lit> a = actor_shadows::litBy(torch, 20.0f, 0.0f, 0.0f, t, 0.35f);
        const std::vector<actor_shadows::Lit> b = actor_shadows::litBy(lamp, 20.0f, 0.0f, 0.0f, t, 0.35f);
        if (a.size() != 1 || b.size() != 1) {
            within = false;
            break;
        }
        low = std::min(low, a[0].weight);
        high = std::max(high, a[0].weight);
        within = within && a[0].weight >= 0.0f && a[0].weight <= 1.0f;
        steady = steady && std::fabs(b[0].weight - lampAtFirst) < 1e-6f;
    }
    CHECK(within);
    CHECK(high - low > 0.05f);  // it trembles
    CHECK(steady);
}

namespace {

// A character at the screen's middle, 100 units in front of a camera looking along the world's z (up along -y)
struct Scene {
    camera::Projection p = camera::projectionFor(1920, 1080);
    float viewToWorld[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    Blob blob{0, 0, 20.0f, 8.0f, 100.0f};
    float feet[3] = {0, 0, 100.0f};
    Scene() {
        blob.x = p.cx;
        blob.y = p.cy + 100.0f;
        feet[1] = 100.0f * 100.0f / p.f1;
    }
    // a torch `left` units to the feet's left (world -x), 12 above them
    actor_shadows::Light torchAt(float left) const { return {feet[0] - left, feet[1] - 12.0f, feet[2], 40.0f, 0}; }
};

}  // namespace

TEST(actor_shadows_underground_from_the_fires_alone) {
    // 2.6.14: under the ground a character's shadow falls away from the torch beside it (to the right on the screen);
    // with no fire near none falls - no sun there, the game's round one alone
    const Scene s;
    const float sun[2] = {0.83f, 0.56f};
    const std::vector<actor_shadows::Light> lights = {s.torchAt(6.0f)};
    auto c = actor_shadows::castsFor({s.blob}, lights, s.p, s.viewToWorld, true, sun, 1.0f, false, 0.0f, 0.45f,
                                     0.0, 0.0f);
    CHECK_EQ(c.size(), size_t(1));
    if (c.size() != 1) return;
    CHECK_EQ(c[0].size(), size_t(1));
    if (c[0].size() == 1) {
        CHECK(c[0][0].dir[0] > 0.99f && std::fabs(c[0][0].dir[1]) < 0.05f);
        CHECK(std::fabs(c[0][0].length - 0.45f * 0.5f) < 1e-3f);  // 6 away, 12 above: half the usual
        CHECK(std::fabs(c[0][0].weight - 1.0f) < 1e-4f);
    }
    c = actor_shadows::castsFor({s.blob}, {}, s.p, s.viewToWorld, true, sun, 1.0f, false, 0.0f, 0.45f, 0.0, 0.0f);
    CHECK(c.size() == 1 && c[0].empty());
}

TEST(actor_shadows_under_the_sky_the_sun_by_day_the_fires_by_night) {
    // 2.6.14: by day the sun outshines a torch - its shadow alone, as ever; at night the torch's, the sun's gone; at
    // dusk both, each half; at night with no fire near none
    const Scene s;
    const float sun[2] = {0.83f, 0.56f};
    const std::vector<actor_shadows::Light> lights = {s.torchAt(6.0f)};
    auto day = actor_shadows::castsFor({s.blob}, lights, s.p, s.viewToWorld, true, sun, 1.2f, true, 0.0f, 0.45f, 0.0,
                                       0.0f);
    CHECK(day.size() == 1 && day[0].size() == 1);
    if (day.size() == 1 && day[0].size() == 1) {
        CHECK(day[0][0].dir[0] == 0.83f && day[0][0].dir[1] == 0.56f);
        CHECK(std::fabs(day[0][0].length - 0.45f * 1.2f) < 1e-4f && day[0][0].weight == 1.0f);
    }
    auto night = actor_shadows::castsFor({s.blob}, lights, s.p, s.viewToWorld, true, sun, 1.2f, true, 1.0f, 0.45f,
                                         0.0, 0.0f);
    CHECK(night.size() == 1 && night[0].size() == 1);
    if (night.size() == 1 && night[0].size() == 1) CHECK(night[0][0].dir[0] > 0.99f && night[0][0].weight > 0.99f);
    auto dusk = actor_shadows::castsFor({s.blob}, lights, s.p, s.viewToWorld, true, sun, 1.2f, true, 0.5f, 0.45f,
                                        0.0, 0.0f);
    CHECK(dusk.size() == 1 && dusk[0].size() == 2);
    if (dusk.size() == 1 && dusk[0].size() == 2)
        CHECK(std::fabs(dusk[0][0].weight - 0.5f) < 1e-4f && std::fabs(dusk[0][1].weight - 0.5f) < 1e-4f);
    auto dark = actor_shadows::castsFor({s.blob}, {}, s.p, s.viewToWorld, true, sun, 1.2f, true, 1.0f, 0.45f, 0.0,
                                        0.0f);
    CHECK(dark.size() == 1 && dark[0].empty());
}

TEST(actor_shadows_lights_of_a_map_file) {
    // 2.6.14: the map's torches (47, and 690, 691, 696 the game does not show) reach 40 units and tremble; its lamps
    // as far as their light (0.45 of their range, 15-70), steady; other effects none; in the game's world by its axes
    rsw::World w;
    rsw::Effect torch;
    torch.id = 47;
    torch.pos[0] = 10.0f;
    torch.pos[1] = -12.0f;
    torch.pos[2] = 20.0f;
    rsw::Effect smoke = torch;
    smoke.id = 44;
    w.effects = {torch, smoke};
    rsw::Light lamp;
    lamp.pos[0] = -5.0f;
    lamp.range = 100.0f;
    map::Axes axes;
    axes.x = -1;
    axes.z = 1;
    const std::vector<actor_shadows::Light> l = actor_shadows::lightsOf(w, {lamp}, axes);
    CHECK_EQ(l.size(), size_t(2));
    if (l.size() != 2) return;
    CHECK(l[0].x == -10.0f && l[0].y == -12.0f && l[0].z == 20.0f && l[0].reach == 40.0f && l[0].seed != 0);
    CHECK(l[1].x == 5.0f && std::fabs(l[1].reach - 45.0f) < 1e-4f && l[1].seed == 0);
}

TEST(actor_shadows_the_games_round_spot_gives_way_to_a_characters_own) {
    // 2.6.15: effects.actor_shadows_spot off - the game's round spot fades out as the character's own shadow comes on:
    // gone under a whole one, half under a half-faded one (a torch's far off), whole where none falls (underground
    // with no fire near); on - under everyone, as ever
    std::vector<Blob> blobs = {Blob{200.0f, 300.0f, 20.0f, 8.0f, 150.0f}, Blob{400.0f, 300.0f, 20.0f, 8.0f, 150.0f},
                               Blob{600.0f, 300.0f, 20.0f, 8.0f, 150.0f}};
    std::vector<Layer> layers = {layer(180.0f, 200.0f, 220.0f, 300.0f, 150.5f),
                                 layer(380.0f, 200.0f, 420.0f, 300.0f, 150.5f)};  // the third has no layer of its own
    const std::vector<std::vector<actor_shadows::Cast>> casts = {
        {actor_shadows::Cast{{1.0f, 0.0f}, 0.5f, 1.0f}}, {actor_shadows::Cast{{1.0f, 0.0f}, 0.5f, 0.5f}}, {}};
    const std::vector<ShadowQuad> q = actor_shadows::cast(blobs, layers, casts);
    CHECK(q.size() == 2 && q[0].blob == 0 && q[1].blob == 1);
    const std::vector<float> off = actor_shadows::spotShares(q, blobs.size(), false);
    CHECK(off.size() == 3);
    if (off.size() == 3) CHECK(off[0] == 0.0f && std::fabs(off[1] - 0.5f) < 1e-5f && off[2] == 1.0f);
    const std::vector<float> on = actor_shadows::spotShares(q, blobs.size(), true);
    CHECK(on.size() == 3 && on[0] == 1.0f && on[1] == 1.0f && on[2] == 1.0f);
}

TEST(actor_shadows_spot_kept_by_default) {
    // 2.6.15: the game's round spot stays under everyone unless the player turns it off
    CHECK(parseConfig("").effects.actorShadowsSpot);
    CHECK(!parseConfig("[effects]\nactor_shadows_spot = 0\n").effects.actorShadowsSpot);
}
