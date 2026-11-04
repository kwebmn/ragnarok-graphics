// 3.0.11-beta.237 (the owner: «когда убиваешь моба … временно показывалось на сколько добавилось опыта и над мобом
// было типа + столько то опыта»): core/exp_gain - a kill of the character's and the experience it gave paired in
// either order, the floater over the monster rising and fading, the label by the bars; a gain with no kill the label
// alone
#include <string>

#include "core/exp_gain.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(exp_gain_a_kill_and_its_experience) {
    expgain::Tracker t;
    t.experience(1, 1000, 0);  // the first: the whole as it was
    t.experience(2, 500, 0);
    t.hit(77);
    CHECK(t.died(77) && !t.died(78));  // 78 not the character's
    t.killedAt(77, 400, 300, 1000);
    t.experience(1, 1250, 1050);  // base +250, job +40 - two packets
    t.experience(2, 540, 1060);
    t.step(1100);
    CHECK(t.floaters().size() == 1 && t.floaters()[0].base == 250 && t.floaters()[0].job == 40);
    CHECK(t.floaters()[0].x == 400 && t.label().base == 250 && t.label().ms == 1050);
    // the experience first, its kill after
    t.experience(1, 1300, 2000);
    t.hit(80);
    t.died(80);
    t.killedAt(80, 100, 100, 2200);
    t.step(2250);
    CHECK(t.floaters().size() == 2 && t.floaters()[1].base == 50);
    // a gain with no kill (the party's share): the label alone after a short wait
    t.experience(1, 1400, 5000);
    t.step(5100);
    CHECK(t.label().ms == 0 || t.label().base != 100);  // still waiting
    t.step(5500);
    CHECK(t.label().base == 100 && t.floaters().empty());  // the old floaters gone by now
    // a new level starts it over: no gain shown
    t.experience(1, 10, 6000);
    t.step(7000);
    CHECK(t.label().base == 100);
    t.step(9000);
    CHECK(t.label().ms == 0);
}

TEST(exp_gain_rising_fading_and_words) {
    const expgain::Floater f{0, 0, 10, 0, 1000};
    CHECK(expgain::rise(f, 1000) == 0 && expgain::rise(f, 1000 + expgain::kFloatMs) == expgain::kRise);
    CHECK(expgain::rise(f, 1400) > expgain::kRise * 0.25f);  // quick first
    CHECK(expgain::opacity(1000, 1500, expgain::kFloatMs, expgain::kFadeMs) == 1);
    CHECK(expgain::opacity(1000, 2300, expgain::kFloatMs, expgain::kFadeMs) < 1);
    CHECK(expgain::opacity(1000, 2700, expgain::kFloatMs, expgain::kFadeMs) == 0);
    CHECK(expgain::text(1234, 567, ',') == "+1\xC2\xA0" "234 Base \xC2\xB7 +567 Job");
    CHECK(expgain::text(0, 567, ',') == "+567 Job");
    CHECK(expgain::text(1234, 0, ',', 0.0005) == "+1\xC2\xA0" "234 Base (0,05\xC2\xA0%)");
}
