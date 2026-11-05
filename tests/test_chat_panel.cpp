// 3.0.11-beta.41: the chat (core/chat_panel) - the game's lines into its channels, the tabs, the rows broken at words,
// its places and the mouse over them
#include <algorithm>
#include <string>
#include <vector>

#include "core/chat_panel.hpp"
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

TEST(chat_rows_break_at_words) {
    const Texts t = texts();
    const Line l = fromGame(Source::Party, "Lis : хілю, тримайтесь біля мене, ідемо далі на південь до воріт Payon",
                            0xFFFFFF, t, 0);
    const Line s = fromGame(Source::Public, "Rin : hi", 0xFFFFFF, t, 0);
    const std::vector<const Line*> lines{&l, &s};
    const std::vector<Row> rows = wrap(lines, 260, 12.5f, estimate);
    CHECK(rows.size() >= 3);
    bool name = false;
    for (const Row& r : rows) {
        float right = 0;
        for (const Piece& p : r) {
            CHECK(p.x >= right - 0.01f);  // never over the one before
            right = p.x + p.w;
            name = name || (p.name && p.text == "Lis");
        }
        CHECK(right <= 260 + 24);  // a word's room past the row at most (its space)
    }
    CHECK(name && rows.back().front().line == 1 && rows.front().front().line == 0);
    const Line longWord = fromGame(Source::Other, std::string(80, 'x'), 0xFFFFFF, t, 0);
    const std::vector<const Line*> one{&longWord};
    CHECK(wrap(one, 120, 12.5f, estimate).size() >= 3);  // by its letters
}

TEST(chat_layout_and_mouse) {
    const Texts t = texts();
    Show sh;
    sh.unread[1] = 3;
    Input in;
    const Layout L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    CHECK(L.y + L.h == 1070 && L.x == 10 && L.w == 520 && L.tabs.size() == 6 && L.rows == 7);
    CHECK(L.lines.h == 7 * 18 && L.input.y > L.lines.y + L.lines.h && L.field.x > L.mode.x + L.mode.w);
    CHECK(L.tabs[2].x > L.tabs[1].x + L.tabs[1].w && L.tabs[5].x + L.tabs[5].w < L.glass.w);
    const Layout Q = layout(10, 1070, 1.0f, 7, true, sh, in, t, estimate);
    // 3.0.11-beta.240 (the chat concept, stage 1): idle, only its ground fades - its tabs, rows and input stay
    CHECK(Q.quiet && Q.tabs.size() == 6 && Q.rows == 7 && Q.h == L.h && Q.input.w == L.input.w);
    std::vector<Line> lines;
    for (int i = 0; i < 12; ++i)
        lines.push_back(fromGame(Source::Public, "P" + std::to_string(i) + " : hello", 0, t, 0));
    std::vector<const Line*> ptrs;
    for (const Line& l : lines) ptrs.push_back(&l);
    const std::vector<Row> rows = wrap(ptrs, L.lines.w, L.px, estimate);
    CHECK(rows.size() == 12);
    size_t first = 0, count = 0;
    shown(L, rows, 0, first, count);
    CHECK(first == 5 && count == 7);
    shown(L, rows, 3, first, count);
    CHECK(first == 2 && count == 7);
    const Hit tab = hitTest(L, rows, sh, L.tabs[3].x + 4, L.tabs[3].y + 4);
    CHECK(tab.part == Part::Tab && tab.index == 3);
    const float lastRow = L.lines.y + L.lines.h - L.rowH / 2;
    const Hit name = hitTest(L, rows, sh, L.lines.x + 3, lastRow);  // the newest line's name
    CHECK(name.part == Part::Name && name.index == 11);
    CHECK(hitTest(L, rows, sh, L.mode.x + 3, L.mode.y + 3).part == Part::Mode);
    CHECK(hitTest(L, rows, sh, L.field.x + 30, L.field.y + 5).part == Part::Field);
    CHECK(hitTest(L, rows, sh, -1, -1).part == Part::None);
    in.whisper = "Nova";
    const Layout W = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    CHECK(W.clear.w > 0 && hitTest(W, rows, sh, W.clear.x + 3, W.clear.y + 3).part == Part::Clear);
}

TEST(chat_picture_and_words) {
    const Texts t = texts();
    Show sh;
    sh.unread[3] = 2;
    Input in;
    in.text = "привіт";
    const Layout L = layout(0, 300, 1.0f, 4, false, sh, in, t, estimate);
    const Line a = fromGame(Source::Whisper, "( From Nova ) : hey", 0xFFFF, t, 0);
    const std::vector<const Line*> lines{&a};
    const std::vector<Row> rows = wrap(lines, L.lines.w, L.px, estimate);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    paint(p, L, rows, in, sh, t, estimate);
    const uint32_t mid = p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)];
    CHECK((mid >> 24) > 0xC0);  // the glass
    const std::vector<Words> w = words(L, rows, in, sh, t, estimate);
    const auto has = [&](const std::string& s) {
        for (const Words& x : w)
            if (x.text == s) return true;
        return false;
    };
    CHECK(has("Усі") && has("Бій") && has("2") && has("привіт") && has("Nova") && has("hey"));
    in.text.clear();
    CHECK(!words(L, rows, in, sh, t, estimate).empty());
    float caretX = 0;
    in.text = std::string(200, 'w');
    in.caret = std::string::npos;
    const std::string shown = fieldText(L, in, estimate, caretX);
    CHECK(shown.size() < in.text.size() && shown.rfind("\xE2\x80\xA6", 0) == 0 && caretX <= L.field.w);
}

// 3.0.11-beta.240 (the chat concept, stage 1): the input's own commands - /p, /g, /w Name (a name with spaces in
// quotes), /r to the last who whispered; their hints while «/» is typed
TEST(chat_commands_and_their_hints) {
    chatpanel::Command c;
    CHECK(command("/p hi all", "", c) && c.mode == 1 && c.whisper.empty() && c.words == "hi all");
    CHECK(command("/g  hello", "", c) && c.mode == 2 && c.words == "hello");
    CHECK(command("/w Ann hi there", "", c) && c.whisper == "Ann" && c.words == "hi there");
    CHECK(command("/w \"Dark Lord\" sell?", "", c) && c.whisper == "Dark Lord" && c.words == "sell?");
    CHECK(command("/r ok", "Bob", c) && c.whisper == "Bob" && c.words == "ok");
    CHECK(!command("/r ok", "", c));       // no one whispered yet
    CHECK(!command("/p ", "", c));          // nothing to send
    CHECK(!command("/sit", "", c));         // the game's own
    CHECK(!command("/w Ann", "", c));       // no words
    CHECK(!command("hello /p x", "", c));
    CHECK(commandHints("/") && commandHints("/p") && !commandHints("/p x") && !commandHints("/sit") &&
          !commandHints("hi"));
    Texts t;
    t.commands[0] = "/p текст — у групу";
    t.commands[1] = "/g текст — у гільдію";
    t.commands[2] = "/w Ім'я текст — шепіт";
    t.commands[3] = "/r текст — відповісти {1}";
    Show sh;
    Input in;
    in.text = "/";
    in.replyTo = "Bob";
    const Layout L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    CHECK(L.hints.w > 0 && L.hints.y >= L.lines.y && L.hints.y + L.hints.h <= L.lines.y + L.lines.h + 0.5f);
    const std::vector<Words> w = words(L, {}, in, sh, t, estimate);
    CHECK(std::any_of(w.begin(), w.end(), [](const Words& x) { return x.text == "/r текст — відповісти Bob"; }));
}

// 3.0.11-beta.241 (the chat concept, stage 2: «размер мышью»): its corner dragged - the width and the rows it gives,
// its bottom left staying; held to 360..900 wide, 4..20 rows
TEST(chat_size_by_its_corner) {
    Texts t;
    Show sh;
    Input in;
    const Layout L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate, 640);
    CHECK(L.w == 640 && L.grip.w > 0 && L.grip.x + L.grip.w <= L.glass.w + 0.5f && L.grip.y < L.lines.y);
    CHECK(hitTest(L, {}, sh, L.grip.x + 4, L.grip.y + 4).part == Part::Grip);
    int w = 0, r = 0;
    sizeTo(L, float(L.x) + 800, float(L.y) - 18 * 3, w, r);  // wider, three rows taller
    CHECK(w == 800 && r == 10);
    sizeTo(L, float(L.x) + 100, float(L.y + L.h), w, r);  // too small: held
    CHECK(w == kLeastWidth && r == kLeastRows);
    sizeTo(L, float(L.x) + 5000, float(L.y) - 5000, w, r);
    CHECK(w == kMostWidth && r == kMostRows);
    const Layout Q = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate, 100);
    CHECK(Q.w == kLeastWidth);
}
