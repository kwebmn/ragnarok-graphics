// 3.0.16-beta.31 (the owner: «делай» - the homunculus' and the mercenary's skills in the new style): the use level
// chosen (‹ ›) as the game's own skills window keeps it, a press on a row moved far a drag onto the hotbar
#include <string>
#include <utility>

#include "core/companion_skills.hpp"
#include "core/homun_card.hpp"
#include "core/merc_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

TEST(companion_use_level_and_drag) {
    CHECK_EQ(companion::useOf(0, 5), 5);  // none kept: its level
    CHECK_EQ(companion::useOf(3, 5), 3);
    CHECK_EQ(companion::useOf(7, 5), 5);  // above its level (it fell): its level
    CHECK_EQ(companion::stepped(3, 5, -1), 2);
    CHECK_EQ(companion::stepped(1, 5, -1), 1);
    CHECK_EQ(companion::stepped(5, 5, +1), 5);
    CHECK(!companion::dragged(10, 10, 14, 13, 1.0f));
    CHECK(companion::dragged(10, 10, 17, 10, 1.0f));
    const panel::Box row{16, 100, 288, 20};
    const companion::RowParts raise = companion::rowParts(row, 1.0f, true);
    const companion::RowParts plain = companion::rowParts(row, 1.0f, false);
    CHECK(raise.less.x < raise.level.x && raise.level.x < raise.more.x);
    CHECK(raise.more.x + raise.more.w <= row.x + row.w - 22);  // the raise «+» keeps its place
    CHECK(plain.more.x > raise.more.x);
}

TEST(companion_rows_in_the_cards) {
    const homuncard::Layout h = homuncard::layout(1920, 1080, 1.0f, 3);
    CHECK(h.skillLess.size() == 3 && h.skillMore.size() == 3);
    auto [lx, ly] = mid(h.skillLess[1]);
    CHECK(homuncard::hitTest(h, lx, ly).part == homuncard::Part::Less && homuncard::hitTest(h, lx, ly).index == 1);
    auto [mx, my] = mid(h.skillMore[2]);
    CHECK(homuncard::hitTest(h, mx, my).part == homuncard::Part::More);
    auto [px, py] = mid(h.skillPlus[0]);
    CHECK(homuncard::hitTest(h, px, py).part == homuncard::Part::Plus);
    CHECK(homuncard::hitTest(h, h.skillRows[0].x + 10, h.skillRows[0].y + 5).part == homuncard::Part::Skill);
    const merccard::Layout m = merccard::layout(1920, 1080, 1.0f, 2);
    auto [rx, ry] = mid(m.skillLess[0]);
    CHECK(merccard::hitTest(m, rx, ry).part == merccard::Part::Less);
    CHECK(merccard::hitTest(m, m.skillRows[1].x + 10, m.skillRows[1].y + 5).part == merccard::Part::Skill);
    CHECK(merccard::hitTest(m, m.skillRows[1].x + 10, m.skillRows[1].y + 5).index == 1);
    // «Рів. 3/5» while used below its level
    homuncard::Info i;
    i.skills.push_back(homuncard::SkillRow{"Healing Touch", 5, false, 3});
    homuncard::Texts t;
    t.skillLevel = "Lv {1}";
    const homuncard::Layout one = homuncard::layout(1920, 1080, 1.0f, 1);
    bool seen = false;
    for (const homuncard::Words& w : homuncard::words(one, i, homuncard::Show{}, t)) seen = seen || w.text == "Lv 3/5";
    CHECK(seen);
}
