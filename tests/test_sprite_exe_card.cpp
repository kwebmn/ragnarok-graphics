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
    glint::Vertex v[8];
    glint::star(100, 50, 0.3f, 0.9f, 20, 1.0f, v);
    CHECK(v[0].x == 90 && v[1].x == 110 && v[4].y == 40 && v[6].y == 60);
    CHECK((v[0].diffuse >> 24) == 255 && v[0].z == 0.3f && v[7].rhw == 0.9f);
}
