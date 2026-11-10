// 3.0.11-beta.98 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's «Разговор
// с NPC»): the NPC talk in the new look (core/npc_talk) - the server's words with their ^RRGGBB colours wrapped to the
// window, its name from a «[Name]» first line, the menu's options numbered in two columns, the button the window waits
// for, the keys and the controller's buttons under it
#include <algorithm>
#include <string>
#include <vector>

#include "core/npc_talk.hpp"
#include "core/pad_glyphs.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

float measure(const std::string& t, float px, bool bold) { return float(t.size()) * px * (bold ? 0.6f : 0.55f); }

npctalk::Texts texts() {
    npctalk::Texts t;
    t.next = "Далі";
    t.close = "Закрити";
    t.keys = "1–8 вибір · Enter далі · Esc закрити";
    t.server = "Текст — як у сервера";
    t.choose = "вибір";
    t.ok = "OK";
    return t;
}

}  // namespace

TEST(npc_talk_colours_and_their_dark_ones_lit) {
    const std::vector<npctalk::Run> runs = npctalk::colours("Hello ^FF0000red^000000 and ^0000FFblue", 0xEEF2F8);
    CHECK(runs.size() == 4);
    CHECK(runs[0].text == "Hello " && runs[0].rgb == 0xEEF2F8u);
    CHECK(runs[1].text == "red" && runs[1].rgb == 0xFF0000u);
    CHECK(runs[2].text == " and " && runs[2].rgb == 0xEEF2F8u);  // black on the dark glass: the text's own colour
    CHECK(runs[3].text == "blue" && ((runs[3].rgb >> 16) & 0xFF) > 0x40);  // a dark blue made lighter
    CHECK(npctalk::colours("^nothing", 0xEEF2F8).size() == 1);  // not a colour: as it is
}

TEST(npc_talk_its_name_from_the_first_line) {
    npctalk::Talk t;
    t.lines = {"[Kafra Employee]", "Welcome to Kafra Corp."};
    const npctalk::Talk shown = npctalk::named(t);
    CHECK(shown.name == "Kafra Employee" && shown.lines.size() == 1 && shown.lines[0] == "Welcome to Kafra Corp.");
    npctalk::Talk plain;
    plain.lines = {"Hello there"};
    CHECK(npctalk::named(plain).name.empty() && npctalk::named(plain).lines.size() == 1);
}

// 3.0.11-beta.117 (R-2YRWU2: «неправильне відображення імені НПЦ, інтерфейс не розуміє кодів кольору»): a name line
// with the server's ^RRGGBB colours - around the brackets or inside them - names the NPC without them
TEST(npc_talk_its_name_without_the_colours) {
    npctalk::Talk t;
    t.lines = {"^0000FF[Guide]^000000", "Hello"};
    npctalk::Talk shown = npctalk::named(t);
    CHECK(shown.name == "Guide" && shown.lines.size() == 1 && shown.lines[0] == "Hello");
    t.lines = {"[^FF0000Prontera Guard^000000]", "Halt!"};
    shown = npctalk::named(t);
    CHECK(shown.name == "Prontera Guard" && shown.lines.size() == 1);
    t.lines = {"^0000FFJust words^000000"};
    CHECK(npctalk::named(t).name.empty() && npctalk::named(t).lines.size() == 1);  // no brackets: a line of words
}

TEST(npc_talk_layout_words_options_and_buttons) {
    npctalk::Talk t;
    t.name = "Kafra Employee";
    t.lines = {"Welcome to Kafra Corp. Always by your side, wherever you go. How may I be of help?"};
    t.options = {"Save", "Use Storage", "Use Teleport Service", "Cancel"};
    t.selected = 1;
    t.button = npctalk::Button::None;
    npctalk::Show sh;
    npctalk::Layout L = npctalk::layout(960, 900, 1.0f, t, sh, texts(), measure);
    CHECK(L.options.size() == 4 && L.options[1].x > L.options[0].x && L.options[2].y > L.options[0].y);  // 2 columns
    CHECK(L.button.w == 0 && L.lines.size() >= 1);
    CHECK(L.y + L.h <= 900 && std::abs(L.x + L.w / 2 - 960) <= 1);  // its bottom middle where asked
    const auto at = [&](const panel::Box& b) { return npctalk::hitTest(L, b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(at(L.options[3]).part == npctalk::Part::Option && at(L.options[3]).index == 3);
    std::vector<npctalk::Words> w = npctalk::words(L, t, sh, texts(), measure);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const npctalk::Words& x) { return x.text == text; });
    };
    CHECK(has("Kafra Employee") && has("1") && has("Use Storage") && has("4") && has("Текст — як у сервера"));
    t.options.clear();
    t.button = npctalk::Button::Next;
    L = npctalk::layout(960, 900, 1.0f, t, sh, texts(), measure);
    CHECK(L.button.w > 0 && at(L.button).part == npctalk::Part::Button);
    w = npctalk::words(L, t, sh, texts(), measure);
    CHECK(has("Далі"));
    sh.pad = true;
    L = npctalk::layout(960, 900, 1.0f, t, sh, texts(), measure);
    w = npctalk::words(L, t, sh, texts(), measure);
    CHECK(!has("1–8 вибір · Enter далі · Esc закрити") && has("Далі"));
    // 3.0.18-beta.31: uaUI's footer - a button for each of the controller's glyph boxes, A first
    const std::vector<int> buttons = npctalk::padButtons(t);
    CHECK(!buttons.empty() && buttons.size() == L.padGlyphs.size() && buttons[0] == int(padglyph::Button::A));
}

// 3.0.11-beta.195 (the tester: the Kafras' menus «у старому стилі» - in Glast Heim, the voting staff, Prontera's): a
// script's menu with no words before it (the game's menu window alone, no text window) - the talk shows its options
// alone, numbered, with nothing above them
TEST(npc_talk_a_menu_alone) {
    npctalk::Talk t;
    t.options = {"Save", "Teleport to Prontera", "Open Storage", "Cancel"};
    t.selected = 0;
    t = npctalk::named(t);
    CHECK(t.name.empty() && t.lines.empty());
    npctalk::Show sh;
    const npctalk::Layout L = npctalk::layout(960, 900, 1.0f, t, sh, texts(), measure);
    CHECK(L.options.size() == 4 && L.lines.empty() && L.button.w == 0);
    CHECK(L.h > 0 && L.y + L.h <= 900 && L.options[0].y >= L.body.y);
    for (size_t i = 0; i < L.options.size(); ++i) {
        const panel::Box& b = L.options[i];
        const npctalk::Hit h = npctalk::hitTest(L, b.x + b.w / 2, b.y + b.h / 2);
        CHECK(h.part == npctalk::Part::Option && h.index == int(i));
    }
    const std::vector<npctalk::Words> w = npctalk::words(L, t, sh, texts(), measure);
    CHECK(std::any_of(w.begin(), w.end(), [](const npctalk::Words& x) { return x.text == "Open Storage"; }));
}

// 3.0.11-beta.255 (the concept «Разговор с NPC с геймпада», stage 1): the menu's two columns by the D-pad, LB / RB by
// eight, A's wait after a new page, the option chosen last time kept by its menu
TEST(npc_talk_pad_menu_moves_wait_and_memory) {
    using npctalk::Move;
    using npctalk::moved;
    CHECK(moved(0, 5, Move::Down) == 2 && moved(2, 5, Move::Down) == 4 && moved(4, 5, Move::Down) == 4);
    CHECK(moved(3, 5, Move::Down) == 4);  // the last row has one: the odd column goes to it
    CHECK(moved(3, 6, Move::Down) == 5 && moved(1, 6, Move::Up) == 1 && moved(3, 6, Move::Up) == 1);
    CHECK(moved(0, 6, Move::Right) == 1 && moved(1, 6, Move::Right) == 1 && moved(1, 6, Move::Left) == 0);
    CHECK(moved(4, 5, Move::Right) == 4);  // nothing to its right
    CHECK(moved(3, 30, Move::PageDown) == 11 && moved(27, 30, Move::PageDown) == 29 && moved(5, 30, Move::PageUp) == 0);
    CHECK(moved(0, 0, Move::Down) == -1);
    CHECK(npctalk::waiting(1000, 1000) == 1 && npctalk::waiting(1000, 1200) > 0.4f &&
          npctalk::waiting(1000, 1000 + npctalk::kGuardMs) == 0 && npctalk::waiting(0, 5) == 0);
    npctalk::Talk t;
    t.name = "Kafra Employee";
    t.place = "prontera";
    t.options = {"Save", "Teleport", "Storage"};
    npctalk::Talk u = t;
    u.options.push_back("Cancel");
    CHECK(npctalk::menuKey(t) != npctalk::menuKey(u));
    std::map<std::string, int> c{{npctalk::menuKey(t), 1}, {npctalk::menuKey(u), 3}};
    const std::map<std::string, int> back = npctalk::readChoices(npctalk::writeChoices(c));
    CHECK(back == c);
    CHECK(npctalk::readChoices("x\tbroken\n\tno index\n5").empty());
}

// 3.0.12-beta.2 (a player's wish: «зробити трохи менші ... вікна чатів з НПЦ»): as wide as its words - a few short
// lines a narrow window, a long line up to the old width, the options' columns room for their words
TEST(npc_talk_window_as_wide_as_its_words) {
    npctalk::Talk t;
    t.name = "Employee";
    t.lines = {"Welcome", "How can I help you?"};
    t.button = npctalk::Button::Next;
    npctalk::Show sh;
    npctalk::Layout L = npctalk::layout(960, 900, 1.0f, t, sh, texts(), measure);
    CHECK(L.w >= 460 && L.w <= 462);
    CHECK(std::abs(L.x + L.w / 2 - 960) <= 1);
    t.lines = {std::string(200, 'w')};
    L = npctalk::layout(960, 900, 1.0f, t, sh, texts(), measure);
    CHECK(L.w == 860);
    t.lines = {"Hi"};
    t.options = {std::string(40, 'o'), "Cancel"};
    t.button = npctalk::Button::None;
    L = npctalk::layout(960, 900, 1.0f, t, sh, texts(), measure);
    CHECK(L.w > 460 && L.w < 860 && L.options[0].w >= measure(t.options[0], 14, false));
}

// 3.0.12-beta.3 (a player's screenshot: «  Слава Україні» indented): a line's leading spaces dropped
TEST(npc_talk_drops_a_lines_leading_spaces) {
    npctalk::Talk t;
    t.lines = {"   Glory", "^FF0000  Red^000000 words"};
    t.button = npctalk::Button::Next;
    npctalk::Show sh;
    const npctalk::Layout L = npctalk::layout(960, 900, 1.0f, t, sh, texts(), measure);
    CHECK(!L.lines.empty() && L.lines[0].text == "Glory");
    bool red = false;
    for (const npctalk::Placed& p : L.lines)
        if (p.text == "Red") red = p.box.x == L.lines[0].box.x;  // its colour made readable, its place the edge
    CHECK(red);
}
