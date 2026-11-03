// 3.0.11-beta.195 (the tester: «повідомлення про застосування скіла кимось (над головою у монка ...) перекриває це
// вікно»): the game's windows of the world - drawn under the driver's windows
#include <cstdint>

#include "core/world_windows.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(world_windows_the_bars_words_and_signs) {
    CHECK(worldwindows::worldWindow(0x696D4C));  // an actor's HP/SP gauge (3.0.11-beta.191)
    CHECK(worldwindows::worldWindow(0x696CB4));  // its other gauge
    CHECK(worldwindows::worldWindow(0x696C18));  // its cast's
    CHECK(worldwindows::worldWindow(0x696068));  // the words over a head: the chat, a skill's shout
    CHECK(worldwindows::worldWindow(0x6966A4));  // a chat room's sign
    CHECK(worldwindows::worldWindow(0x69673C));  // a shop's sign
}

TEST(world_windows_not_the_interface) {
    CHECK(!worldwindows::worldWindow(0x699248));  // the menu (UI window 0x30)
    CHECK(!worldwindows::worldWindow(0x698EB8));  // the skill card (0x2E)
    CHECK(!worldwindows::worldWindow(0x6988C8));  // the shop's bar (0x32)
    CHECK(!worldwindows::worldWindow(0));
}
