// 3.0.17-beta.27 (the concept «простой старт», Task 8): the chosen row's «що це дає» - its hint's first sentence -
// over the console's keys, and under a row of «Налаштування». 3.0.21-beta.7: «Швидко»'s tiles are gone with the tab
#include <algorithm>

#include "core/console_tiles.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(console_tiles_the_gist_of_a_hint) {
    CHECK_EQ(consoletiles::gist("Day and night on the map. Off: the game's light.", 80),
             std::string("Day and night on the map."));
    CHECK_EQ(consoletiles::gist("One line\nanother", 80), std::string("One line"));
    CHECK_EQ(consoletiles::gist("Монстр, по якому влучили, на мить спалахує білим", 20),
             std::string("Монстр, по якому…"));
    CHECK_EQ(consoletiles::gist("", 20), std::string());
    CHECK_EQ(consoletiles::gist("v1.5 shading. Rest", 80), std::string("v1.5 shading."));  // a dot inside a word stays
}
