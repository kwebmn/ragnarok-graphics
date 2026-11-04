// 3.0.11-beta.220 (the owner «1-3 делай»: the guild's tabs 2-5 in the new look): the positions (0x0166 names, 0x0160
// rights and tax), the guild's skills (0x0162: the points, each skill's level and whether it can rise), the expelled
// (0x0163: name, account, reason) and the notice (0x016f: subject, body) read from the server's packets
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/guild_card.hpp"
#include "core/guild_tabs.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

void w16(std::vector<uint8_t>& p, size_t at, int v) {
    p[at] = uint8_t(v);
    p[at + 1] = uint8_t(v >> 8);
}
void w32(std::vector<uint8_t>& p, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[at + size_t(i)] = uint8_t(v >> (8 * i));
}
void text(std::vector<uint8_t>& p, size_t at, const char* t) { std::memcpy(p.data() + at, t, std::strlen(t)); }

}  // namespace

TEST(guild_tabs_positions_from_both_packets) {
    std::vector<uint8_t> names(4 + 2 * 28, 0);
    w16(names, 0, 0x166);
    w16(names, 2, int(names.size()));
    w32(names, 4, 0);
    text(names, 8, "Master");
    w32(names, 32, 1);
    text(names, 36, "Officer");
    std::vector<uint8_t> infos(4 + 2 * 16, 0);
    w16(infos, 0, 0x160);
    w16(infos, 2, int(infos.size()));
    w32(infos, 4, 0);
    w32(infos, 8, 0x11);  // invite + expel
    w32(infos, 16, 10);   // tax
    w32(infos, 20, 1);
    w32(infos, 24, 0x01);
    w32(infos, 32, 0);
    guildtabs::Data d;
    CHECK(guildtabs::read(d, names.data(), names.size()));
    CHECK(guildtabs::read(d, infos.data(), infos.size()));
    CHECK_EQ(d.positions.size(), size_t(2));
    if (d.positions.size() == 2) {
        CHECK(d.positions[0].name == "Master" && d.positions[0].invite && d.positions[0].expel &&
              d.positions[0].tax == 10);
        CHECK(d.positions[1].name == "Officer" && d.positions[1].invite && !d.positions[1].expel &&
              d.positions[1].tax == 0);
    }
}

TEST(guild_tabs_skills_expelled_notice) {
    std::vector<uint8_t> sk(6 + 2 * 37, 0);
    w16(sk, 0, 0x162);
    w16(sk, 2, int(sk.size()));
    w16(sk, 4, 3);  // points
    w16(sk, 6, 10000);
    w16(sk, 6 + 6, 1);
    sk[6 + 36] = 0;
    w16(sk, 43, 10004);
    w16(sk, 43 + 6, 2);
    sk[43 + 36] = 1;
    guildtabs::Data d;
    CHECK(guildtabs::read(d, sk.data(), sk.size()));
    CHECK_EQ(d.points, 3);
    CHECK_EQ(d.skills.size(), size_t(2));
    if (d.skills.size() == 2) {
        CHECK(d.skills[0].id == 10000 && d.skills[0].lv == 1 && !d.skills[0].up);
        CHECK(d.skills[1].id == 10004 && d.skills[1].lv == 2 && d.skills[1].up);
    }
    CHECK_EQ(guildtabs::skillName(10004), std::string("Guild Extension"));
    CHECK_EQ(guildtabs::skillName(10014), std::string("Permanent Development"));
    CHECK(guildtabs::skillName(9999).empty());
    std::vector<uint8_t> ex(4 + 88, 0);
    w16(ex, 0, 0x163);
    w16(ex, 2, int(ex.size()));
    text(ex, 4, "Bob");
    text(ex, 28, "bob01");
    text(ex, 52, "spam");
    CHECK(guildtabs::read(d, ex.data(), ex.size()));
    CHECK(d.expelled.size() == 1 && d.expelled[0].name == "Bob" && d.expelled[0].account == "bob01" &&
          d.expelled[0].reason == "spam");
    std::vector<uint8_t> no(182, 0);
    w16(no, 0, 0x16f);
    text(no, 2, "Siege tonight");
    text(no, 62, "Meet at Prontera 20:00");
    CHECK(guildtabs::read(d, no.data(), no.size()));
    CHECK(d.subject == "Siege tonight" && d.body == "Meet at Prontera 20:00");
    std::vector<uint8_t> other(10, 0);
    w16(other, 0, 0x99);
    CHECK(!guildtabs::read(d, other.data(), other.size()));
}

// The card's tabs 2-5 drawn by it: rows for the positions, the skills (a «+» where one can rise and points are left)
// and the expelled; the notice its subject and body; «Змінити» on the positions and the notice opens the game's own
TEST(guild_card_tabs_two_to_five) {
    using T = guildcard::Tab;
    CHECK(!guildcard::gameDrawn(2, false) && guildcard::gameDrawn(2, true) && guildcard::gameDrawn(5, true));
    CHECK(!guildcard::gameDrawn(3, true) && !guildcard::gameDrawn(4, true) && !guildcard::gameDrawn(1, true));
    const guildcard::Layout pos = guildcard::layout(1920, 1080, 1.0f, T::Positions, 4);
    CHECK(pos.rows.size() == 4 && pos.edit.w > 0 && pos.edit.y > pos.rows[3].y);
    const auto mid = [](const panel::Box& b) { return std::make_pair(b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(guildcard::hitTest(pos, mid(pos.edit).first, mid(pos.edit).second).part == guildcard::Part::Edit);
    const guildcard::Layout sk = guildcard::layout(1920, 1080, 1.0f, T::Skills, 2);
    CHECK(sk.rows.size() == 2 && sk.edit.w == 0 && sk.plus.size() == 2);
    const guildcard::Hit p = guildcard::hitTest(sk, mid(sk.plus[1]).first, mid(sk.plus[1]).second);
    CHECK(p.part == guildcard::Part::Plus && p.index == 1);
    const guildcard::Layout no = guildcard::layout(1920, 1080, 1.0f, T::Notice, 0);
    CHECK(no.notice.h > 60 && no.edit.w > 0);
    guildcard::More more;
    more.lines = {{"Master", "Запрошення · Вигнання", "10 %", false}, {"Officer", "Запрошення", "0 %", false}};
    more.head = "4 посади";
    guildcard::Texts t;
    t.edit = "Змінити";
    guildcard::Show sh;
    sh.tab = T::Positions;
    sh.canEdit = true;  // the tab's rights
    const std::vector<guildcard::Words> w = guildcard::words(pos, guildcard::Info{}, {}, more, sh, t);
    const auto has = [&](const std::string& s) {
        for (const auto& x : w)
            if (x.text == s) return true;
        return false;
    };
    CHECK(has("Master") && has("Запрошення · Вигнання") && has("10 %") && has("Змінити") && has("4 посади"));
    more.subject = "Siege tonight";
    more.body = "Meet at Prontera";
    sh.tab = T::Notice;
    const std::vector<guildcard::Words> nw = guildcard::words(no, guildcard::Info{}, {}, more, sh, t);
    bool subject = false, body = false;
    for (const auto& x : nw) {
        subject = subject || x.text == "Siege tonight";
        body = body || x.text == "Meet at Prontera";
    }
    CHECK(subject && body);
}
