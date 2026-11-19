// 3.0.11-beta.165 (task 6 of the remaining windows: the guild, Alt+G): the game's guild window (its tabs are UI windows
// 0x3C-0x41) drawn by the driver. 3.0.22-beta.26 (docs/superpowers/plans/2026-11-19-guild-window.md, task 3): rebuilt
// as «Навички» - the tabs' views from the game's guild object, its members' list and the server's packets, the
// window's hit test over RmlUi's boxes
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "core/client_libs.hpp"
#include "core/guild_card.hpp"
#include "test.hpp"

using namespace uagfx;
namespace gc = uagfx::guildcard;
namespace gs = uagfx::guildskills;

namespace {

gc::Texts texts() {
    gc::Texts t;
    t.none = "немає";
    t.infoSub = "Рівень {1} · {2} з {3} учасників · {4} у грі";
    t.expLine = "Досвід {1} з {2} · {3} % до {4}-го рівня";
    t.master = "Майстер";
    t.average = "Середній рівень";
    t.castles = "Замки";
    t.yourTax = "Ваш податок";
    t.yourTaxValue = "{1} % досвіду · посада «{2}»";
    t.online = "У грі зараз · {1}";
    t.offline = "Не в грі — {1}";
    t.nearMaster = "коли майстер поруч";
    t.warOnly = "на війні гільдій, 1 хв";
    t.count = "{1} з {2}";
    t.colName = "Ім'я";
    t.colJob = "Клас";
    t.colLevel = "Рів.";
    t.colPosition = "Посада";
    t.colGiven = "Внесок";
    t.inGame = "у грі";
    t.away = "не в грі";
    t.givenValue = "{1} досвіду гільдії · {2}-й серед {3}";
    t.whisper = "Написати";
    t.party = "Запросити в групу";
    t.friendly = "У друзі";
    t.setPosition = "Посада: {1}";
    t.expel = "Вигнати з гільдії…";
    t.invite = "Запрошення";
    t.expelRight = "Вигнання";
    t.noRights = "без прав";
    t.folded = "Ще {1} посад без людей";
    t.peopleOn = "На посаді {1}";
    const char* groups[4] = {"Замок", "Розвиток гільдії", "Аури майстра", "На війні гільдій"};
    for (int i = 0; i < 4; ++i) t.groups[i] = groups[i];
    t.kinds[0] = "Пасивна";
    t.kinds[1] = "Аура";
    t.kinds[2] = "Активна";
    t.sp = "{1} SP";
    for (int i = 0; i < 14; ++i) t.lines[i] = "line" + std::to_string(i), t.about[i] = "about" + std::to_string(i);
    t.youNear = "{1}, коли майстер поруч";
    t.whereAura = "aura where";
    t.whereWar = "war where";
    t.coolShared = "5 хв, спільна";
    t.ready = "Готово";
    t.wait = "ще {1}";
    t.coolRow = "перезарядка, ще {1}";
    t.levelOf = "рівень {1} з {2}";
    t.raiseTo = "Підняти до {1}";
    t.places = "{1} місць";
    t.need = "Потрібно: {1}";
    t.points = "Вільних очок навичок: {1}";
    t.memberFoot = "Піднімає майстер гільдії";
    t.skillHints = "Клацніть|двічі";
    t.counter = "{1} з {2}";
    t.whoRaisesText = "Майстер гільдії — очками рівнів гільдії";
    t.padTabs = "Вкладки";
    t.padClose = "Закрити";
    return t;
}

std::vector<gs::Sent> sent() {
    return {{10000, 1, 0, 0, false}, {10004, 10, 0, 0, false}, {10006, 2, 0, 0, true}, {10010, 1, 4, 1, false},
            {10011, 1, 4, 1, true}};
}

gc::Member member(const char* name, int level, bool online, uint32_t character, const char* title = "") {
    gc::Member m;
    m.name = name;
    m.level = level;
    m.online = online;
    m.character = character;
    m.job = "Knight";
    m.title = title;
    return m;
}

guildtabs::Member listed(const char* name, uint32_t character, uint32_t exp, int position) {
    guildtabs::Member x;
    x.name = name;
    x.character = character;
    x.exp = exp;
    x.position = position;
    return x;
}

}  // namespace

// The guild's windows (FUN_004d42d0 0x3C-0x41): where the manager keeps each (+0x170 on, 4 apart)
TEST(guild_card_the_games_windows) {
    CHECK(gc::kWindows == 6 && gc::kFirstAt == 0x170 && gc::kCloseButton == 0x87);
    CHECK(gc::allowed(0, 0) && !gc::allowed(1, 0) && gc::allowed(1, 0x57) && !gc::allowed(5, 0x57));
    CHECK(gc::allowed(5, 0xD7) && gc::isMaster(0xD7) && !gc::isMaster(0x57));
    CHECK(gc::gameDrawn(int(gc::Tab::Positions), true) && !gc::gameDrawn(int(gc::Tab::Notice), true));
}

TEST(guild_card_castles_in_the_servers_words) {
    CHECK_EQ(gc::castles("None Taken"), 0);
    CHECK_EQ(gc::castles("One Castle"), 1);
    CHECK_EQ(gc::castles("Two Castles"), 2);
    CHECK_EQ(gc::castles("Twenty Three Castles"), 23);
    CHECK_EQ(gc::castles("Total Domination"), 24);
    CHECK_EQ(gc::castles("Prontera"), -1);
}

TEST(guild_card_its_size_and_the_smallest_box_under_the_mouse) {
    gc::Layout L = gc::layout(1920, 1080, 1.0f);
    CHECK_EQ(L.w, 760);
    CHECK(L.h > 760 && L.y + L.h <= 1080 && L.x + L.w <= 1920);
    L.hits = {{gc::Part::Skill, 10011, panel::Box{10, 100, 400, 38}},
              {gc::Part::Plus, 10011, panel::Box{380, 106, 24, 24}}};
    CHECK(gc::hitTest(L, 390, 110).part == gc::Part::Plus);
    const gc::Hit row = gc::hitTest(L, 50, 110);
    CHECK(row.part == gc::Part::Skill && row.index == 10011);
    CHECK(gc::hitTest(L, 700, 700).part == gc::Part::Body);
    CHECK(gc::hitTest(L, -3, 5).part == gc::Part::None);
    const gc::Layout big = gc::layout(1920, 1080, 1.5f);
    CHECK_EQ(big.w, 1140);
}

TEST(guild_card_skills_view_master_and_member) {
    const gc::Texts t = texts();
    gc::Show sh;
    sh.skill = 10011;
    const std::vector<gs::Row> rows = gs::rows(sent(), 2, true, 192000);
    gc::SkillsView v = gc::skillsView(rows, sh, t, true);
    CHECK_EQ(v.rows.size(), size_t(14));
    CHECK_EQ(v.rows[0].group, std::string("Замок"));
    CHECK(v.rows[1].group.empty());
    CHECK_EQ(v.rows[0].aegis, std::string("gd_approval"));
    const auto row = [&](int id) -> const gc::SkillRowV& {
        return *std::find_if(v.rows.begin(), v.rows.end(), [&](const gc::SkillRowV& r) { return r.id == id; });
    };
    CHECK(row(10008).locked);
    CHECK_EQ(row(10008).info, std::string("Потрібно: Glorious Wounds 1"));
    CHECK_EQ(row(10010).info, std::string("перезарядка, ще 3:12"));  // the four war skills wait together
    CHECK_EQ(row(10006).info, std::string("line6"));
    CHECK_EQ(row(10006).dots.size(), size_t(5));
    CHECK(row(10004).dots.empty() && row(10004).level == "10/10");
    CHECK(row(10011).focus && row(10011).plus);
    CHECK_EQ(v.side.name, std::string("Regeneration"));
    CHECK_EQ(v.side.kind, std::string("Активна · 1 SP · рівень 1 з 3"));
    CHECK_EQ(v.side.levels.size(), size_t(3));
    CHECK(v.side.levels[0].now);
    CHECK(v.side.use && v.side.useOff && !v.side.ready);
    CHECK_EQ(v.side.wait, std::string("ще 3:12"));
    CHECK_EQ(v.side.raiseWord, std::string("Підняти до 2"));
    CHECK_EQ(v.side.needs.size(), size_t(3));
    CHECK(v.side.who.empty() && v.side.you.empty());
    sh.skill = 10006;  // a member: what the aura gives them
    v = gc::skillsView(gs::rows(sent(), 2, false, 0), sh, t, false);
    CHECK_EQ(v.side.you, std::string("+2 STR, коли майстер поруч"));
    CHECK(!v.side.use && !v.side.raise && !v.side.who.empty());
    sh.skill = 12345;  // gone: the first one chosen
    v = gc::skillsView(gs::rows(sent(), 2, false, 0), sh, t, false);
    CHECK(v.rows[0].focus && v.side.id == 10000);
}

TEST(guild_card_info_view) {
    const gc::Texts t = texts();
    gc::Info g;
    g.name = "Bastion";
    g.master = "Kael";
    g.territory = "Two Castles";
    g.level = 23;
    g.most = 76;
    g.average = 81;
    g.exp = 302410000;
    g.nextExp = 464000000;
    const std::vector<gc::Member> ms = {member("Kael", 99, true, 1), member("Mirra", 97, true, 2),
                                        member("Odin", 99, false, 3)};
    guildtabs::Data d;
    d.members = {listed("Kael", 1, 100, 0), listed("Mirra", 2, 50, 3), listed("Odin", 3, 10, 3)};
    d.membersSeen = true;
    d.positions = {{0, "Майстер", true, true, 0}, {3, "Воїн", true, false, 10}};
    d.alliances = {{false, 7, "Nightfall"}, {true, 8, "Red Lotus"}};
    d.alliancesSeen = true;
    d.subject = "Похід";
    gc::Show sh;
    sh.rights = 0x57;
    sh.me = "Mirra";
    const std::vector<gs::Row> skills = gs::rows(sent(), 0, false, 0);
    const gc::InfoView v = gc::infoView(g, ms, d, skills, sh, t);
    CHECK_EQ(v.sub, std::string("Рівень 23 · 3 з 76 учасників · 2 у грі"));
    CHECK_EQ(v.expLine, std::string("Досвід 302 410 000 з 464 000 000 · 65 % до 24-го рівня"));
    CHECK(std::abs(v.expShare - 0.6517f) < 0.001f);
    CHECK_EQ(v.facts.size(), size_t(4));
    CHECK(v.facts[0].value == "Kael" && v.facts[0].dot);
    CHECK_EQ(v.facts[2].value, std::string("2"));
    CHECK_EQ(v.facts[3].value, std::string("10 % досвіду · посада «Воїн»"));
    CHECK(v.allies.size() == 1 && v.enemies.size() == 1 && v.enemies[0] == "Red Lotus");
    CHECK_EQ(v.subject, std::string("Похід"));
    CHECK_EQ(v.gifts.size(), size_t(3));  // Great Leadership's aura + Battle Orders + Regeneration
    CHECK_EQ(v.gifts[0].what, std::string("+2 STR"));
    CHECK_EQ(v.online.size(), size_t(2));
    CHECK_EQ(v.offline, std::string("Не в грі — 1"));
    sh.rights = 0xD7;  // the master gets no aura
    CHECK_EQ(gc::infoView(g, ms, d, skills, sh, t).gifts.size(), size_t(2));
}

TEST(guild_card_members_in_the_game_first_then_by_position_and_level) {
    const gc::Texts t = texts();
    gc::Info g;
    g.most = 76;
    const std::vector<gc::Member> ms = {member("Zed", 50, false, 1), member("Ann", 90, true, 2),
                                        member("Bob", 95, true, 3), member("Kael", 99, true, 4)};
    guildtabs::Data d;
    d.members = {listed("Zed", 1, 0, 3), listed("Ann", 2, 300, 1), listed("Bob", 3, 100, 3), listed("Kael", 4, 600, 0)};
    d.membersSeen = true;
    d.positions = {{0, "Майстер", true, true, 0}, {1, "Капітан", true, true, 5}, {3, "Воїн", true, false, 10}};
    gc::Show sh;
    std::vector<size_t> o = gc::memberOrder(ms, d, sh);
    CHECK(o.size() == 4 && o[0] == 3 && o[1] == 1 && o[2] == 2 && o[3] == 0);
    sh.sort = gc::Sort::Given;
    o = gc::memberOrder(ms, d, sh);
    CHECK(o[0] == 3 && o[1] == 1 && o[2] == 2);
    sh.sort = gc::Sort::Online;
    sh.search = "a";  // Ann, Kael (the letters small)
    CHECK_EQ(gc::memberOrder(ms, d, sh).size(), size_t(2));
    sh.search = "B";
    CHECK_EQ(gc::memberOrder(ms, d, sh).size(), size_t(1));
    sh.search.clear();
    sh.member = 2;
    gc::MembersView v = gc::membersView(g, ms, d, sh, t, true);
    CHECK_EQ(v.count, std::string("4 з 76"));
    CHECK_EQ(v.rows.size(), size_t(4));
    CHECK_EQ(v.rows[0].given, std::string("60 %"));
    CHECK(v.rows[1].focus && v.card.name == "Ann");
    CHECK_EQ(v.card.given, std::string("30 % досвіду гільдії · 2-й серед 4"));
    CHECK_EQ(v.card.actions.size(), size_t(5));  // the master: «Посада» and «Вигнати» too
    CHECK_EQ(v.card.actions[3].second, std::string("Посада: Капітан"));
    CHECK_EQ(v.card.actions[4].first, int(gc::Act::Expel));
    CHECK_EQ(gc::membersView(g, ms, d, sh, t, true, 0x5).card.actions.size(), size_t(2));  // what the window can do
    CHECK(v.heads[0].on && v.heads[0].name.find("\xE2\x86\x93") != std::string::npos);
    sh.me = "Ann";  // oneself: no actions
    CHECK(gc::membersView(g, ms, d, sh, t, true).card.actions.empty());
    sh.me.clear();
    CHECK_EQ(gc::membersView(g, ms, d, sh, t, false).card.actions.size(), size_t(3));
    guildtabs::Data none;  // 0x0154 not come yet: the given unknown
    CHECK_EQ(gc::membersView(g, ms, none, sh, t, false).rows[0].given, std::string("\xE2\x80\x94"));
}

TEST(guild_card_positions_fold_the_empty_ones) {
    const gc::Texts t = texts();
    guildtabs::Data d;
    for (int i = 0; i < 20; ++i) d.positions.push_back({i, "Position " + std::to_string(i + 1), i < 2, i == 0, i * 2});
    d.members = {listed("A", 1, 0, 0), listed("B", 2, 0, 1), listed("C", 3, 0, 1)};
    d.membersSeen = true;
    gc::Show sh;
    sh.position = 1;
    sh.rights = 0xD7;
    const gc::PositionsView v = gc::positionsView(d, sh, t);
    CHECK_EQ(v.rows.size(), size_t(2));
    CHECK_EQ(v.folded, std::string("Ще 18 посад без людей"));
    CHECK(v.rows[1].focus && v.rows[1].people == "2");
    CHECK_EQ(v.sidePeople, std::string("На посаді 2"));
    CHECK(v.canEdit);
    sh.rights = 0x57;
    CHECK(!gc::positionsView(d, sh, t).canEdit);
}

TEST(guild_card_notice_view_and_the_foot) {
    const gc::Texts t = texts();
    guildtabs::Data d;
    d.subject = "Hi";
    gc::Show sh;
    sh.rights = 0xD7;
    gc::NoticeView n = gc::noticeView(d, sh, t, 2, 0);
    CHECK(!n.editing && n.canEdit && n.subject == "Hi" && n.subjectCount == "2 з 60" && n.bodyCount == "0 з 120");
    sh.editing = true;
    sh.subject = "New";
    n = gc::noticeView(d, sh, t, 3, 0);
    CHECK(n.editing && n.subject == "New");
    sh.tab = gc::Tab::Skills;
    gc::FootV f = gc::footOf(sh, t, 2, true);
    CHECK(f.left == "Вільних очок навичок: 2" && f.gold && f.hints.size() == 2 && f.pad.empty());
    sh.pad = true;
    f = gc::footOf(sh, t, 0, false);
    CHECK(f.left == "Піднімає майстер гільдії" && !f.gold && f.hints.empty() && f.pad.size() == 3);
}


namespace {
// A 24 x 24 24-bit BMP as the game's emblems are: magenta all round, a red square in the middle (8..15), bottom up
std::string emblemBmp() {
    const int w = 24, h = 24, row = w * 3;
    std::string b(54 + row * h, '\0');
    auto put32 = [&](size_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i) b[at + size_t(i)] = char((v >> (8 * i)) & 0xFF);
    };
    b[0] = 'B', b[1] = 'M';
    put32(2, uint32_t(b.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, uint32_t(w));
    put32(22, uint32_t(h));
    b[26] = 1;
    b[28] = 24;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const bool red = x >= 8 && x < 16 && y >= 8 && y < 16;
            const size_t at = 54 + size_t(y) * size_t(row) + size_t(x) * 3;  // B G R
            b[at] = char(red ? 0 : 255), b[at + 1] = 0, b[at + 2] = char(255);
        }
    return b;
}
}  // namespace

// 3.0.19-beta.1: the guild's emblem from the game's own file (<client>\_tmpEmblem\<server>_<guild>_<emblem>.ebm - the
// packet 0x0152's data as the server sent it: the BMP zlib-packed) - unpacked, its magenta see-through, made twice as
// big pixel for pixel; a plain BMP read as it is; anything else nothing
TEST(guild_card_emblem_from_the_games_file) {
    const std::string bmp = emblemBmp();
    std::vector<unsigned char> packed(bmp.size() + 64);
    unsigned long len = static_cast<unsigned long>(packed.size());
    CHECK_EQ(client_libs::zlibCompress(packed.data(), &len, reinterpret_cast<const unsigned char*>(bmp.data()),
                                      static_cast<unsigned long>(bmp.size())),
             0);
    const std::string ebm(reinterpret_cast<const char*>(packed.data()), len);
    for (const std::string* file : {&ebm, &bmp}) {
        const std::vector<uint32_t> px = guildcard::emblemPixels(*file, 48);
        CHECK_EQ(px.size(), size_t(48 * 48));
        if (px.size() != size_t(48 * 48)) continue;
        CHECK_EQ(px[0] >> 24, 0u);                          // magenta - see-through
        CHECK_EQ(px[size_t(24 * 48 + 24)], 0xFFFF0000u);    // the red middle, opaque
        CHECK_EQ(px[size_t(16 * 48 + 16)], 0xFFFF0000u);    // its corner: 8 * 2
        CHECK_EQ(px[size_t(15 * 48 + 15)] >> 24, 0u);       // just outside it
    }
    CHECK(guildcard::emblemPixels("not an emblem", 48).empty());
    CHECK(guildcard::emblemPixels(ebm, 0).empty());
}
