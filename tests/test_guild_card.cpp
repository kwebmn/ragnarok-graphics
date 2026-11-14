// 3.0.11-beta.165 (task 6 of the remaining windows: the guild, Alt+G): the game's guild window (its tabs are UI windows
// 0x3C-0x41) as a card (core/guild_card) - its information (the name, the master, the level, the members of the most,
// their average level, the experience to the next level, the tax points, the territory) and its members (a row each:
// online or not, the name, the title, the job and the level), read as the game keeps them
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "core/client_libs.hpp"
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

// 3.0.19-beta.1 (the testers' doc 14.11: «сирувате окне і не видно значок ГІ твоеї»): «Інформація» opens with the
// guild's own head - its emblem (the game's 24 x 24 twice) at the left, the guild's name beside it in gold, its level
// and members under the name; the facts below it
TEST(guild_card_info_opens_with_the_emblem_and_the_name) {
    for (float s : {1.0f, 1.5f}) {
        const guildcard::Layout L = guildcard::layout(1920, 1080, s, guildcard::Tab::Info, 0);
        CHECK(std::abs(L.emblem.w - 48 * s) < 0.01f && std::abs(L.emblem.h - 48 * s) < 0.01f);
        CHECK(L.emblem.y >= L.tabs[3].y + L.tabs[3].h);  // under the tabs
        CHECK(L.facts[0].y >= L.emblem.y + L.emblem.h);  // the facts under it
        CHECK(L.head.x >= L.emblem.x + L.emblem.w);      // the name beside it
        CHECK(L.sub.y >= L.head.y + L.head.h - 0.01f && L.sub.x == L.head.x);
    }
    guildcard::Info g;
    g.name = "Plague.Corp";
    g.level = 32;
    g.members = 51;
    g.most = 76;
    guildcard::Show sh;
    sh.tab = guildcard::Tab::Info;
    const guildcard::Layout L = guildcard::layout(1920, 1080, 1, sh.tab, 0);
    bool name = false, sub = false;
    for (const guildcard::Words& w : guildcard::words(L, g, {}, sh, texts())) {
        if (w.text == "Plague.Corp" && w.bold && std::abs(w.box.y - L.head.y) < 0.01f) name = true;
        if (w.text == "Рівень 32 · Учасники 51 / 76" && std::abs(w.box.y - L.sub.y) < 0.01f) sub = true;
    }
    CHECK(name);
    CHECK(sub);
    // other tabs keep no emblem
    CHECK(guildcard::layout(1920, 1080, 1, guildcard::Tab::Members, 3).emblem.w == 0);
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
