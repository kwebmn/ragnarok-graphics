// 3.0.11-beta.16: the site's quests in «Довідник» (core/reference_quests) - their manifest, the file, the pages.
#include <string>
#include <vector>

#include "core/reference_pages.hpp"
#include "core/reference_quests.hpp"
#include "core/sha256.hpp"
#include "core/tracker_panel.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

namespace {

const char* kQuests = R"({"v":1,"quests":[
 ["access-ayothaya",{"ua":"Доступ до Ayothaya","ru":"Доступ в Ayothaya","en":"Ayothaya Access"},"access",0,
  "Powerful-Looking Woman","ayothaya",{"ua":"Тайське місто за морем.","en":"A town over the sea."},
  {"ua":["Дістанься до Alberta.","Заплати 10 000 zeny."],"en":["Reach Alberta.","Pay 10,000 zeny."]},
  [[7288,1]],{"ua":["Доступ до Ayothaya."]}],
 ["bunny-band",{"ua":"Bunny Band","en":"Bunny Band"},"headgear",10,"Kafra","prontera",{},{"en":["Bring feathers."]},
  [[949,100],[7288,2]],{}],
 ["nameless",{},"misc",0,"","",{},{},[],{}]]})";

refindex::Index index() {
    refindex::Index ix;
    CHECK(refindex::build(R"({"v":1,"data":1,"commit":"c","date":"d","items":{"7288":["Old Ring","Old_Ring",3,0,1,0,0],
        "949":["Feather","Feather",3,4,1,0,0]},"mobs":{},"spawns":{},"routes":[],"shops":[],"npcs":[],"pets":[],
        "craft":[],"flags":{}})", ix));
    CHECK(refquests::read(kQuests, ix.quests));
    for (size_t q = 0; q < ix.quests.size(); ++q)
        for (const auto& [item, n] : ix.quests[q].items) ix.questsOf[item].push_back(int(q));
    return ix;
}

size_t indexOf(const refpages::Page& p, const std::string& left) {
    for (size_t i = 0; i < p.rows.size(); ++i)
        if (p.rows[i].left == left) return i;
    return size_t(-1);
}

}  // namespace

TEST(reference_quests_manifest_and_file) {
    const std::string file = "some gzip bytes";
    refquests::Manifest m;
    const std::string sha = sha256::hexOf(file);
    CHECK(refquests::parse("{\"v\":1,\"sha256\":\"" + sha + "\",\"size\":15,\"count\":60,\"url\":\"https://x\"}", m));
    CHECK_EQ(m.count, 60);
    CHECK_EQ(refquests::fileName(m), "quests-" + sha.substr(0, 12) + ".json.gz");
    CHECK_EQ(refquests::sitePath(m), "/api/public/game/quests-" + sha.substr(0, 12) + ".json.gz");
    CHECK(refquests::verify(file, m));
    CHECK(!refquests::verify(file + "!", m));
    refquests::Manifest back;
    CHECK(refquests::parse(refquests::write(m), back) && back.sha256 == sha && back.size == 15);
    CHECK(!refquests::needs(m, back, 15));
    CHECK(refquests::needs(m, back, 14));
    CHECK(refquests::needs(m, std::nullopt, -1));
    CHECK(!refquests::parse("{\"v\":2,\"sha256\":\"" + sha + "\",\"size\":15}", m));
    CHECK(!refquests::parse("{\"v\":1,\"sha256\":\"xyz\",\"size\":15}", m));
    std::vector<refquests::Quest> q;
    CHECK(refquests::read(kQuests, q));
    CHECK_EQ(q.size(), size_t(2));  // a quest without a title left out
    CHECK_EQ(q[0].steps[refquests::Uk].size(), size_t(2));
    CHECK_EQ(refquests::textOf(q[1].title, refquests::Ru), std::string("Bunny Band"));  // no Russian: English
    CHECK_EQ(refquests::listOf(q[1].steps, refquests::Uk).front(), std::string("Bring feathers."));
}

TEST(reference_quests_pages) {
    const refindex::Index ix = index();
    const refpages::Page home = refpages::home(ix, Lang::Uk);
    const size_t tile = indexOf(home, "Квести");  // 3.0.11-beta.26: its name, its count at its right
    CHECK(tile != size_t(-1) && home.rows[tile].right == "2" && home.rows[tile].link.go == refpages::Go::Quests);
    const refpages::Page list = refpages::quests(ix, Lang::Uk);
    CHECK(list.rows[0].style == refpages::Style::Heading && list.rows[0].left == "Доступ до міст");
    CHECK_EQ(list.rows[1].left, std::string("Доступ до Ayothaya"));
    CHECK_EQ(list.rows[1].right, std::string("ayothaya"));
    CHECK_EQ(list.rows[3].right, std::string("від Lv 10 · prontera"));
    const refpages::Page q = refpages::quest(ix, Lang::Uk, 0);
    CHECK_EQ(q.rows[0].left, std::string("Доступ до Ayothaya"));
    CHECK(q.rows[2].style == refpages::Style::Text && q.rows[2].left == "Тайське місто за морем.");
    const size_t steps = indexOf(q, "Кроки");
    CHECK(steps != size_t(-1) && q.rows[steps + 1].left == "1. Дістанься до Alberta.");
    const size_t items = indexOf(q, "Що потрібно");
    CHECK(q.rows[items + 1].left == "Old Ring" && q.rows[items + 1].right == "×1");
    CHECK_EQ(q.rows[items + 1].icon, std::string("item:7288"));
    const size_t npc = indexOf(q, "Хто дає");
    CHECK(q.rows[npc + 1].left == "Powerful-Looking Woman" && q.rows[npc + 1].link.map == "ayothaya");
    const refpages::Page ring = refpages::item(ix, Lang::Uk, 7288, refpages::Context{});
    const size_t needed = indexOf(ring, "Для квестів");
    CHECK(needed != size_t(-1) && ring.rows[needed].right == "2");
    CHECK(ring.rows[needed + 2].left == "Bunny Band" && ring.rows[needed + 2].right == "×2");
    CHECK(ring.rows[needed + 2].link.go == refpages::Go::Quest && ring.rows[needed + 2].link.id == 1);
    const refpages::Page found = refpages::found(ix, Lang::Uk, "bunny");
    CHECK(indexOf(found, "Квести · 1") != size_t(-1));
    const refpages::Page en = refpages::found(ix, Lang::Uk, "access");  // English titles found too
    CHECK(indexOf(en, "Доступ до Ayothaya") != size_t(-1));
    // 3.0.11-beta.28: found by its English title - that title first in its line, the query marked there
    const refpages::Row& access = en.rows[indexOf(en, "Доступ до Ayothaya")];
    CHECK(access.style == refpages::Style::Result && access.icon == "ui:quest" && access.right == "квест");
    CHECK(access.sub.rfind("Ayothaya Access · ", 0) == 0 && access.marks.empty());
    CHECK((access.subMarks == refpages::Marks{{9, 15}}));
}

// 3.0.11-beta.70 (the concept: «Единственная кнопка — «✓ Відстежується»: закрепить квест в панели отслеживания»): the
// quest card's pin at its end, its state from the tracked
TEST(reference_quests_track) {
    const refindex::Index ix = index();
    refpages::Context c;
    const refpages::Page q = refpages::quest(ix, Lang::Uk, 1, c);
    const refpages::Row& pin = q.rows.back();
    CHECK(pin.style == refpages::Style::Button && pin.left == "Відстежувати" && !pin.gold);
    CHECK(pin.link.go == refpages::Go::Track && pin.link.id == 0 && pin.link.map == "bunny-band");
    const std::vector<std::string> tracked{"bunny-band"};
    c.trackedQuests = &tracked;
    const refpages::Row on = refpages::quest(ix, Lang::Uk, 1, c).rows.back();
    CHECK(on.left == "\xE2\x9C\x93 Відстежується" && on.gold && on.link.go == refpages::Go::Track);
    CHECK(refpages::quest(ix, Lang::Uk, 0, c).rows.back().left == "Відстежувати");  // another quest: not tracked
}

// 3.0.11-beta.70: the tracker's entry of a pinned quest - its title, «квест», what to bring (no counts: the owner's),
// whom to give it; a slug the pack has not: none
TEST(reference_quests_tracked_entries) {
    const refindex::Index ix = index();
    const std::vector<tracker::Entry> e = refpages::trackedEntries(ix, Lang::Uk, {"bunny-band", "gone-quest"}, {});
    CHECK_EQ(e.size(), size_t(1));
    if (e.empty()) return;
    CHECK(e[0].kind == tracker::Kind::Quest && e[0].title == "Bunny Band" && e[0].tag == "квест");
    CHECK_EQ(e[0].lines.size(), size_t(3));
    if (e[0].lines.size() != 3) return;
    CHECK(e[0].lines[0].text == "Feather" && e[0].lines[0].right == "\xC3\x97" "100");
    CHECK(e[0].lines[1].text == "Old Ring" && e[0].lines[1].right == "\xC3\x97" "2");
    CHECK(e[0].lines[2].strong && e[0].lines[2].text.rfind("Здати: Kafra", 0) == 0);
}
