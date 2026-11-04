#include <cstdint>
#include <cstring>

#include "core/grid_cell.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.11-beta.215: the world's draw reads the game's flag at kRead with these bytes in this build (uaRO.exe dump)
TEST(grid_cell_read_is_the_games_flag_test) {
    uint8_t game[9];
    gridcell::original(game);
    const uint8_t want[9] = {0xA1, 0x80, 0xE7, 0x76, 0x00, 0x85, 0xC0, 0x75, 0x27};
    CHECK(std::memcmp(game, want, sizeof want) == 0);
    uint32_t flag = 0;
    std::memcpy(&flag, game + 1, 4);
    CHECK_EQ(flag, gridcell::kFlag);
    CHECK_EQ(gridcell::kRead + 9 + 0x27, 0x4FAB3Eu);  // past the draw's call: the cell not drawn
}

TEST(grid_cell_hidden_only_while_the_controller_plays_with_its_cursor_hidden) {
    CHECK(gridcell::hides(true, true));     // the arrow walk, the controller played with: the cursor is hidden
    CHECK(!gridcell::hides(true, false));   // the mouse played with
    CHECK(!gridcell::hides(false, true));   // the stick walks by the game's cursor: it is shown
    CHECK_EQ(gridcell::answer(0, false), 0u);  // the game's cell drawn
    CHECK_EQ(gridcell::answer(0, true), 1u);   // not drawn
    CHECK_EQ(gridcell::answer(1, false), 1u);  // the game has none either
    CHECK_EQ(gridcell::answer(1, true), 1u);
}
