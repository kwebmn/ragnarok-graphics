// 3.0.11-beta.282 (the owner's feature 18): the healer's alarm
#include "core/party_alarm.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(party_alarm_falls_under_30) {
    partyalarm::Watch w;
    CHECK(!w.step({{1, 80}, {2, 10}}, 0));   // first seen: nothing
    CHECK(!w.step({{1, 30}, {2, 5}}, 100));
    CHECK(w.step({{1, 29}, {2, 5}}, 200));
    CHECK(w.pulse(1, 200) > 0.9f);
    CHECK(w.pulse(2, 200) == 0.0f);
    CHECK(!w.step({{1, 20}, {2, 5}}, 300));  // still under: once
    CHECK(!w.step({{1, -1}}, 3300));
    CHECK(w.pulse(1, 3300) == 0.0f);
    CHECK(!w.step({{1, 50}}, 3400));
    CHECK(w.step({{1, 10}}, 3500));          // back up and down again
}
