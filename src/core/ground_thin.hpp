#pragma once
#include <cstdint>

namespace uagfx::ground_thin {

// 3.0.19-beta.42 (item 5 of the owner's «делаем все по порядку»: the speed): in the 3D view the game works out each of
// its ground's cells (uaRO.exe FUN_004E5E20: its four corners projected, its face made and sorted - some 8000 a frame
// to the sight) and the driver records each, while the renderer's own ground is drawn in their place (the game's
// draws then only teach it their colours and fog). A cell farther than kNearDepth (its view depth: the reflections'
// and the torches' shadows' casters reach 600 in the 3D view, and a cell's side) only one frame in kEvery - its turn by
// its vertices' address -, the near ones every frame
constexpr float kNearDepth = 620.0f;
constexpr uint32_t kEvery = 8;
bool workedOut(float viewDepth, uint32_t key, uint32_t frame);

}  // namespace uagfx::ground_thin
