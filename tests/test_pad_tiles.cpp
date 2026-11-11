// 3.0.11-beta.90 (the concept «Контроллер в интерфейсе uaRO», stage 6): Start on the controller opens a tile of the
// game's windows - nine in three rows, the chosen one in gold, the buttons' hints under them (core/pad_tiles)
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/pad_tiles.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(pad_tiles_layout_and_moves) {
    const padtiles::Layout L = padtiles::layout(960, 540, 1.0f);
    CHECK_EQ(L.tiles.size(), size_t(padtiles::kTiles));
    CHECK(L.x + L.w / 2 == 960 && L.y + L.h / 2 == 540);
    CHECK(L.tiles[1].x > L.tiles[0].x + L.tiles[0].w && L.tiles[3].y > L.tiles[0].y + L.tiles[0].h);
    const panel::Box& t4 = L.tiles[4];
    CHECK_EQ(padtiles::hitTest(L, t4.x + t4.w / 2, t4.y + t4.h / 2), 4);
    CHECK_EQ(padtiles::hitTest(L, 1, 1), -1);
    CHECK_EQ(padtiles::moved(0, -1, 0), 2);  // round its row
    CHECK_EQ(padtiles::moved(0, 0, -1), 6);  // round its column
    CHECK_EQ(padtiles::moved(4, 1, 1), 8);
}

// 3.0.11-beta.216 (the owner: «в интерфейсе нету управления наемниками»): a hired mercenary adds a tenth tile,
// «Найманець», alone in a fourth row under the middle column; the frame reaches it down the middle and comes back up
TEST(pad_tiles_tenth_for_the_mercenary) {
    const padtiles::Layout L = padtiles::layout(960, 540, 1.0f, padtiles::kMostTiles);
    CHECK_EQ(L.tiles.size(), size_t(10));
    CHECK_EQ(int(padtiles::Tile::Merc), 9);
    const padtiles::Layout nine = padtiles::layout(960, 540, 1.0f);
    CHECK(L.h > nine.h && L.w == nine.w);
    const panel::Box& m = L.tiles[9];
    CHECK(m.y > L.tiles[7].y + L.tiles[7].h && std::abs(m.x - L.tiles[7].x) < 1.0f);  // under the middle column
    CHECK(L.hints.y > m.y + m.h);
    CHECK_EQ(padtiles::moved(7, 0, 1, 10), 9);
    CHECK_EQ(padtiles::moved(6, 0, 1, 10), 9);  // any column down from the third row
    CHECK_EQ(padtiles::moved(9, 0, 1, 10), 1);  // round to the top, the middle
    CHECK_EQ(padtiles::moved(9, 0, -1, 10), 7);
    CHECK_EQ(padtiles::moved(9, 1, 0, 10), 9);  // alone in its row
    CHECK_EQ(padtiles::moved(1, 0, -1, 10), 9);
    CHECK_EQ(padtiles::moved(0, 0, -1, 10), 9);
    CHECK_EQ(padtiles::moved(0, 0, -1), 6);  // nine as before
}

// 3.0.18-beta.35: uaUI's tiles - each tile's icon as the picture's (the first «Речі»'s bag), past the last none
TEST(pad_tiles_icon_of_a_tile) {
    CHECK(padtiles::iconOf(0) != padtiles::iconOf(1));
    CHECK(padtiles::iconOf(-1) == uiicons::Icon::Search);
}

// 3.0.18-beta.124 (the concept «Ігровий інтерфейс»: the second row's windows on the controller too): any number of
// tiles three to a row, a short last row in the middle; up and down to the nearest column; our windows' icons
TEST(pad_tiles_any_count) {
    const padtiles::Layout L = padtiles::layout(960, 540, 1.0f, 13);
    CHECK_EQ(L.tiles.size(), size_t(13));
    CHECK(std::abs(L.tiles[12].x - L.tiles[1].x) < 1.0f);  // alone: the middle column
    CHECK_EQ(padtiles::moved(9, 0, 1, 13), 12);
    CHECK_EQ(padtiles::moved(11, 0, 1, 13), 12);
    CHECK_EQ(padtiles::moved(12, 0, 1, 13), 1);
    CHECK_EQ(padtiles::moved(12, 0, -1, 13), 10);
    const padtiles::Layout two = padtiles::layout(960, 540, 1.0f, 14);  // two in the last row, between the columns
    CHECK(two.tiles[12].x > two.tiles[9].x && two.tiles[13].x < two.tiles[11].x);
    CHECK_EQ(padtiles::moved(12, 1, 0, 14), 13);
    CHECK_EQ(padtiles::moved(13, 1, 0, 14), 12);
    CHECK(padtiles::iconOf(int(padtiles::Tile::Hunt)) == uiicons::Icon::Battle);
    CHECK(padtiles::iconOf(int(padtiles::Tile::Recap)) == uiicons::Icon::Warn);
    // 3.0.18-beta.136: the hotbar's profile tile - fifteen in all
    CHECK(padtiles::iconOf(int(padtiles::Tile::Profile)) == uiicons::Icon::Edit);
    CHECK_EQ(padtiles::layout(960, 540, 1.0f, 15).tiles.size(), size_t(15));
}
