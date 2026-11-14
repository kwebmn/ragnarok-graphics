// 3.0.19-beta.4 (the owner: «давай» - a quest's step in the game's «Довідник»; the testers: «щоб відстежувалось
// від серва де ти, на чому застряг»): the site reads the character's quest variable (KielHyreQuest) on the server and
// says the step and whom to go to (GET /api/client/quests/progress?char=); the quest's card shows it
#include <string>
#include <vector>

#include "core/quest_progress.hpp"
#include "core/reference_pages.hpp"
#include "core/site_api.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

namespace {
const char* kAnswer = R"({"char":"Another","quests":[
  {"slug":"story-kiel-hyre","value":23,"started":true,"done":false,"step":10,"steps":30,
   "npc":{"name":"Cute Student","map":"kh_school","x":179,"y":39}},
  {"slug":"story-other","value":108,"started":true,"done":true,"step":30,"steps":30,"npc":null}]})";

const refpages::Row* rowWith(const refpages::Page& p, const std::string& left) {
    for (const refpages::Row& r : p.rows)
        if (r.left == left) return &r;
    return nullptr;
}
}  // namespace

TEST(quest_progress_the_sites_answer) {
    std::vector<questprogress::State> s;
    CHECK(questprogress::parse(kAnswer, s));
    CHECK_EQ(s.size(), size_t(2));
    const questprogress::State* k = questprogress::find(s, "story-kiel-hyre");
    CHECK(k != nullptr);
    if (k) {
        CHECK(k->started && !k->done && k->value == 23 && k->step == 10 && k->steps == 30);
        CHECK(k->npc == "Cute Student" && k->map == "kh_school" && k->x == 179 && k->y == 39);
    }
    const questprogress::State* o = questprogress::find(s, "story-other");
    CHECK(o && o->done && o->npc.empty());
    CHECK(questprogress::find(s, "none") == nullptr);
    CHECK(!questprogress::parse("<html>", s));
    CHECK(!questprogress::parse(R"({"error":"not found"})", s));
}

TEST(quest_progress_on_the_quest_card) {
    refindex::Index ix;
    refquests::Quest q;
    q.slug = "story-kiel-hyre";
    q.category = "story";
    q.title[refquests::Uk] = "Kiel Hyre Quest";
    for (int i = 1; i <= 30; ++i) q.steps[refquests::Uk].push_back("Крок номер " + std::to_string(i) + ".");
    ix.quests = {q};
    ix.maps["kh_school"].npcs = {refindex::Npc{176, 60, "Lady"}, refindex::Npc{179, 39, "Cute Student"}};
    std::vector<questprogress::State> states;
    CHECK(questprogress::parse(kAnswer, states));
    refpages::Context c;
    c.progress = &states;
    const refpages::Page p = refpages::quest(ix, Lang::Uk, 0, c);
    CHECK(rowWith(p, "Ваш прогрес") != nullptr);
    CHECK(rowWith(p, "Крок 10 з 30") != nullptr);
    CHECK(rowWith(p, "Крок номер 10.") != nullptr);
    const refpages::Row* next = rowWith(p, "Далі: Cute Student");
    CHECK(next != nullptr);
    if (next) {
        CHECK(next->link.go == refpages::Go::Npc && next->link.map == "kh_school" && next->link.id == 1);
        CHECK(next->right.find("179, 39") != std::string::npos);
    }
    // finished; not started; no word of the site - no block
    states[0].done = true;
    CHECK(rowWith(refpages::quest(ix, Lang::Uk, 0, c), "Завершено") != nullptr);
    states[0].done = false;
    states[0].started = false;
    states[0].step = 1;
    CHECK(rowWith(refpages::quest(ix, Lang::Uk, 0, c), "Ще не розпочато") != nullptr);
    c.progress = nullptr;
    CHECK(rowWith(refpages::quest(ix, Lang::Uk, 0, c), "Ваш прогрес") == nullptr);
}

// The request: the device's token, the character's name as UTF-8 bytes percent-encoded (Cyrillic, spaces, «!»)
TEST(quest_progress_the_request) {
    const siteapi::Request r = siteapi::questProgress("\xD0\xA7\xD1\x83\xD0\xBC\xD0\xB0 !");  // «Чума !»
    CHECK_EQ(r.method, std::string("GET"));
    CHECK(r.auth);
    CHECK_EQ(r.path, std::string("/api/client/quests/progress?char=%D0%A7%D1%83%D0%BC%D0%B0%20%21"));
    CHECK_EQ(siteapi::questProgress("Another_1.x-y~").path,
             std::string("/api/client/quests/progress?char=Another_1.x-y~"));
}
