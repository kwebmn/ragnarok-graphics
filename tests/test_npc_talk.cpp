// 3.0.11-beta.98 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's «Разговор
// с NPC»): the NPC talk in the new look (core/npc_talk) - the server's words with their ^RRGGBB colours wrapped to the
// window, its name from a «[Name]» first line, the menu's options numbered in two columns, the button the window waits
// for, the keys and the controller's buttons under it
#include <algorithm>
#include <string>
#include <vector>

#include "core/npc_talk.hpp"
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
}
