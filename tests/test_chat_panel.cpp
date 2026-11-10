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

// 3.0.18-beta.32 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5): uaUI's chat - the parts paint()
// decides beyond the layout: the unread badges, the channel chip's colour, the caret, the names' underlines
TEST(chat_view_for_uaui) {
    const Texts t = texts();
    Show sh;
    sh.unread[3] = 2;
    Input in;
    in.text = "привіт";
    in.typing = true;
    in.caretOn = true;
    const Layout L = layout(0, 300, 1.0f, 4, false, sh, in, t, estimate);
    const Line a = fromGame(Source::Whisper, "( From Nova ) : hey", 0xFFFF, t, 0);
    const std::vector<const Line*> lines{&a};
    const std::vector<Row> rows = wrap(lines, L.lines.w, L.px, estimate);
    View v = view(L, rows, in, sh, t, estimate);
    CHECK(v.badges.size() == L.tabs.size() && v.badges[3].w > 0 && v.badges[0].w == 0);
    CHECK(v.caret.w > 0 && v.caret.x >= L.field.x && v.chevron.w > 0 && v.underlines.empty());
    sh.hoverLine = 0;  // Nova's name pointed at: underlined
    v = view(L, rows, in, sh, t, estimate);
    CHECK(!v.underlines.empty());
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

// 3.0.11-beta.242 (the chat concept, stage 3: «шёпот по собеседникам»): on «Шепіт» a row of the ones whispered with
// («Усі» first, their unread), one chosen shows only theirs; a right click on a name - its menu
TEST(chat_whispers_by_person_and_a_names_menu) {
    Texts t;
    t.from = "від {1}";
    t.to = "до {1}";
    t.all = "Усі";
    t.menu[0] = "Шепнути";
    t.menu[1] = "Блокувати шепіт";
    t.menu[2] = "Копіювати ім'я";
    const Line ann = fromGame(Source::Whisper, "( From Ann ) : hi", 0, t, 0);
    const Line bob = fromGame(Source::WhisperResult, "( To Bob ) : yo", 0, t, 0);
    CHECK(ann.who == "Ann" && bob.who == "Bob");
    CHECK(showsWith(Tab::Whisper, "", ann) && showsWith(Tab::Whisper, "Ann", ann));
    CHECK(!showsWith(Tab::Whisper, "Ann", bob));
    CHECK(showsWith(Tab::All, "Ann", bob));  // the choice only on «Шепіт»
    Show sh;
    sh.tab = Tab::Whisper;
    sh.people = {"Ann", "Bob"};
    sh.peopleUnread = {2, 0};
    Input in;
    Layout L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    CHECK(L.people.size() == 3 && L.peopleRow.w > 0 && L.lines.y >= L.peopleRow.y + L.peopleRow.h);
    CHECK(hitTest(L, {}, sh, L.people[1].x + 3, L.people[1].y + 3).part == Part::Person);
    std::vector<Words> w = words(L, {}, in, sh, t, estimate);
    const auto has = [&](const std::string& x) {
        return std::any_of(w.begin(), w.end(), [&](const Words& y) { return y.text == x; });
    };
    CHECK(has("Усі") && has("Ann") && has("2") && has("Bob"));
    sh.tab = Tab::All;  // no row on another tab
    CHECK(layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate).people.empty());
    sh.menuLine = 0;
    sh.menuX = 100;
    sh.menuY = 120;
    L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    CHECK(L.menu.w > 0 && L.items.size() == 4);  // 3.0.11-beta.244: and «Копіювати рядок»
    CHECK(hitTest(L, {}, sh, L.items[2].x + 3, L.items[2].y + 3).part == Part::Item);
    w = words(L, {}, in, sh, t, estimate);
    CHECK(has("Копіювати ім'я"));
}

// 3.0.11-beta.243 (the chat concept, stage 4): «Усі»'s filters, the server's own lines short, «×N» on a line said
// again, a line's time when pointed at
TEST(chat_filters_short_lines_repeats_and_times) {
    Texts t;
    t.autoloot = "Автолут: до {1}";
    t.autolootOff = "Автолут вимкнено";
    t.expOn = "Показ досвіду ввімкнено";
    t.expGained = "Досвід: Base +{1} · Job +{2}";
    t.filters[0] = "Системні рядки";
    t.filters[1] = "Підібране й досвід";
    t.filters[2] = "Оголошення";
    t.filters[3] = "Торгівля";
    const Line loot = fromGame(Source::Other, "Autolooting items with drop rates of 100.00% and below.", 0, t, 0);
    CHECK(loot.kind == Kind::System && loot.spans.size() == 1 && loot.spans[0].text == "Автолут: до 100.00%");
    CHECK(fromGame(Source::Other, "Autoloot is now off.", 0, t, 0).spans[0].text == "Автолут вимкнено");
    const Line gained = fromGame(Source::Other, "Experience Gained Base:1442 (0.00%) Job:952 (0.00%)", 0, t, 0);
    CHECK(gained.kind == Kind::Loot && gained.spans[0].text == "Досвід: Base +1 442 · Job +952");
    CHECK(fromGame(Source::Other, "Something else.", 0, t, 0).spans[0].text == "Something else.");
    // the filters: «Усі» only
    const Line sale = fromGame(Source::Public, "Ann : S> Elu 50k", 0, t, 0);
    CHECK(sale.trade && showsWith(Tab::All, "", sale) && !showsWith(Tab::All, "", sale, kHideTrade));
    CHECK(!showsWith(Tab::All, "", loot, kHideSystem) && showsWith(Tab::All, "", loot, kHideTrade));
    CHECK(!showsWith(Tab::All, "", gained, kHideLoot) && showsWith(Tab::Trade, "", sale, kHideTrade));
    // «×3» after a line said three times
    Line again = fromGame(Source::Public, "Bob : hi", 0, t, 0);
    again.repeat = 3;
    const std::vector<const Line*> ls{&again};
    const std::vector<Row> rows = wrap(ls, 400, 12.5f, estimate);
    std::string all;
    for (const Piece& p : rows[0]) all += p.text;
    CHECK(all.find("\xC3\x97" "3") != std::string::npos);
    // the time: 90 s ago at 14:32:10 -> 14:30
    CHECK(clock(90000, 14 * 3600 + 32 * 60 + 10) == "14:30" && clock(120000, 30) == "23:58");
    // the gear on «Усі» and its menu
    Show sh;
    sh.filtersOpen = true;
    Input in;
    const Layout L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    CHECK(L.filtersButton.w > 0 && L.filtersButton.x + L.filtersButton.w <= L.grip.x + 0.5f && L.filters.size() == 4);
    CHECK(hitTest(L, {}, sh, L.filters[3].x + 3, L.filters[3].y + 3).part == Part::Filter);
}

// 3.0.11-beta.244 (the chat concept, stage 5): a thing's or a monster's name a link (its piece carries its id); a
// line's own menu copies it, a name's has all four
TEST(chat_links_and_a_lines_menu) {
    Line picked;
    picked.kind = Kind::Loot;
    picked.spans = {Span{"Підібрано: ", kMuted, false, false}, Span{"Jellopy", kText, true, false, 909, 0},
                    Span{" ×3", kMuted, false, false}};
    const std::vector<const Line*> ls{&picked};
    const std::vector<Row> rows = wrap(ls, 400, 12.5f, estimate);
    const auto link = std::find_if(rows[0].begin(), rows[0].end(), [](const Piece& p) { return p.item == 909; });
    CHECK(link != rows[0].end() && link->text.find("Jellopy") != std::string::npos);
    Texts t;
    Show sh;
    Input in;
    const Layout L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    const float top = L.lines.y + L.lines.h - L.rowH;
    const Hit h = hitTest(L, rows, sh, L.lines.x + link->x + 2, top + 4);
    CHECK(h.part == Part::Link && h.index == 909);
    sh.menuNamed = false;
    CHECK(menuAction(sh, 0) == 3);
    sh.menuNamed = true;
    CHECK(menuAction(sh, 0) == 0 && menuAction(sh, 3) == 3);
    sh.menuLine = 0;
    sh.menuNamed = false;
    CHECK(layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate).items.size() == 1);
}

// 3.0.11-beta.245 (the owner: «в чате нету скрола»; R-YCYYXU): the history's scroll bar - its thumb at the bottom for
// the newest, taken and dragged; the server's own lines (0x008e, no «name :») short and the system's
TEST(chat_scroll_bar_and_the_servers_own_lines) {
    Texts t;
    t.autoloot = "Автолут: до {1}";
    t.expOn = "Показ досвіду ввімкнено";
    const Line own = fromGame(Source::PublicOwn, "Autolooting items with drop rates of 100.00% and below.", 0, t, 0);
    CHECK(own.kind == Kind::System && own.spans[0].text == "Автолут: до 100.00%");
    const Line other = fromGame(Source::PublicOwn, "Welcome to the server!", 0, t, 0);
    CHECK(other.kind == Kind::System);
    const Line said = fromGame(Source::PublicOwn, "Me : hello", 0, t, 0);
    CHECK(said.kind == Kind::Public && said.who == "Me");
    Show sh;
    Input in;
    const Layout L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    CHECK(L.bar.w > 0 && L.bar.x >= L.lines.x + L.lines.w);
    CHECK(barThumb(L, 5, 0).w == 0);  // all shown: no bar
    const panel::Box newest = barThumb(L, 70, 0), oldest = barThumb(L, 70, 63);
    CHECK(newest.w > 0 && newest.y > oldest.y && std::fabs(newest.y + newest.h - (L.bar.y + L.bar.h)) < 0.5f);
    CHECK(scrollAt(L, 70, L.bar.y) == 63 && scrollAt(L, 70, L.bar.y + L.bar.h) == 0);
    std::vector<Row> rows(70);
    for (Row& r : rows) r.push_back(Piece{"x", kText, false, false, 0, 5, 0});
    CHECK(hitTest(L, rows, sh, L.bar.x + 2, L.bar.y + 10).part == Part::Bar);
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

// 3.0.18-beta.105 (the owner: «поиск в чате» - Ctrl+F only with words typed was hard to find): a magnifier in the
// tabs' row on every tab, left of «Усі»'s gear; its own part
TEST(chat_search_button_in_the_tabs_row) {
    const Texts t = texts();
    Show sh;
    Input in;
    Layout L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    CHECK(L.searchButton.w > 0);
    CHECK(L.searchButton.x + L.searchButton.w <= L.filtersButton.x + 0.5f);  // left of the gear
    CHECK(hitTest(L, {}, sh, L.searchButton.x + 4, L.searchButton.y + 4).part == Part::Search);
    sh.tab = Tab::Party;  // no gear there: the magnifier where the gear stands on «Усі»
    L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate);
    CHECK(L.searchButton.w > 0 && L.filtersButton.w <= 0);
    CHECK(L.searchButton.x + L.searchButton.w <= L.grip.x + 0.5f);
    CHECK(L.tabs.back().x + L.tabs.back().w <= L.searchButton.x);  // after the tabs
    sh.tab = Tab::All;  // the narrowest chat: never over the tabs (none when no room)
    L = layout(10, 1070, 1.0f, 7, false, sh, in, t, estimate, kLeastWidth);
    CHECK(L.searchButton.w <= 0 || L.tabs.back().x + L.tabs.back().w <= L.searchButton.x);
}
