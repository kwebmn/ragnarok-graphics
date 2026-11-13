// 3.0.11-beta.41: the chat (core/chat_panel) - the game's lines into its channels, the tabs, the rows broken at words,
// its places and the mouse over them
#include <algorithm>
#include <string>
#include <vector>

#include "core/chat_panel.hpp"
#include "core/settings/search.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::chatpanel;

namespace {

Texts texts() {
    Texts t;
    const char* tabs[6] = {"Усі", "Група", "Гільдія", "Шепіт", "Торг", "Бій"};
    for (int i = 0; i < 6; ++i) t.tabs[i] = tabs[i];
    t.from = "від {1}";
    t.to = "до {1}";
    t.send = "Enter — надіслати · Tab — канал";
    t.type = "Enter — писати · Tab — канал";
    t.newer = "\xE2\x86\x93";
    return t;
}

std::string all(const Line& l) {
    std::string s;
    for (const Span& sp : l.spans) s += sp.text;
    return s;
}

}  // namespace

TEST(chat_lines_from_the_game) {
    const Texts t = texts();
    const Line pub = fromGame(Source::Public, "Rin : ПРОДАМ: Elunium 10 шт", 0xFFFFFF, t, 5);
    CHECK(pub.kind == Kind::Public && pub.who == "Rin" && pub.trade && pub.ms == 5);
    CHECK(pub.spans.size() == 2 && pub.spans[0].name && pub.spans[0].text == "Rin");
    CHECK(all(pub) == "Rin: ПРОДАМ: Elunium 10 шт");
    CHECK(!fromGame(Source::PublicOwn, "Sora : hi all", 0xFFFFFF, t, 0).trade);
    const Line party = fromGame(Source::Party, "Tor : Збираємося", 0xC8C8FF, t, 0);
    CHECK(party.kind == Kind::Party && party.who == "Tor" && all(party) == "[Група] Tor: Збираємося");
    CHECK(party.spans[0].rgb == kParty && party.spans[1].name);
    const Line guild = fromGame(Source::Guild, "Kael : хто на GH?", 0xFFFFFF, t, 0);
    CHECK(guild.kind == Kind::Guild && all(guild) == "[Гільдія] Kael: хто на GH?");
    const Line in = fromGame(Source::Whisper, "( From Nova ) : продаси Clip?", 0xFFFF, t, 0);
    CHECK(in.kind == Kind::Whisper && in.who == "Nova" && all(in) == "[від Nova] продаси Clip?");
    CHECK(in.spans[1].name && in.spans[1].text == "Nova");
    const Line out = fromGame(Source::WhisperResult, "( To Nova ) : так", 0xFFFF, t, 0);
    CHECK(out.kind == Kind::Whisper && out.who == "Nova" && all(out) == "[до Nova] так");
    const Line gone = fromGame(Source::WhisperResult, "Nova is not online", 0xFFFF, t, 0);
    CHECK(gone.kind == Kind::Whisper && gone.who.empty() && gone.spans[0].rgb == 0xFFFF00);
    const Line ann = fromGame(Source::Announce, "Server restart in 5 minutes", 0xFFFF, t, 0);
    CHECK(ann.kind == Kind::Announce && ann.spans[0].rgb == 0xFFFF00 && ann.spans[0].bold);
    const Line sys = fromGame(Source::Other, "You got 10 zeny", 0xFF00, t, 0);
    CHECK(sys.kind == Kind::System && sys.spans[0].rgb == 0x00FF00);
    CHECK(gameRgb(0x9BCD9B) == 0x9BCD9B && gameRgb(0xFF) == 0xFF0000);
    CHECK(tradeLike("S> Clip 400k") && tradeLike("куплю Elu") && tradeLike("Обмін"));
    CHECK(!tradeLike("привіт усім"));
    CHECK(grouped(6140) == "6 140" && grouped(1288) == "1 288" && grouped(12) == "12");
    CHECK(grouped(1000000) == "1 000 000");
}

TEST(chat_tabs_show_their_lines) {
    const Texts t = texts();
    Line battle;
    battle.kind = Kind::Battle;
    Line loot;
    loot.kind = Kind::Loot;
    const Line trade = fromGame(Source::Public, "Rin : WTB Clip", 0xFFFFFF, t, 0);
    const Line party = fromGame(Source::Party, "Tor : go", 0xFFFFFF, t, 0);
    CHECK(!shows(Tab::All, battle) && shows(Tab::All, loot) && shows(Tab::All, trade) && shows(Tab::All, party));
    CHECK(shows(Tab::Battle, battle) && shows(Tab::Battle, loot) && !shows(Tab::Battle, party));
    CHECK(shows(Tab::Trade, trade) && !shows(Tab::Trade, party) && shows(Tab::Party, party));
    CHECK(unreadOn(party) == 1 && unreadOn(trade) == -1 && unreadOn(battle) == -1);
}

TEST(chat_search_matches_words_and_speaker) {  // 3.0.11-beta.284 (the owner's feature 17)
    chatpanel::Line l;
    l.who = "Ольга";
    l.spans.push_back(chatpanel::Span{"Продам Poring Card", chatpanel::kText});
    CHECK(chatpanel::matches(l, ""));
    CHECK(chatpanel::matches(l, settings::foldCase("poring")));
    CHECK(chatpanel::matches(l, settings::foldCase("ПРОДАМ")));
    CHECK(chatpanel::matches(l, settings::foldCase("ольга")));
    CHECK(!chatpanel::matches(l, settings::foldCase("куплю")));
}

// 3.0.12-beta.3 (players' screenshots): the server's MVP lines glued - spaced; «Not Enough SP» in the player's
// language; a player's own words never touched
TEST(chat_tidies_the_server_mvp_lines_and_no_sp) {
    Texts t = texts();
    t.noSp = "Недостатньо SP";
    const auto text = [](const Line& l) {
        std::string s;
        for (const Span& p : l.spans) s += p.text;
        return s;
    };
    CHECK(text(fromGame(Source::Other, "Congratulations! You are the MVP! Your reward EXP Points are29587acquired!",
                        0xFFFF, t, 0)) ==
          "Congratulations! You are the MVP! Your reward EXP Points are 29587 acquired!");
    CHECK(text(fromGame(Source::Other, "Congratulations! You are the MVP! Your reward item isBlue Potion", 0xFFFF, t,
                        0)) == "Congratulations! You are the MVP! Your reward item is Blue Potion");
    CHECK(text(fromGame(Source::Other, "Not Enough SP", 0x0000FF, t, 0)) == "Недостатньо SP");
    const Line spaced = fromGame(Source::Other, "Your reward EXP Points are 5 acquired!", 0, t, 0);
    CHECK(text(spaced) == "Your reward EXP Points are 5 acquired!");
    CHECK(text(fromGame(Source::Public, "Rin : Your reward item isX", 0xFFFFFF, t, 0)).find("isX") !=
          std::string::npos);
}

// 3.0.13-beta.3 (the owner's pick 24 of «50 идей»): the chat's colours - the party's, the guild's, one's own and the
// others' words; a GM's (the game's own colour) kept
TEST(chat_lines_in_the_players_colours) {
    Texts t = texts();
    t.party = chatColour("orange", kParty);
    t.guild = chatColour("violet", kGuild);
    t.own = chatColour("yellow", 0);
    t.others = chatColour("white", 0);
    CHECK(chatColour("default", kParty) == kParty && chatColour("nonsense", 7) == 7);
    CHECK(fromGame(Source::Party, "Tor : hi", 0xFFFFFF, t, 0).spans[0].rgb == 0xFFB067);
    CHECK(fromGame(Source::Guild, "Kael : hi", 0xFFFFFF, t, 0).spans[0].rgb == 0xC3A6FF);
    CHECK(fromGame(Source::PublicOwn, "Me : hi", 0xFFFFFF, t, 0).spans[1].rgb == 0xF3E27A);
    CHECK(fromGame(Source::Public, "Rin : hi", 0xFFFFFF, t, 0).spans[1].rgb == 0xFFFFFF);
    CHECK(fromGame(Source::Public, "GM : hi", 0x00FFFF, t, 0).spans[1].rgb != 0xFFFFFF);  // a GM's own colour
    const Texts plain = texts();
    CHECK(fromGame(Source::Public, "Rin : hi", 0xFFFFFF, plain, 0).spans[1].rgb == kText);
}

// 3.0.13-beta.3 (the owner's pick 28 of «50 идей»): [interface] chat_log - a line with its time, the fight's skipped
TEST(chat_log_line_with_its_time) {
    const Texts t = texts();
    const Line party = fromGame(Source::Party, "Tor : hi\nall", 0xFFFFFF, t, 0);
    const std::string l = logLine(party, 9, 5, 7);
    CHECK(l.rfind("[09:05:07] ", 0) == 0 && l.find("Tor") != std::string::npos);
    CHECK(l.find('\n') == l.size() - 1 && l.find("hi all") != std::string::npos);
    Line fight;
    fight.kind = Kind::Battle;
    fight.spans = {Span{"Poring -12", kText, false, false}};
    CHECK(logLine(fight, 1, 2, 3).empty());
}

// 3.0.13-beta.12 (the owner's pick 21 of «50 идей»): a line past the server's 70 sent as pieces cut between words,
// none longer than the piece, none starting with a space; one long word cut where it must
TEST(chat_long_line_cut_between_words) {
    std::string text;
    for (int i = 0; i < 30; ++i) text += "word" + std::to_string(i) + " ";
    const std::vector<std::string> p = pieces(text, kChatPiece);
    CHECK(p.size() >= 3);
    std::string joined;
    for (const std::string& s : p) {
        CHECK(!s.empty() && s.size() <= kChatPiece && s.front() != ' ' && s.back() != ' ');
        joined += s + " ";
    }
    CHECK(joined == text);  // nothing lost, the spaces between the pieces the ones cut at
    CHECK(pieces("short", kChatPiece).size() == 1);
    const std::vector<std::string> word = pieces(std::string(150, 'x'), kChatPiece);
    CHECK(word.size() == 3 && word[0].size() == kChatPiece && word[2].size() == 150 - 2 * kChatPiece);
}


// 3.0.18-beta.170 (R-CD8KCP: the chat's frame grew from 0.13 to 1.33 ms in three minutes of hunting - each kill,
// pick-up or experience line wrapped all its up to 200 lines again, every word measured): a line's rows kept while it
// stays the same - its number, its spans' count, its repeats - and the width, the size and the look do too
TEST(chat_panel_wrap_cache_measures_only_new_or_changed_lines) {
    int calls = 0;
    const Measure m = [&](const std::string& s, float px, bool) {
        ++calls;
        return float(s.size()) * px * 0.5f;
    };
    Line a, b;
    a.serial = 1;
    a.spans = {Span{"Poring dropped Jellopy ", kText, false, false}};
    b.serial = 2;
    b.spans = {Span{"Base EXP +12", kText, true, false}};
    const std::vector<const Line*> both = {&a, &b};
    WrapCache cache;
    const std::vector<Row> first = cache.wrap(both, 120, 10, m, 1);
    CHECK(first.size() == wrap(both, 120, 10, m).size());  // the same rows as the whole wrap
    const int afterFirst = calls;
    const std::vector<Row> again = cache.wrap(both, 120, 10, m, 1);
    CHECK_EQ(calls, afterFirst);  // nothing measured again
    CHECK(again.size() == first.size());
    b.spans.push_back(Span{" · Job EXP +4", kText, true, false});  // the kill's line got its experience
    calls = 0;
    cache.wrap(both, 120, 10, m, 1);
    CHECK(calls > 0);
    const int changedOnly = calls;
    calls = 0;
    wrap(std::vector<const Line*>{&b}, 120, 10, m);
    CHECK_EQ(changedOnly, calls);  // the changed line alone measured
    calls = 0;
    cache.wrap(both, 200, 10, m, 1);  // another width: all again
    CHECK(calls > 0);
    calls = 0;
    cache.wrap(both, 200, 10, m, 2);  // another look (the words' sizes may differ): all again
    CHECK(calls > 0);
}

TEST(chat_panel_wrap_cache_keeps_each_piece_on_its_line_of_the_list) {
    const Measure m = [](const std::string& s, float px, bool) { return float(s.size()) * px * 0.5f; };
    Line a, b, c;
    a.serial = 1;
    a.spans = {Span{"one", kText, false, false}};
    b.serial = 2;
    b.spans = {Span{"two", kText, false, false}};
    c.serial = 3;
    c.spans = {Span{"three", kText, false, false}};
    WrapCache cache;
    cache.wrap({&a, &b, &c}, 200, 10, m, 1);
    const std::vector<Row> rows = cache.wrap({&b, &c}, 200, 10, m, 1);  // a filtered out: b first now
    CHECK(rows.size() == 2);
    CHECK(rows[0][0].text == "two" && rows[0][0].line == 0);
    CHECK(rows[1][0].text == "three" && rows[1][0].line == 1);
    Line d;  // no number (0): never kept, always wrapped
    d.spans = {Span{"four", kText, false, false}};
    const std::vector<Row> withD = cache.wrap({&d}, 200, 10, m, 1);
    CHECK(withD.size() == 1 && withD[0][0].text == "four" && withD[0][0].line == 0);
}

// 3.0.18-beta.245 («Довідник» › chat links): «[Name]» in a line - a thing's or a monster's name the reference knows
// becomes its link (the brackets kept), the rest of the words as they were
TEST(chat_panel_bracketed_names_link_to_the_reference) {
    const auto resolve = [](const std::string& name) -> std::pair<int, int> {
        if (name == "Poring Card") return {4001, 0};
        if (name == "Poring") return {0, 1002};
        return {0, 0};
    };
    std::vector<chatpanel::Span> in{chatpanel::Span{"Ann: ", chatpanel::kText, true, true},
                                    chatpanel::Span{"WTS [Poring Card] 1kk, kill [Poring] [Nobody] [oops", 0xFFFFFF}};
    const std::vector<chatpanel::Span> out = chatpanel::bracketLinks(in, resolve);
    CHECK_EQ(out.size(), size_t(6));
    if (out.size() != 6) return;
    CHECK(out[0].name && out[0].text == "Ann: ");  // the speaker's span untouched
    CHECK(out[1].text == "WTS " && out[1].item == 0 && out[1].rgb == 0xFFFFFF);
    CHECK(out[2].text == "[Poring Card]" && out[2].item == 4001 && out[2].mob == 0);
    CHECK(out[3].text == " 1kk, kill ");
    CHECK(out[4].text == "[Poring]" && out[4].mob == 1002);
    CHECK(out[5].text == " [Nobody] [oops" && out[5].item == 0 && out[5].mob == 0);
    const std::vector<chatpanel::Span> none = chatpanel::bracketLinks({chatpanel::Span{"no names here", 1}}, resolve);
    CHECK(none.size() == 1 && none[0].text == "no names here");
}
