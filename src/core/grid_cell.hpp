#pragma once
#include <cstdint>

namespace uagfx::gridcell {

// 3.0.11-beta.215 (the owner: «когда играю на геймпаде, то курсор под мышкой которая отключена (клеточка на
// карте) пристуствует»): the game's cell under the mouse - grid.tga on the ground. The ground's mouse handler
// FUN_005664d0 (with Alt+Home's switch 0x772D3C on) picks the cell under the cursor every frame: the cell 0x76E778 /
// 0x76E77C, its colour 0x76E784, shown while kFlag is 0; the world's draw FUN_004fa8b0 reads kFlag at kRead (mov eax,
// [kFlag]; test eax, eax; jne past the call of the ground's slot 0x38, FUN_004e7370). The driver hides the game's
// cursor while the controller plays (win/context hidePadCursor) - the mouse stays where it was and its cell went on
// showing (moving with the camera). Read through the driver instead: no cell while the cursor is hidden.
constexpr uint32_t kRead = 0x4FAB0E;
constexpr uint32_t kFlag = 0x76E780;
void original(uint8_t out[9]);  // what this build has at kRead
bool hides(bool arrowWalk, bool padShown);
uint32_t answer(uint32_t gameFlag, bool hidden);  // what the draw reads: non-zero draws no cell

}  // namespace uagfx::gridcell
