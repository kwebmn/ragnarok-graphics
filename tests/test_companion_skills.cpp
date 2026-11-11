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

