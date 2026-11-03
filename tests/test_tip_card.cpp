// 3.0.11-beta.204 (the tester's third list: «Tip of the Day» still in the game's old look): the game's tip window as a
// card in style A (core/tip_card) - the tip's words, its number of all, «Назад», «Далі», «Закрити» and the game's own
// «show at the start» box
#include <algorithm>
#include <string>
#include <vector>

#include "core/tip_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

tipcard::Texts texts() {
    tipcard::Texts t;
    t.title = "Порада дня";
    t.back = "Назад";
    t.next = "Далі";
    t.close = "Закрити";
    t.startup = "Показувати під час входу в гру";
    t.closeKey = "Esc";
    return t;
}

tipcard::Card card(int rows) {
    tipcard::Card c;
    c.paragraphs = {"/savechat : сохраняет чат в текстовом файле."};
    c.number = 3;
    c.count = 42;
    c.rows = rows;
    return c;
}

bool has(const std::vector<tipcard::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const tipcard::Words& x) { return x.text == text; });
}

}  // namespace

// tipOfTheday.txt's lines between its '#' lines: the first one's «* » off, every line's spaces at its ends, empty ones
// out; a «- » note kept as the file has it
TEST(tip_card_paragraphs_from_the_game_lines) {
    CHECK((tipcard::paragraphs({"* /savechat : сохраняет чат.", ""}) ==
           std::vector<std::string>{"/savechat : сохраняет чат."}));
    CHECK((tipcard::paragraphs({"* Камера.", "   Двойной щелчок.\r", "- Второй игрок.", "  "}) ==
           std::vector<std::string>{"Камера.", "Двойной щелчок.", "- Второй игрок."}));
    CHECK(tipcard::paragraphs({}).empty());
    CHECK((tipcard::paragraphs({"*Без пробела"}) == std::vector<std::string>{"Без пробела"}));
}

// The game's window's own ids: Back 0xB4, Next 0x98, Close 0x87, the start box 0x93 (its SendMsg's message 6)
TEST(tip_card_game_buttons) {
    using P = tipcard::Part;
    CHECK_EQ(tipcard::gameButton(P::Back), 0xB4);
    CHECK_EQ(tipcard::gameButton(P::Next), 0x98);
    CHECK_EQ(tipcard::gameButton(P::Close), 0x87);
    CHECK_EQ(tipcard::gameButton(P::Startup), 0x93);
    CHECK_EQ(tipcard::gameButton(P::None), 0);
}

TEST(tip_card_counter) {
    CHECK(tipcard::counter(3, 42) == "3 / 42");
    CHECK(tipcard::counter(0, 0).empty());
    CHECK(tipcard::counter(5, 0).empty());
}

// At the screen's middle across; the words' room grows with their rows (three at least, twelve at most); the start
// box over the buttons' row; each part found where it is drawn
TEST(tip_card_layout_and_parts) {
    const tipcard::Layout a = tipcard::layout(card(2), 1920, 1080, 1.0f, texts());
    const tipcard::Layout b = tipcard::layout(card(6), 1920, 1080, 1.0f, texts());
    const tipcard::Layout c = tipcard::layout(card(40), 1920, 1080, 1.0f, texts());
    CHECK(a.w > 300 && a.w < 600);
    CHECK_EQ(a.x, 1920 / 2 - a.w / 2);
    CHECK(b.text.h > a.text.h);
    CHECK(c.text.h <= 12 * 19.0f + 0.5f);
    CHECK(a.text.h >= 3 * 19.0f - 0.5f);
    CHECK(a.tick.y + a.tick.h <= a.back.y);
    CHECK(a.back.y == a.next.y && a.next.y == a.close.y);
    CHECK(a.back.x < a.next.x && a.next.x < a.close.x);
    CHECK(a.close.x + a.close.w <= float(a.w));
    using P = tipcard::Part;
    const auto mid = [](const panel::Box& x) { return std::pair<float, float>{x.x + x.w / 2, x.y + x.h / 2}; };
    CHECK(tipcard::partAt(a, mid(a.back).first, mid(a.back).second) == P::Back);
    CHECK(tipcard::partAt(a, mid(a.next).first, mid(a.next).second) == P::Next);
    CHECK(tipcard::partAt(a, mid(a.close).first, mid(a.close).second) == P::Close);
    CHECK(tipcard::partAt(a, mid(a.tick).first, mid(a.tick).second) == P::Startup);
    CHECK(tipcard::partAt(a, mid(a.tickLabel).first, mid(a.tickLabel).second) == P::Startup);
    CHECK(tipcard::partAt(a, mid(a.text).first, mid(a.text).second) == P::None);
    const tipcard::Layout big = tipcard::layout(card(2), 3840, 2160, 2.0f, texts());
    CHECK(big.w >= 2 * a.w - 2);
    CHECK(big.y + big.h <= 2160);
}

// The words: the title, the number of all, the tip's paragraphs (broken into rows at the box), the buttons; the box's
// tick only while the game shows the tips at the start
TEST(tip_card_words) {
    tipcard::Card c = card(2);
    c.paragraphs = {"Камера.", "Двойной щелчок."};
    const tipcard::Layout L = tipcard::layout(c, 1920, 1080, 1.0f, texts());
    std::vector<tipcard::Words> w = tipcard::words(L, c, tipcard::Show{}, texts());
    CHECK(has(w, "Порада дня"));
    CHECK(has(w, "3 / 42"));
    CHECK(has(w, "Камера.\nДвойной щелчок."));
    CHECK(has(w, "Назад") && has(w, "Далі") && has(w, "Закрити"));
    CHECK(has(w, "Показувати під час входу в гру"));
    CHECK(has(w, "\xE2\x9C\x93"));
    const auto text = std::find_if(w.begin(), w.end(), [](const tipcard::Words& x) { return x.wrap; });
    CHECK(text != w.end() && text->box.w == L.text.w);
    c.startup = false;
    w = tipcard::words(L, c, tipcard::Show{}, texts());
    CHECK(!has(w, "\xE2\x9C\x93"));
}

// The words' room across (their rows measured against it by the window part)
TEST(tip_card_text_width) {
    CHECK(tipcard::textWidth(1.0f) > 300);
    CHECK(tipcard::textWidth(2.0f) > 2 * tipcard::textWidth(1.0f) - 1);
}

// The tip's rows at a width: broken at spaces (a word longer than the width stays whole), its own line breaks kept
TEST(tip_card_rows_at_a_width) {
    const auto measure = [](const std::string& w) { return float(w.size()); };
    CHECK_EQ(tipcard::rows("aaa bbb ccc", 8, measure), 2);
    CHECK_EQ(tipcard::rows("aaa bbb ccc", 100, measure), 1);
    CHECK_EQ(tipcard::rows("aaa\nbbb", 100, measure), 2);
    CHECK_EQ(tipcard::rows("", 10, measure), 1);
    CHECK_EQ(tipcard::rows("aaaaaaaaaaaa", 5, measure), 1);
    CHECK_EQ(tipcard::rows("aa bb\ncc dd ee", 6, measure), 3);
}
