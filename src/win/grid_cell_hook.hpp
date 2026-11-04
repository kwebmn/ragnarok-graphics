#pragma once

namespace uagfx::win {

// 3.0.11-beta.215 (core/grid_cell): the game's cell under the mouse read through the driver - none while the controller
// plays with the game's cursor hidden. false, with a log line, when the exe is another build.
bool installGridCell();
// Each frame: whether the cell is hidden now (the arrow walk and the controller played with)
void hideGridCell(bool hidden);

}  // namespace uagfx::win
