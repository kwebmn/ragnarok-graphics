// 3.0.11-beta.278 (the owner's features 6 and 10): the hunt's numbers - damage a second, the skills' shares, the
// experience an hour across a level, zeny an hour, kills
#include "core/hunt_stats.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(hunt_stats_numbers) {
    hunt::Stats h;
    h.reset(1000);
    h.status(50, 900, 1000, 10000, 1000);  // 90 %
    for (int i = 0; i < 30; ++i) h.hit(100, i % 3 == 0 ? 5 : 0, 1000 + uint64_t(i) * 1000);  // 30 s: 100 a second
    h.status(51, 200, 2000, 19000, 1800 * 1000 + 1000);  // half an hour: 10 % of 50 + 10 % of 51
    h.kill("Orc Warrior");
    h.kill("Orc Warrior");
    h.kill("Orc Archer");
    const hunt::View v = h.view(31000);
    CHECK(v.dps10 > 90 && v.dps10 < 110 && v.skills.size() == 2 && v.skills.front().first == 0);
    CHECK(v.skills.front().second == 67 || v.skills.front().second == 66);
    const hunt::View late = h.view(1800 * 1000 + 1000);
    CHECK(late.levelPerHour > 39 && late.levelPerHour < 41);  // 20 % in half an hour
    CHECK(late.zenyPerHour == 18000 && late.kills == 3 && late.killed.front().first == "Orc Warrior");
    CHECK(late.secondsToLevel > 0);
    CHECK(hunt::duration(3725) == "1:02:05" && hunt::duration(65) == "1:05" && hunt::grouped(45000) == "45 000");
    const hunt::Texts t{"Полювання · {1}", "DPS {1} · за хв {2}", "удар", "Досвід {1} %/год", "до рівня {1}",
                        "Зені {1}/год", "Убито {1}: {2}", "\xE2\x80\x94"};
    const std::vector<std::string> l = hunt::lines(late, t, {"", "Bash"});
    CHECK(l.size() == 6 && l[0] == "Полювання · 30:00" && l[4] == "Зені +18 000/год");
    CHECK(l[5] == "Убито 3: Orc Warrior 2 \xC2\xB7 Orc Archer 1");
}
