// 3.0.16-beta.2: [interface] damage_numbers - the game's numbers as words, rising and fading
#include "core/damage_numbers.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(damage_numbers_text_and_kinds) {
    CHECK(damagenum::kindOf(0x16) == damagenum::Kind::Critical && damagenum::kindOf(0x72) == damagenum::Kind::Heal);
    CHECK(damagenum::textOf(1234567, damagenum::Kind::Hit) == "1 234 567");
    CHECK(damagenum::textOf(45, damagenum::Kind::Heal) == "+45");
    CHECK(damagenum::rgbOf(0xFF00FF00, damagenum::Kind::Heal) == 0x00FF00);
}

TEST(damage_numbers_rise_and_fade) {
    const auto a = damagenum::poseAt(0, damagenum::Kind::Hit), b = damagenum::poseAt(900, damagenum::Kind::Hit);
    CHECK(b.rise > a.rise && a.opacity == 1.0f && b.opacity < 1.0f && b.opacity > 0.0f);
    CHECK(damagenum::poseAt(damagenum::kLifeMs, damagenum::Kind::Hit).opacity == 0.0f);
    const auto first = damagenum::poseAt(0, damagenum::Kind::Critical);
    CHECK(first.size > damagenum::poseAt(500, damagenum::Kind::Critical).size);
}
