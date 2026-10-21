#include <cmath>
#include "core/sprite_exe.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(sprite_a_card_on_the_ground) {
    // 0.95: data\sprite\아이템\이름없는카드.spr - every card on the ground
    CHECK(sprite_exe::cardOnGround("data\\sprite\\\xBE\xC6\xC0\xCC\xC5\xDB\\"
                                   "\xC0\xCC\xB8\xA7\xBE\xF8\xB4\xC2\xC4\xAB\xB5\xE5.spr"));
    CHECK(!sprite_exe::cardOnGround("data\\sprite\\\xBE\xC6\xC0\xCC\xC5\xDB\\apple.spr"));
    CHECK(!sprite_exe::cardOnGround("data\\sprite\\npc\\\xC4\xAB\xB5\xE5.spr"));  // not an item
    CHECK(!sprite_exe::cardOnGround(nullptr));
}

#include "core/glint.hpp"

TEST(glint_flares_now_and_then) {
    int lit = 0;
    for (int i = 0; i < 1800; ++i) lit += glint::flare(i / 1000.0, 7) > 0.0f ? 1 : 0;
    CHECK(lit > 300 && lit < 500);  // about a fifth of the time
    CHECK(glint::flare(0.0, 0) == 0.0f);
}

TEST(glint_a_sparkle_not_a_cross) {
    // 2.7.14 (the tester: «хрестиком белым, не красивый»): two even bars made a white cross. Four long rays and four
    // short diagonal ones, each bright at the middle and gone at its tip, over a soft round glow - one list of
    // triangles, at the card's depth.
    glint::Vertex v[glint::kSparkle];
    glint::sparkle(100, 50, 0.3f, 0.9f, 40, 1.0f, v);
    float farthest = 0.0f;
    int bright = 0, tips = 0, lit = 0, rim = 0, core = 0;
    float middle = 0.0f;
    bool depth = true;
    for (const glint::Vertex& p : v) {
        const float d = std::hypot(p.x - 100.0f, p.y - 50.0f);
        const uint32_t a = p.diffuse >> 24;
        farthest = std::fmax(farthest, d);
        if (d > 9.0f) (a == 0 ? tips : lit) += 1;  // past the glow: only the rays' tips, faded out
        if (d < 4.0f && a > 200) ++bright;         // the long rays' middles shine
        if (a > 200 && d > 0.5f) middle = std::fmax(middle, d);
        if (a == 0 && d > 4.7f && d < 4.9f) ++rim;  // 2.7.15: the glow 12 % of the size across its middle
        if (d < 0.01f && a >= 190 && a <= 192) ++core;  // at 75 %: at full it whited the card out
        depth = depth && p.z == 0.3f && p.rhw == 0.9f;
    }
    CHECK(farthest > 19.9f && farthest < 20.1f);  // tip to tip `size`
    CHECK(tips == 8 && lit == 0);                 // each ray's two tips
    CHECK(bright == 8);
    // 2.7.15 (on the bright paving of Prontera a thin needle): the long rays 8 % of the size wide at their middles
    CHECK(middle > 3.1f && middle < 3.3f);
    CHECK(rim == 16 && core == 8);
    CHECK(depth);
    glint::sparkle(100, 50, 0.3f, 0.9f, 40, 0.0f, v);  // not flaring: nothing shows
    bool dark = true;
    for (const glint::Vertex& p : v) dark = dark && (p.diffuse >> 24) == 0;
    CHECK(dark);
}

TEST(glint_an_orbs_head_is_its_newest_sprite) {
    // 2.7.25 (the user: «можно сделать так, чтобы тот шарик был в начале того синего шлейфа?»): of each caster's orb
    // sprites (2.7.27: those up in the air - the device leaves their shadows out) the newest is the head of its trail;
    // another caster far off its own head. 2.7.28 (the user: «камера под углом, и шарик сбегается со шлейфом в одном
    // месте, а в других местах крутится рядом»): the newest is the most opaque (its alpha 250, less by 3 a moment,
    // uaRO.exe 0x5CC5D0), not the biggest on the screen - an older sprite nearer the camera looked bigger
    glint::OrbHeads heads;
    for (int i = 7; i >= 1; --i)  // the older ones, behind the head round the hero, the nearest the biggest
        heads.note(400.0f - 9.0f * float(i), 300.0f + 4.0f * float(i), 0.5f, 0.9f, 70.0f + float(i),
                   (250.0f - 9.0f * float(i)) / 255.0f);
    heads.note(400.0f, 300.0f, 0.5f, 0.9f, 70.0f, 250.0f / 255.0f);  // the newest, on the far side
    heads.note(1100.0f, 320.0f, 0.5f, 0.9f, 70.0f, 0.6f);  // another caster's
    CHECK_EQ(heads.heads().size(), size_t(2));
    const glint::OrbHeads::Head& h = heads.heads()[0];
    CHECK(h.x == 400.0f && h.y == 300.0f && h.size == 70.0f);
    CHECK(std::fabs(heads.heads()[1].x - 1100.0f) < 1e-3f);
    heads.clear();
    CHECK(heads.heads().empty());
}

TEST(glint_an_orbs_head_a_small_ball_of_light) {
    // 2.7.25: a white core and a pale blue glow fading out, a third of the sprite's size across, at its depth
    glint::Vertex v[glint::kOrb];
    glint::orb(200, 100, 0.4f, 0.8f, 60, v);
    float farthest = 0.0f;
    int middle = 0, rim = 0;
    bool depth = true;
    for (const glint::Vertex& p : v) {
        const float d = std::hypot(p.x - 200.0f, p.y - 100.0f);
        const uint32_t a = p.diffuse >> 24;
        farthest = std::fmax(farthest, d);
        if (d < 0.01f && a > 230) ++middle;  // the core's middle near opaque white
        if (d > 9.9f && a == 0) ++rim;      // the glow gone at its edge
        depth = depth && p.z == 0.4f && p.rhw == 0.8f;
    }
    CHECK(farthest > 9.9f && farthest < 10.1f);  // the glow's radius a sixth of the size
    CHECK(middle == 12 && rim == 36);
    CHECK(depth);
}

