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
