// 3.0.11-beta.165 (task 6 of the remaining windows: the guild, Alt+G): the game's guild window (its tabs are UI windows
// 0x3C-0x41) as a card (core/guild_card) - its information (the name, the master, the level, the members of the most,
// their average level, the experience to the next level, the tax points, the territory) and its members (a row each:
// online or not, the name, the title, the job and the level), read as the game keeps them
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/guild_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

guildcard::Texts texts() {
    guildcard::Texts t;
    t.title = "Гільдія";
    t.tabs[0] = "Інформація";
    t.tabs[1] = "Учасники";
    t.tabs[2] = "Посади";
    t.tabs[3] = "Навички";
    t.tabs[4] = "Вигнані";
    t.tabs[5] = "Оголошення";
    const char* const facts[7] = {"Майстер", "Рівень", "Учасники", "Середній рівень", "Досвід", "Податок", "Територія"};
    for (int i = 0; i < 7; ++i) t.facts[i] = facts[i];
    t.none = "немає";
    t.online = "{1} з {2} у грі";
    return t;
}

}  // namespace

// The guild's windows (FUN_004d42d0 0x3C-0x41): where the manager keeps each (+0x170 on, 4 apart) - any of them open
// means the guild window is
TEST(guild_card_the_games_windows) {
    CHECK(guildcard::kWindows == 6 && guildcard::kFirstAt == 0x170 && guildcard::kCloseButton == 0x87);
}

TEST(guild_card_places_and_the_mouse) {
    const guildcard::Layout L = guildcard::layout(1920, 1080, 1.0f, guildcard::Tab::Members, 3);
    CHECK(L.tabs[1].x > L.tabs[0].x && L.rows.size() == 3 && L.rows[0].y > L.tabs[0].y);
    using P = guildcard::Part;
    const guildcard::Hit t = guildcard::hitTest(L, mid(L.tabs[0]).first, mid(L.tabs[0]).second);
    CHECK(t.part == P::Tab && t.index == 0);
    CHECK(guildcard::hitTest(L, mid(L.close).first, mid(L.close).second).part == P::Close);
    CHECK(guildcard::hitTest(L, -5, -5).part == P::None);
    const guildcard::Layout many = guildcard::layout(1920, 1080, 1.0f, guildcard::Tab::Members, 40);
    CHECK(many.rows.size() == 12 && guildcard::mostScroll(many, 40) == 28);
    const guildcard::Layout info = guildcard::layout(1920, 1080, 1.0f, guildcard::Tab::Info, 40);
    CHECK(info.rows.empty() && info.facts[6].y > info.facts[0].y);
}

TEST(guild_card_words_and_picture) {
    guildcard::Info g;
    g.name = "Heroes";
    g.master = "Ann";
    g.level = 12;
    g.members = 18;
    g.most = 36;
    g.average = 81;
    g.exp = 1500;
    g.nextExp = 4000;
    g.tax = 7;
    std::vector<guildcard::Member> m(2);
    m[0].name = "Ann";
    m[0].title = "Master";
    m[0].job = "High Wizard";
    m[0].level = 99;
    m[0].online = true;
    m[1].name = "Bob";
    m[1].job = "Knight";
    m[1].level = 80;
    const guildcard::Texts t = texts();
    guildcard::Show sh;
    const guildcard::Layout info = guildcard::layout(1920, 1080, 1.0f, guildcard::Tab::Info, m.size());
    std::vector<guildcard::Words> w = guildcard::words(info, g, m, sh, t);
    const auto has = [&](const std::vector<guildcard::Words>& ws, const std::string& s) {
        return std::any_of(ws.begin(), ws.end(), [&](const guildcard::Words& x) { return x.text == s; });
    };
    CHECK(has(w, "Heroes") && has(w, "Ann") && has(w, "12") && has(w, "18 / 36") && has(w, "81"));
    CHECK(has(w, "1500 / 4000") && has(w, "7") && has(w, "немає"));  // no territory
    sh.tab = guildcard::Tab::Members;
    const guildcard::Layout L = guildcard::layout(1920, 1080, 1.0f, guildcard::Tab::Members, m.size());
    w = guildcard::words(L, g, m, sh, t);
    CHECK(has(w, "Ann") && has(w, "Master") && has(w, "High Wizard 99") && has(w, "Knight 80"));
    CHECK(has(w, "1 з 2 у грі"));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    guildcard::paint(p, L, m, sh);
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);
}

// 3.0.11-beta.210 (the tester: «alt g відкриває режим гільди, але у ГМа немає купи функцій»): all six of the game's
// tabs on the card, two rows of three; the game's own rights for four of them (its 0x0168 bits: members 1, positions
// 2, the expelled 0x10, the notice 0x80); the information and the members drawn by the card, the rest the game's own
// windows for now (every master's tool in them)
TEST(guild_card_six_tabs) {
    const guildcard::Layout L = guildcard::layout(1920, 1080, 1.0f, guildcard::Tab::Info, 3);
    CHECK(L.tabs[0].y == L.tabs[2].y && L.tabs[3].y > L.tabs[0].y && L.tabs[3].y == L.tabs[5].y);
    CHECK(L.tabs[0].x < L.tabs[1].x && L.tabs[1].x < L.tabs[2].x && L.tabs[3].x == L.tabs[0].x);
    CHECK(L.head.y >= L.tabs[5].y + L.tabs[5].h);
    for (int i = 0; i < guildcard::kTabs; ++i) {
        const guildcard::Hit h = guildcard::hitTest(L, mid(L.tabs[i]).first, mid(L.tabs[i]).second);
        CHECK(h.part == guildcard::Part::Tab && h.index == i);
    }
    CHECK(!guildcard::gameDrawn(0) && !guildcard::gameDrawn(1));
    CHECK(guildcard::gameDrawn(2) && guildcard::gameDrawn(3) && guildcard::gameDrawn(4) && guildcard::gameDrawn(5));
    CHECK(guildcard::allowed(0, 0) && guildcard::allowed(3, 0));
    CHECK(!guildcard::allowed(1, 0) && guildcard::allowed(1, 1));
    CHECK(!guildcard::allowed(2, 1) && guildcard::allowed(2, 2));
    CHECK(!guildcard::allowed(4, 0xEF) && guildcard::allowed(4, 0x10));
    CHECK(!guildcard::allowed(5, 0x7F) && guildcard::allowed(5, 0x80));
    std::vector<guildcard::Member> none;
    guildcard::Show sh;
    sh.rights = 0x01;
    const std::vector<guildcard::Words> w = guildcard::words(L, guildcard::Info{}, none, sh, texts());
    const auto word = [&](const std::string& t) {
        return std::find_if(w.begin(), w.end(), [&](const guildcard::Words& x) { return x.text == t; });
    };
    CHECK(word("Посади") != w.end() && word("Учасники") != w.end() && word("Оголошення") != w.end());
    CHECK(word("Посади")->rgb != word("Учасники")->rgb);  // not allowed: dimmed
}
