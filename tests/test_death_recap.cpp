// 3.0.11-beta.277 (the owner's feature 8): the last hits before a death, the last first, 10 s back
#include "core/death_recap.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(death_recap_last_hits) {
    deathrecap::Log log;
    log.hit({1, "Poring", 0, 5, 1000});
    for (int i = 0; i < 12; ++i) log.hit({2, "Orc Warrior", i % 2 ? 5 : 0, 100 + i, 20000 + uint64_t(i) * 100});
    const std::vector<deathrecap::Hit> hits = log.before(21200);
    CHECK(hits.size() == 10 && hits.front().damage == 111 && hits.back().damage == 102);  // the Poring's long gone
    CHECK(deathrecap::total(hits) == 102 + 103 + 104 + 105 + 106 + 107 + 108 + 109 + 110 + 111);
    const deathrecap::Texts t{"удар", "{1} с тому"};
    CHECK(deathrecap::line(hits.front(), 21200, "Bash", t, ',') ==
          "Orc Warrior \xC2\xB7 Bash \xC2\xB7 111 \xC2\xB7 0,1 с тому");
    CHECK(deathrecap::line({7, "", 0, 9, 21000}, 21200, "", t, ',') ==
          "#7 \xC2\xB7 удар \xC2\xB7 9 \xC2\xB7 0,2 с тому");
    CHECK(log.before(60000).empty());
}
