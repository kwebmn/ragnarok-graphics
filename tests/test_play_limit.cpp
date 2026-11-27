// 3.0.24-beta.31 (R-S2JJXG, R-V7SV57, R-XQ93QH, R-P4HVX6, R-MWNRVM: five «drops» of one laptop - each the map server's
// notice 1 some 41 s after the map, the server's daily play-time limit): its words read, told in the player's, its
// kick no drop to report
#include <cstdio>
#include <string>

#include "core/chat_panel.hpp"
#include "core/play_limit.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// the server's own words as its map server makes them (map-server_sql pc_playlimit_check: the message, then its hint)
std::string reachedWords(int hours, int kickIn, int resetH, int resetMin, const char* hint) {
    char line[512];
    std::snprintf(line, sizeof line,
                  "Daily play-time limit (%d h) reached. You will be disconnected in %d seconds. The limit resets at "
                  "00:00 server time (in %d h %02d min).%s",
                  hours, kickIn, resetH, resetMin, hint);
    return line;
}

std::string buyHint(const char* before, int zeny) {
    char line[256];
    std::snprintf(line, sizeof line,
                  "%s Buy more time with @buyplaytime <minutes>, or pay as you play with @autopay on (%d zeny a "
                  "minute).",
                  before, zeny);
    return line;
}

playlimit::Words someWords() {
    playlimit::Words w;
    w.reached = "limit {1} h, out in {2} s.";
    w.kicked = "kicked: limit {1} h.";
    w.left = "{1} min left.";
    w.reset = "reset in {1} h {2} min.";
    w.buy = "buy: @buyplaytime.";
    w.autopay = "autopay {1} z.";
    return w;
}

}  // namespace

TEST(play_limit_reads_the_servers_words) {
    playlimit::Notice n;
    CHECK(playlimit::read(reachedWords(8, 40, 11, 42, buyHint("", 1).c_str()), n));
    CHECK(n.reached && n.hours == 8 && n.kickInS == 40 && n.resetMin == 11 * 60 + 42);
    CHECK(n.canBuy && n.zenyPerMinute == 1 && n.minutesLeft == -1);
    // no hint (the autopay already on, or nothing to buy with): the limit and its reset only
    playlimit::Notice bare;
    CHECK(playlimit::read(reachedWords(6, 30, 0, 5, ""), bare));
    CHECK(bare.reached && bare.hours == 6 && bare.kickInS == 30 && bare.resetMin == 5);
    CHECK(!bare.canBuy && bare.zenyPerMinute == -1);
    // the warning before it
    playlimit::Notice left;
    const std::string warning = "Daily play time: 10 minute(s) left today." + buyHint(" See @playtime.", 85);
    CHECK(playlimit::read(warning, left));
    CHECK(!left.reached && left.minutesLeft == 10 && left.canBuy && left.zenyPerMinute == 85 && left.resetMin == -1);
    // the rest, and a player's own line saying the same (one's own public words come as «Name : words»)
    playlimit::Notice other;
    CHECK(!playlimit::read("Autoloot is now off.", other));
    CHECK(!playlimit::read("Primrose : " + reachedWords(8, 40, 11, 42, ""), other));
    CHECK(!playlimit::read("", other));
}

TEST(play_limit_words_for_the_player) {
    const playlimit::Words w = someWords();
    playlimit::Notice n;
    CHECK(playlimit::read(reachedWords(8, 40, 11, 42, buyHint("", 1).c_str()), n));
    CHECK_EQ(playlimit::words(n, w, false), std::string("limit 8 h, out in 40 s. reset in 11 h 42 min. buy: "
                                                        "@buyplaytime. autopay 1 z."));
    // after the kick: its own first words, the reset by the minutes gone since the words
    CHECK_EQ(playlimit::words(n, w, true, 1), std::string("kicked: limit 8 h. reset in 11 h 41 min. buy: @buyplaytime. "
                                                          "autopay 1 z."));
    CHECK_EQ(playlimit::words(n, w, true, 100000), std::string("kicked: limit 8 h. reset in 0 h 0 min. buy: "
                                                               "@buyplaytime. autopay 1 z."));
    playlimit::Notice left;
    CHECK(playlimit::read("Daily play time: 10 minute(s) left today." + buyHint(" See @playtime.", 85), left));
    CHECK_EQ(playlimit::words(left, w, false), std::string("10 min left. buy: @buyplaytime. autopay 85 z."));
    playlimit::Notice bare;
    CHECK(playlimit::read(reachedWords(6, 30, 0, 5, ""), bare));
    CHECK_EQ(playlimit::words(bare, w, false), std::string("limit 6 h, out in 30 s. reset in 0 h 5 min."));
    // the texts of each language there, the server's commands as they are
    for (const settings::Lang l : {settings::Lang::Uk, settings::Lang::En, settings::Lang::Ru}) {
        const std::string said = playlimit::words(n, playlimit::wordsIn(l), false);
        CHECK(said.find("8") != std::string::npos && said.find("40") != std::string::npos);
        CHECK(said.find("@buyplaytime") != std::string::npos && said.find("@autopay on") != std::string::npos);
        CHECK(!playlimit::blockedWords(l).empty());
    }
}

TEST(play_limit_kick_is_no_drop) {
    playlimit::Notice n;
    CHECK(playlimit::read(reachedWords(8, 40, 11, 42, ""), n));
    nettrail::Drop d;
    d.notice = true;
    d.noticeCode = 1;
    d.role = nettrail::Role::Zone;
    d.ms = 100000 + 41000;
    CHECK(playlimit::kickOf(d, 100000, n));
    d.ms = 100000 + 40000 + playlimit::kKickSlackMs;
    CHECK(playlimit::kickOf(d, 100000, n));
    d.ms = 100000 + 40000 + playlimit::kKickSlackMs + 1;  // too late: another drop
    CHECK(!playlimit::kickOf(d, 100000, n));
    d.ms = 99000;  // before the words
    CHECK(!playlimit::kickOf(d, 100000, n));
    d.ms = 141000;
    d.noticeCode = 2;  // «another one logged in with this account»
    CHECK(!playlimit::kickOf(d, 100000, n));
    d.noticeCode = 1;
    d.role = nettrail::Role::Char;
    CHECK(!playlimit::kickOf(d, 100000, n));
    d.role = nettrail::Role::Zone;
    d.notice = false;  // closed with no notice
    CHECK(!playlimit::kickOf(d, 100000, n));
    d.notice = true;
    playlimit::Notice left;  // only the warning came
    CHECK(playlimit::read("Daily play time: 10 minute(s) left today.", left));
    CHECK(!playlimit::kickOf(d, 100000, left));
}

TEST(chat_panel_play_limit_in_the_players_words) {
    chatpanel::Texts t;
    t.limit = someWords();
    chatpanel::Line l;
    CHECK(chatpanel::shortSystem(reachedWords(8, 40, 11, 42, buyHint("", 1).c_str()), t, l));
    CHECK(l.kind == chatpanel::Kind::System && l.spans.size() == 1);
    CHECK_EQ(l.spans[0].text, std::string("limit 8 h, out in 40 s. reset in 11 h 42 min. buy: @buyplaytime. "
                                          "autopay 1 z."));
    chatpanel::Texts none;  // no words given: as it came
    chatpanel::Line as;
    CHECK(!chatpanel::shortSystem(reachedWords(8, 40, 11, 42, ""), none, as));
}
