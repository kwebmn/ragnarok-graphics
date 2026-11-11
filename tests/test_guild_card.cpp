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
    // 3.0.11-beta.220: all six the card's - the game's own only while «Змінити» edits positions or the notice
    for (int i = 0; i < guildcard::kTabs; ++i) CHECK(!guildcard::gameDrawn(i, false));
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

// 3.0.11-beta.271 (the tester: «вікно формування привітання в гільдії - в старому стилі»): the notice edited on the
// card - its two fields and «Скасувати» beside «Зберегти»; the subject's caret
TEST(guild_card_notice_edited_on_the_card) {
    const guildcard::Layout L = guildcard::layout(1920, 1080, 1.0f, guildcard::Tab::Notice, 0, true);
    CHECK(L.fields[0].w > 0 && L.fields[1].h > L.fields[0].h && L.cancel.w > 0);
    CHECK(L.fields[0].y + L.fields[0].h < L.fields[1].y);
    const guildcard::Hit body = guildcard::hitTest(L, L.fields[1].x + 5, L.fields[1].y + 5);
    CHECK(body.part == guildcard::Part::Field && body.index == 1);
    CHECK(guildcard::hitTest(L, L.cancel.x + 5, L.cancel.y + 5).part == guildcard::Part::Cancel);
    const guildcard::Layout plain = guildcard::layout(1920, 1080, 1.0f, guildcard::Tab::Notice, 0);
    CHECK(plain.fields[0].w == 0 && plain.cancel.w == 0 && plain.edit.w > 0);
    guildcard::Show sh;
    sh.tab = guildcard::Tab::Notice;
    sh.canEdit = sh.editing = true;
    sh.subject = "Збір";
    guildcard::Texts t;
    t.save = "Зберегти";
    bool caret = false, save = false;
    for (const guildcard::Words& w : guildcard::words(L, guildcard::Info{}, {}, guildcard::More{}, sh, t)) {
        caret = caret || w.text == "Збір|";
        save = save || w.text == "Зберегти";
    }
    CHECK(caret && save);
}
