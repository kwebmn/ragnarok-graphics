#include "core/sprite_cells.hpp"
#include "core/sprite_exe.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

uint8_t of(sprite_exe::Source s) { return uint8_t(s); }

}  // namespace

TEST(sprite_cells_the_newest_frame_where_the_cache_is_used_again) {
    // 2.7.12 (the tester on 2.7.11, hit flashes: the monster's quad, 37x48 at its place, taken for «interface»): the
    // game writes a frame where older ones lay - its cache's space used again - and the older cells stood ahead of it
    // in the list
    sprite_cells::Cells c;
    c.written(Rect{0, 0, 64, 64}, of(sprite_exe::Source::Interface), false);  // a cursor's frame once
    c.written(Rect{64, 0, 96, 32}, of(sprite_exe::Source::Player), false);
    c.written(Rect{16, 8, 53, 56}, of(sprite_exe::Source::Monster), false);  // a monster over part of the cursor's
    CHECK_EQ(int(c.at(30, 30, nullptr)), int(of(sprite_exe::Source::Monster)));
    CHECK_EQ(int(c.at(4, 4, nullptr)), 0);  // the cursor's frame is gone, written over
    CHECK_EQ(int(c.at(80, 10, nullptr)), int(of(sprite_exe::Source::Player)));  // untouched
    bool card = false;
    c.written(Rect{70, 40, 90, 60}, of(sprite_exe::Source::Item), true);
    CHECK(c.at(75, 45, &card) == of(sprite_exe::Source::Item) && card);
    CHECK(c.at(30, 30, &card) == of(sprite_exe::Source::Monster) && !card);
    c.written(Rect{16, 8, 53, 56}, of(sprite_exe::Source::Npc), false);  // the same cell again
    CHECK_EQ(int(c.at(30, 30, nullptr)), int(of(sprite_exe::Source::Npc)));
    CHECK_EQ(int(c.size()), 3);
    CHECK_EQ(int(c.at(200, 200, nullptr)), 0);  // nothing there
}

TEST(sprite_cells_keep_to_a_bound) {
    // a cache filled with ever new frames, none over another: the oldest go first past the bound
    sprite_cells::Cells c;
    for (int i = 0; i < sprite_cells::Cells::kMost + 10; ++i)
        c.written(Rect{i * 2, 0, i * 2 + 2, 2}, of(sprite_exe::Source::Monster), false);
    CHECK_EQ(int(c.size()), sprite_cells::Cells::kMost);
    CHECK_EQ(int(c.at(1, 1, nullptr)), 0);  // the first one went
    CHECK_EQ(int(c.at(float((sprite_cells::Cells::kMost + 9) * 2 + 1), 1, nullptr)),
             int(of(sprite_exe::Source::Monster)));
}
