// 3.0.22-beta.26 (docs/superpowers/plans/2026-11-19-guild-window.md, task 5): «Гільдія» by hand in RmlUi as
// «Навички» - laid out on the host with the real theme: its size, every pressable part's box for core guildcard's hit
// test, the lists inside their places, shorter lists without RmlUi's errors
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/guild_live.hpp"
#include "uikit/hits.hpp"

using namespace uagfx;
namespace gc = uagfx::guildcard;
namespace gs = uagfx::guildskills;
namespace gl = uagfx::uikit::guild;

namespace {

gc::Texts texts() {
    gc::Texts t;
    const char* tabs[gc::kTabs] = {"Інформація", "Учасники", "Посади", "Навички", "Вигнані", "Оголошення"};
    for (int i = 0; i < gc::kTabs; ++i) t.tabs[i] = tabs[i];
    const char* groups[4] = {"Замок", "Розвиток гільдії", "Аури майстра", "На війні гільдій"};
    const char* more[4] = {"", "", "— 2 клітинки довкола, поки майстер іде", "— активні, спільна перезарядка 5 хв"};
    for (int i = 0; i < 4; ++i) t.groups[i] = groups[i], t.groupsMore[i] = more[i];
    t.kinds[0] = "Пасивна";
    t.kinds[1] = "Аура";
    t.kinds[2] = "Активна";
    t.sp = "{1} SP";
    for (int i = 0; i < 14; ++i) {
        t.lines[i] = "+1 STR за рівень своїм поруч з майстром";
        t.about[i] = "Своїм у 15 клітинках від майстра HP і SP відновлюються швидше впродовж 1 хвилини.";
    }
    t.levelOf = "рівень {1} з {2}";
    t.coolRow = "перезарядка, ще {1}";
    t.wait = "ще {1}";
    t.ready = "Готово";
    t.use = "Використати";
    t.raiseTo = "Підняти до {1}";
    t.need = "Потрібно: {1}";
    t.places = "{1} місць";
    t.whereWar = "Лише на картах війни гільдій: guild_vs1–5 будь-коли, замки — під час облоги.";
    t.whereAura = "Будь-де, коли майстер іде поруч.";
    t.coolShared = "5 хв, спільна для всіх чотирьох активних.";
    t.levelsLabel = "Рівні";
    t.needsLabel = "Потрібно";
    t.coolLabel = "Перезарядка";
    t.whereLabel = "Де діє";
    t.count = "{1} з {2}";
    t.search = "Пошук за ім'ям";
    t.colName = "Ім'я";
    t.colJob = "Клас";
    t.colLevel = "Рів.";
    t.colPosition = "Посада";
    t.colGiven = "Внесок";
    t.colRights = "Права";
    t.colTax = "Податок";
    t.colPeople = "Людей";
    t.whisper = "Написати";
    t.party = "Запросити в групу";
    t.friendly = "У друзі";
    t.setPosition = "Посада: {1}";
    t.expel = "Вигнати з гільдії…";
    t.counter = "{1} з {2}";
    t.save = "Зберегти";
    t.cancel = "Скасувати";
    t.edit = "Змінити";
    t.points = "Вільних очок навичок: {1}";
    t.skillHints = "Клацніть — опис праворуч|двічі — використати|перетягніть на панель клавіш";
    return t;
}

gl::Frame frameOf(gc::Tab tab, float scale, int members, bool master = true) {
    const gc::Texts t = texts();
    gc::Show sh;
    sh.tab = tab;
    sh.rights = master ? 0xD7 : 0x57;
    sh.skill = 10011;
    const std::vector<gs::Sent> sent = {{10000, 1, 0, 0, true}, {10004, 10, 0, 0, false}, {10006, 1, 0, 0, true},
                                        {10010, 1, 4, 1, false}, {10011, 1, 4, 1, true}};
    gl::Frame f;
    f.scale = scale;
    f.name = "Bastion";
    f.sub = "рівень 23 · у грі 9 з 37";
    f.hotkey = "Alt+G";
    f.tab = tab;
    for (int i = 0; i < gc::kTabs; ++i)
        f.tabs.push_back(gl::TabV{t.tabs[i], i == 1 ? "37" : "", i == 3 ? "2" : "", i == int(tab), false, false});
    f.skills = gc::skillsView(gs::rows(sent, 2, master, 192000), sh, t, master);
    gc::Info g;
    g.name = "Bastion";
    g.most = 76;
    std::vector<gc::Member> ms;
    guildtabs::Data d;
    for (int i = 0; i < members; ++i) {
        gc::Member m;
        m.name = "Member" + std::to_string(i);
        m.job = "Assassin Cross";
        m.level = 99 - i;
        m.online = i % 3 == 0;
        m.character = uint32_t(1000 + i);
        ms.push_back(m);
        guildtabs::Member x;
        x.name = m.name;
        x.character = m.character;
        x.exp = uint32_t(100 * (i + 1));
        x.position = i % 4;
        d.members.push_back(x);
    }
    d.membersSeen = true;
    d.positions = {{0, "Майстер", true, true, 0}, {1, "Капітан", true, true, 5}, {2, "Воїн", true, false, 10},
                   {3, "Новобранець", false, false, 10}};
    f.info = gc::infoView(g, ms, d, gs::rows(sent, 2, master, 0), sh, t);
    f.members = gc::membersView(g, ms, d, sh, t, master);
    f.positions = gc::positionsView(d, sh, t);
    f.expelled = gc::expelledView(d, sh, t);
    sh.editing = tab == gc::Tab::Notice;
    f.notice = gc::noticeView(d, sh, t, 10, 20);
    f.foot = gc::footOf(sh, t, 2, master);
    return f;
}

int countOf(const gc::Layout& L, gc::Part part) {
    int n = 0;
    for (const gc::HitBox& b : L.hits) n += b.part == part ? 1 : 0;
    return n;
}

std::string iconOf(const std::string& aegis, int side, float& w) {
    w = float(side);
    return "pic:" + aegis;
}

}  // namespace

TEST(uikit_guild_skills_tab_laid_out_with_every_part_pressable) {
    static gl::Model m;
    gl::fill(m, frameOf(gc::Tab::Skills, 1, 5), iconOf, [](int) { return std::string("pic:emblem"); }, nullptr);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 300, 100, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    const gc::Layout core = gc::layout(1920, 1080, 1);
    gc::Layout L = core;
    CHECK(gl::refit(d, L));
    std::printf("  guild window %d x %d (core %d x %d)\n", L.w, L.h, core.w, core.h);
    CHECK(std::abs(L.w - 760) <= 2);
    CHECK(std::abs(L.h - core.h) <= 3);
    CHECK_EQ(countOf(L, gc::Part::Tab), gc::kTabs);
    CHECK_EQ(countOf(L, gc::Part::Skill), 14);
    CHECK(countOf(L, gc::Part::Plus) >= 4);
    CHECK_EQ(countOf(L, gc::Part::Use), 1);
    CHECK_EQ(countOf(L, gc::Part::Raise), 1);
    CHECK_EQ(countOf(L, gc::Part::Close), 1);
    const std::vector<uikit::Placed> area = uikit::placesOf(d, "area"), side = uikit::placesOf(d, "side");
    CHECK(area.size() == 1 && side.size() == 1);
    if (area.size() == 1 && side.size() == 1)
        for (const gc::HitBox& b : L.hits) {  // the lists inside their places, every row whole
            if (b.part == gc::Part::Skill) CHECK(b.box.y + b.box.h <= area[0].box.y + area[0].box.h + 0.5f);
            if (b.part == gc::Part::Use || b.part == gc::Part::Raise)
                CHECK(b.box.x >= side[0].box.x && b.box.x + b.box.w <= side[0].box.x + side[0].box.w + 0.5f);
        }
    for (const gc::HitBox& b : L.hits)
        if (b.part == gc::Part::Plus) {
            const gc::Hit h = gc::hitTest(L, b.box.x + b.box.w / 2, b.box.y + b.box.h / 2);
            CHECK(h.part == gc::Part::Plus && h.index == b.index);  // over its row, the smaller box wins
        }
    rmlhost::close(d);
}

TEST(uikit_guild_members_tab_twenty_rows_its_heads_and_the_card) {
    static gl::Model m;
    gl::fill(m, frameOf(gc::Tab::Members, 1, 37), iconOf, nullptr, nullptr);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 300, 100, &error);
    CHECK_EQ(error, std::string());
    gc::Layout L = gc::layout(1920, 1080, 1);
    CHECK(gl::refit(d, L));
    CHECK_EQ(countOf(L, gc::Part::Member), gc::kMemberRows);
    CHECK_EQ(countOf(L, gc::Part::Head), 4);  // the class is no sort
    CHECK_EQ(countOf(L, gc::Part::Search), 1);
    CHECK_EQ(countOf(L, gc::Part::Action), 5);  // the master's: «Посада», «Вигнати» too
    const std::vector<uikit::Placed> area = uikit::placesOf(d, "area");
    if (area.size() == 1)
        for (const gc::HitBox& b : L.hits)
            if (b.part == gc::Part::Member) CHECK(b.box.y + b.box.h <= area[0].box.y + area[0].box.h + 0.5f);
    rmlhost::close(d);
}

TEST(uikit_guild_info_positions_expelled_and_the_notice_load) {
    for (gc::Tab tab : {gc::Tab::Info, gc::Tab::Positions, gc::Tab::Expelled, gc::Tab::Notice}) {
        static gl::Model m;
        m = gl::Model{};
        gl::fill(m, frameOf(tab, 1, 6), iconOf, [](int) { return std::string("pic:emblem"); }, nullptr);
        std::string error;
        Rml::ElementDocument* d =
            rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 300, 100, &error);
        CHECK_EQ(error, std::string());
        gc::Layout L = gc::layout(1920, 1080, 1);
        CHECK(gl::refit(d, L));
        if (tab == gc::Tab::Info) CHECK_EQ(countOf(L, gc::Part::AllMembers), 1);
        if (tab == gc::Tab::Positions) CHECK_EQ(countOf(L, gc::Part::Position), 4);
        if (tab == gc::Tab::Notice) {
            CHECK_EQ(countOf(L, gc::Part::Field), 2);
            CHECK_EQ(countOf(L, gc::Part::Save), 1);
            CHECK_EQ(countOf(L, gc::Part::Cancel), 1);
        }
        rmlhost::close(d);
    }
}

// The lists shorter after a change (another tab, fewer members): RmlUi must not read past them
TEST(uikit_guild_shorter_lists_without_errors) {
    static gl::Model m;
    gl::fill(m, frameOf(gc::Tab::Members, 1, 37), iconOf, nullptr, nullptr);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 300, 100, &error);
    CHECK_EQ(error, std::string());
    const size_t logBefore = rmlhost::log().size();
    gl::fill(m, frameOf(gc::Tab::Members, 1, 3), iconOf, nullptr, nullptr);
    rmlhost::update();
    gl::fill(m, frameOf(gc::Tab::Skills, 1, 3, false), iconOf, nullptr, nullptr);
    rmlhost::update();
    const std::string added = rmlhost::log().substr(logBefore);
    if (!added.empty()) std::printf("RmlUi said: %s\n", added.c_str());
    CHECK(added.find("out of bounds") == std::string::npos);
    gc::Layout L = gc::layout(1920, 1080, 1);
    CHECK(gl::refit(d, L));
    CHECK_EQ(countOf(L, gc::Part::Use), 0);  // a member: no buttons
    rmlhost::close(d);
}

TEST(uikit_guild_its_size_follows_the_scale) {
    static gl::Model m;
    gl::fill(m, frameOf(gc::Tab::Skills, 1.5f, 5), iconOf, nullptr, nullptr);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 100, 50, &error);
    CHECK_EQ(error, std::string());
    gc::Layout L = gc::layout(1920, 1080, 1.5f);
    CHECK(gl::refit(d, L));
    CHECK(std::abs(L.w - 1140) <= 3);
    rmlhost::close(d);
}

// 3.0.22-beta.27: Alt+G with no guild - two cards a row (how to join, how to make one), only its head pressable
TEST(uikit_guild_no_guild_two_cards) {
    static gl::Model m;
    gl::Frame f = frameOf(gc::Tab::Info, 1, 0);
    f.noGuild = true;
    f.name = "Гільдія";
    f.ngHead = "Ви не в гільдії";
    f.ngJoin = "Вступити";
    f.ngJoinText = "Попросіть майстра чи офіцера гільдії запросити вас.";
    f.ngMake = "Створити свою";
    f.ngMakeText = "Потрібен Emperium у речах. У чаті напишіть /guild Назва.";
    gl::fill(m, f, iconOf, nullptr, nullptr);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 300, 100, &error);
    CHECK_EQ(error, std::string());
    gc::Layout L = gc::layout(1920, 1080, 1);
    CHECK(gl::refit(d, L));
    std::printf("  no guild: %d x %d\n", L.w, L.h);
    CHECK(std::abs(L.w - 600) <= 2);
    CHECK(L.h < 400);
    CHECK_EQ(countOf(L, gc::Part::Tab), 0);
    CHECK_EQ(countOf(L, gc::Part::Close), 1);
    rmlhost::close(d);
}

// 3.0.24-beta.36 (the laptop's run, Alt+G): RmlUi warned «Data array index out of bounds», «Could not get value from
// data variable 'members.heads[2].on'» - the markup reads the tables' heads by their places before the first fill; the
// heads are there from the model's making (five and four)
TEST(uikit_guild_heads_there_before_the_first_fill) {
    static gl::Model m;
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 300, 100, &error);
    rmlhost::update();
    CHECK_EQ(error, std::string());
    CHECK_EQ(m.members.heads.size(), size_t(5));
    CHECK_EQ(m.positions.heads.size(), size_t(4));
    gl::fill(m, frameOf(gc::Tab::Skills, 1, 5), iconOf, [](int) { return std::string("pic:emblem"); }, nullptr);
    CHECK_EQ(m.members.heads.size(), size_t(5));  // filled in their places, not added
    CHECK_EQ(m.positions.heads.size(), size_t(4));
    if (d) rmlhost::close(d);
}
