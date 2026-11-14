// 3.0.19-beta.19 (the testers' round 3: «у монка коли прожимаешь азура ... с далека то він просто підходить і касту
// нема»): the skill the client asks for on a target, how far apart by the server's walks, and the server's answer -
// used, cast begun, refused, or none (out of its range then) - lines for the pad's log
#include <string>

#include "pad/core/logic/skill_watch.hpp"
#include "test.hpp"

using namespace uapad;

namespace {
bool has(const std::optional<std::string>& s, const char* part) { return s && s->find(part) != std::string::npos; }
}  // namespace

TEST(skill_watch_tells_the_ask_and_the_server_using_it) {
    SkillWatch w;
    w.setSelf(1000);
    const auto asked = w.asked(271, "MO_EXTREMITYFIST", 5, 110000123, 4, 2, 1000);
    CHECK(has(asked, "MO_EXTREMITYFIST lv 5") && has(asked, "110000123") && has(asked, "4 cells") &&
          has(asked, "range 2"));
    CHECK(!w.used(271, 999, 1000 + 50));  // another's use of it: not ours
    const auto used = w.used(271, 1000, 1000 + 120);
    CHECK(has(used, "used") && has(used, "120 ms"));
    CHECK(!w.tick(1000 + 5000));  // answered: nothing more
}

TEST(skill_watch_tells_no_answer_once) {
    SkillWatch w;
    w.setSelf(1000);
    CHECK(w.asked(271, "MO_EXTREMITYFIST", 5, 110000123, 5, 2, 1000).has_value());
    CHECK(!w.tick(1000 + SkillWatch::kWaitMs - 1));
    const auto none = w.tick(1000 + SkillWatch::kWaitMs);
    CHECK(has(none, "no answer") && has(none, "MO_EXTREMITYFIST"));
    CHECK(!w.tick(1000 + 3 * SkillWatch::kWaitMs));
}

TEST(skill_watch_cast_begun_and_refused) {
    SkillWatch w;
    w.setSelf(1000);
    w.asked(28, "AL_HEAL", 10, 1000, 0, 9, 0);
    CHECK(has(w.castBegun(28, 1000, 300), "cast"));
    w.asked(271, "MO_EXTREMITYFIST", 5, 110000123, 1, 2, 5000);
    const auto r = w.refused(271, 6, 5040);
    CHECK(has(r, "refused") && has(r, "cause 6"));
    CHECK(!w.tick(5000 + 3 * SkillWatch::kWaitMs));  // answered
}

TEST(skill_watch_unknown_cells_and_range) {
    SkillWatch w;
    const auto asked = w.asked(5, "", 10, 110000123, -1, 0, 0);
    CHECK(has(asked, "skill 5 lv 10") && has(asked, "apart unknown") && !has(asked, "range"));
}

TEST(skill_watch_lines_have_a_limit) {
    SkillWatch w;
    w.setSelf(1000);
    int told = 0;
    for (uint32_t i = 0; i < 200; ++i)
        if (w.asked(5, "SM_BASH", 10, 110000123, 1, 1, i * 10)) ++told;
    CHECK(told == SkillWatch::kMostLines);
}
