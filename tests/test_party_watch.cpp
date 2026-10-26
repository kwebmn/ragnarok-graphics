#include <string>
#include <vector>

#include "core/party_watch.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.7-beta.6 (the user's notification 6): one of the party near dead, or under the threshold - told once each time
TEST(party_watch_a_death_and_low_hp_once_each) {
    party::Watch w;
    w.members({{2000123, "Sora"}, {2000456, "Priest"}}, 2000123);
    std::vector<party::News> news;
    w.hp(2000456, 4000, 4000, 30, news);
    CHECK(news.empty());
    w.hp(2000456, 1000, 4000, 30, news);  // 25 %: low
    CHECK(news.size() == 1 && news[0].kind == party::News::Low && news[0].name == "Priest" && news[0].percent == 25);
    news.clear();
    w.hp(2000456, 900, 4000, 30, news);  // still low: once
    CHECK(news.empty());
    w.hp(2000456, 1500, 4000, 30, news);  // 37 %: over, but not well over the threshold yet - still the same low
    w.hp(2000456, 1000, 4000, 30, news);
    CHECK(news.empty());
    w.hp(2000456, 2000, 4000, 30, news);  // 50 %: healed
    w.hp(2000456, 800, 4000, 30, news);   // low again: told again
    CHECK(news.size() == 1 && news[0].kind == party::News::Low && news[0].percent == 20);
    news.clear();
    w.hp(2000456, 0, 4000, 30, news);  // dead
    CHECK(news.size() == 1 && news[0].kind == party::News::Dead && news[0].name == "Priest");
    news.clear();
    w.hp(2000456, 0, 4000, 30, news);  // still dead: once
    CHECK(news.empty());
    w.died(2000456, news);  // the unit's death seen too: the same death
    CHECK(news.empty());
    w.hp(2000456, 4000, 4000, 30, news);  // up again
    w.died(2000456, news);                 // dead again, seen by its unit first
    CHECK(news.size() == 1 && news[0].kind == party::News::Dead);
}

TEST(party_watch_only_the_party_and_not_the_character_itself) {
    party::Watch w;
    w.members({{2000123, "Sora"}, {2000456, "Priest"}}, 2000123);
    std::vector<party::News> news;
    w.hp(2000123, 0, 4000, 30, news);  // the character itself: its own death is told apart
    w.died(2000123, news);
    w.hp(2000999, 0, 4000, 30, news);  // not of the party
    w.died(2000999, news);
    CHECK(news.empty());
    w.joined(2000789, "Sage");
    w.hp(2000789, 10, 100, 30, news);  // the most over 32767: per cents (10 of 100)
    CHECK(news.size() == 1 && news[0].name == "Sage" && news[0].percent == 10);
    news.clear();
    w.left(2000789);
    w.hp(2000789, 0, 100, 30, news);
    CHECK(news.empty());
    w.members({}, 2000123);  // the party gone
    w.hp(2000456, 0, 4000, 30, news);
    CHECK(news.empty());
}
