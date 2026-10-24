#include <string>

#include "core/config.hpp"
#include "core/platform.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.8 (the user's idea 7): Linux and the Steam Deck through Wine (Proton)
TEST(platform_described_for_the_log) {
    CHECK_EQ(platform::describe(platform::Facts{}), std::string("Windows"));
    platform::Facts deck;
    deck.wine = true;
    deck.wineVersion = "9.0";
    deck.proton = true;
    deck.steamDeck = true;
    CHECK_EQ(platform::describe(deck), std::string("Wine 9.0 (Proton) on a Steam Deck"));
    platform::Facts linux;
    linux.wine = true;
    linux.wineVersion = "10.0";
    CHECK_EQ(platform::describe(linux), std::string("Wine 10.0"));
}

TEST(platform_no_64_bit_renderer_under_wine_unless_asked) {
    CHECK(platform::bridgeAllowed(platform::Facts{}, false));  // Windows: as the settings say
    platform::Facts wine;
    wine.wine = true;
    CHECK(!platform::bridgeAllowed(wine, false));  // its picture crosses processes: no DirectComposition there
    CHECK(platform::bridgeAllowed(wine, true));    // a tester's [renderer] bridge_wine = 1
    CHECK(!parseConfig("").bridgeWine);
    CHECK(parseConfig("[renderer]\nbridge_wine = 1\n").bridgeWine);
}
