// 3.0.17-beta.4 (the concept «простой старт»: «Швидко» in Alt+D): the rows the player pinned (Insert), one key a line
#include "core/console_pins.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(console_pins_toggle_and_keep_order) {
    std::vector<std::string> pins = consolepins::parse("effects.grass\r\n\r\n  interface.hotbar \nnot a key!\n");
    CHECK(pins == (std::vector<std::string>{"effects.grass", "interface.hotbar"}));
    pins = consolepins::toggle(pins, "effects.torch_shadows");
    CHECK(pins.back() == "effects.torch_shadows");
    pins = consolepins::toggle(pins, "effects.grass");
    CHECK(pins == (std::vector<std::string>{"interface.hotbar", "effects.torch_shadows"}));
    CHECK(consolepins::parse(consolepins::format(pins)) == pins);
    CHECK(consolepins::pinned(pins, "interface.hotbar"));
    CHECK(!consolepins::pinned(pins, "effects.grass"));
    std::vector<std::string> many;
    for (int i = 0; i < 40; ++i) many = consolepins::toggle(many, "effects.k" + std::to_string(i));
    CHECK_EQ(many.size(), size_t(consolepins::kMost));  // the oldest go
    CHECK(many.back() == "effects.k39");
}
