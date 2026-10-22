// 2.15: the effects' console opens on Ctrl + the key left of 1 - alone that key types «ё» (Russian) and the apostrophe
// (Ukrainian) in the game's chat.
#include "core/console_key.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(the_console_wants_ctrl_with_the_key_left_of_1) {
    constexpr unsigned kLeftOf1 = 0xC0;  // VK_OEM_3
    CHECK(console::toggles(true, kLeftOf1));
    CHECK(!console::toggles(false, kLeftOf1));  // «ё» and «'» are typed
    CHECK(!console::toggles(true, 'E'));
    CHECK_EQ(console::kKey, kLeftOf1);
}
