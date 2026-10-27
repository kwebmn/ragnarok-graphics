// 3.0.9-beta.4: the developer's answer in the game - the card's shapes (core/answer_card); its words by GDI
#include <string>
#include <vector>

#include "core/answer_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool inside(const panel::Box& b, const card::Geometry& g) {
    return b.x >= 0 && b.y >= 0 && b.x + b.w <= float(g.width) + 0.01f && b.y + b.h <= float(g.height) + 0.01f;
}

size_t letters(const std::string& s) {  // UTF-8 characters
    size_t n = 0;
    for (unsigned char ch : s) n += (ch & 0xC0) != 0x80;
    return n;
}

}  // namespace

TEST(answer_card_fits_inside_the_picture) {
    const card::Widths w{110, 190, 60};
    const card::Geometry g = card::geometry(1.0f, 1920, 2, w);
    CHECK_EQ(g.width, 460);
    for (const panel::Box& b : {g.dot, g.title, g.chip, g.quote, g.open, g.later, g.time}) CHECK(inside(b, g));
    CHECK(g.open.x + g.open.w <= g.later.x);
    CHECK(g.chip.x > g.title.x);
    const panel::Point at = card::position(g, 1920);
    CHECK_EQ(at.x, (1920 - 460) / 2);
    CHECK_EQ(at.y, 8);
    const card::Geometry one = card::geometry(1.0f, 1920, 1, w);  // one line of words: lower
    CHECK(one.height < g.height);
    const card::Geometry narrow = card::geometry(1.0f, 300, 2, w);  // an 800x600 game at a big scale
    CHECK_EQ(narrow.width, 284);
    CHECK_EQ(card::position(narrow, 300).x, 8);
    CHECK_EQ(card::geometry(2.0f, 3840, 2, card::Widths{220, 380, 120}).width, 920);
}

TEST(answer_card_knows_its_buttons) {
    const card::Geometry g = card::geometry(1.0f, 1920, 2, card::Widths{110, 190, 60});
    CHECK(card::hitTest(g, g.open.x + g.open.w / 2, g.open.y + g.open.h / 2) == card::Part::Open);
    CHECK(card::hitTest(g, g.later.x + g.later.w / 2, g.later.y + g.later.h / 2) == card::Part::Later);
    CHECK(card::hitTest(g, g.quote.x + 5, g.quote.y + 5) == card::Part::Body);  // the card itself opens it too
    CHECK(card::hitTest(g, -1, -1) == card::Part::None);
    CHECK(card::hitTest(g, float(g.width) + 1, 5) == card::Part::None);
}

TEST(answer_card_lines_keep_words_whole) {
    const auto twenty = [](const std::string& s) { return letters(s) <= 20; };
    const std::vector<std::string> l = card::fitLines(
        "Надішліть, будь ласка, звернення з увімкненим Берсерком і натиснутою Pause", 2, twenty);
    CHECK_EQ(l.size(), size_t(2));
    if (l.size() == 2) {
        CHECK_EQ(l[0], std::string("Надішліть, будь"));
        CHECK_EQ(l[1], std::string("ласка, звернення з…"));
    }
    const auto ten = [](const std::string& s) { return letters(s) <= 10; };
    const std::vector<std::string> long1 = card::fitLines("ааааааааааааааааааааааааа", 2, ten);  // 25: cut
    CHECK_EQ(long1.size(), size_t(2));
    if (long1.size() == 2) {
        CHECK_EQ(long1[0], std::string("аааааааааа"));
        CHECK_EQ(long1[1], std::string("ааааааааа…"));
    }
    CHECK(card::fitLines("", 2, ten).empty());
    CHECK(card::fitLines("  \n ", 2, ten).empty());
    const std::vector<std::string> two = card::fitLines("a\nb", 2, ten);  // its lines' ends as spaces
    CHECK_EQ(two.size(), size_t(1));
    if (!two.empty()) CHECK_EQ(two[0], std::string("a b"));
}

TEST(answer_card_colours_say_the_status) {
    CHECK_EQ(card::statusColour(reports::Status::NeedInfo), 0xF0A050u);
    CHECK_EQ(card::statusColour(reports::Status::Fixed), 0x4CAF6Eu);
    CHECK_EQ(card::statusColour(reports::Status::Read), 0x5B8DEFu);
    const card::Geometry g = card::geometry(1.0f, 1920, 1, card::Widths{110, 190, 60});
    panel::Picture p, hovered;
    card::paint(p, g, reports::Status::NeedInfo, card::Part::None);
    card::paint(hovered, g, reports::Status::NeedInfo, card::Part::Open);
    CHECK_EQ(p.width, g.width);
    CHECK_EQ(p.height, g.height);
    const size_t mid = size_t(g.open.y + g.open.h / 2) * size_t(p.width) + size_t(g.open.x + g.open.w / 2);
    CHECK(mid < p.px.size());
    if (mid >= p.px.size()) return;
    CHECK_EQ(p.px[mid] >> 24, 0xFFu);  // the button opaque
    CHECK((p.px[mid] & 0xFF) > ((p.px[mid] >> 16) & 0xFF));  // blue
    CHECK(hovered.px[mid] != p.px[mid]);  // under the mouse: lighter
    CHECK_EQ(p.px[0] >> 24, 0u);  // the rounded corner: nothing
}
