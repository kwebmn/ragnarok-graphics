// 3.0.17-beta.27 (the concept «простой старт», Task 6/8; the owner: «делай 1 и 2»): «Швидко»'s tiles, four a row,
// and the chosen row's «що це дає» - its hint's first sentence - over the console's keys
#include "core/console_tiles.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(console_tiles_four_a_row) {
    const auto r = consoletiles::rows({"a", "b", "c", "d", "e", "f"});
    CHECK_EQ(r.size(), size_t(2));
    CHECK(r[0] == (std::vector<std::string>{"a", "b", "c", "d"}));
    CHECK(r[1] == (std::vector<std::string>{"e", "f"}));
    CHECK(consoletiles::rows({}).empty());
    CHECK(consoletiles::split(consoletiles::join(r[1])) == r[1]);
}

TEST(console_tiles_the_choice_moves_within_its_row) {
    CHECK_EQ(consoletiles::move(0, 1, 4), 1);
    CHECK_EQ(consoletiles::move(3, 1, 4), 3);  // no wrap: the edge stays
    CHECK_EQ(consoletiles::move(0, -1, 4), 0);
    CHECK_EQ(consoletiles::move(5, 0, 2), 1);  // a shorter row below: its last
    CHECK_EQ(consoletiles::at(10, 0, 400, 4), 0);
    CHECK_EQ(consoletiles::at(250, 0, 400, 4), 2);
    CHECK_EQ(consoletiles::at(399, 0, 400, 2), 1);  // two tiles: the row's width for four, the rest empty -> its last
}

TEST(console_tiles_the_gist_of_a_hint) {
    CHECK_EQ(consoletiles::gist("Day and night on the map. Off: the game's light.", 80),
             std::string("Day and night on the map."));
    CHECK_EQ(consoletiles::gist("One line\nanother", 80), std::string("One line"));
    CHECK_EQ(consoletiles::gist("Монстр, по якому влучили, на мить спалахує білим", 20),
             std::string("Монстр, по якому…"));
    CHECK_EQ(consoletiles::gist("", 20), std::string());
    CHECK_EQ(consoletiles::gist("v1.5 shading. Rest", 80), std::string("v1.5 shading."));  // a dot inside a word stays
}
