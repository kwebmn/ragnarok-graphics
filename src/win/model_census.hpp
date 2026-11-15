#pragma once
#include <cstdint>

namespace uagfx::win::modelcensus {

// 3.0.20-beta.1 (stage 1 of «Модели нашим рендером»): the game's model node draw (core/model_census kNodeDraw) hooked -
// its time, nodes and faces a frame, and a few nodes a frame checked: their drawn vertices' colours against the colours
// in the game's memory; nothing drawn differently
void install();
// at the game's present: the frame's tally (the time since the last one), the log line every 30 s
void frame();
// the game's vertex pool grown at `at` by `n` vertices (win/exe_hot growHook) - the first in a node's own draw is its
// faces' block
void noteGrow(uint32_t vector, uint32_t at, uint32_t n);
// 3.0.20-beta.3 (stage 2a): the node being drawn had its vertices projected (win/exe_hot modelProjection) - its
// node-local points and the game's node-to-view matrix: a static node not yet sent on this map is captured whole
// (core/game_models) and sent to the renderer, a few a frame
void noteProjection(const float* points, const float* matrix, int count);
// a new map: the nodes captured so far let go (the renderer lets them go at the new map's first)
void mapChanged();
// 3.0.20-beta.4 (stage 2b): [view3d] own_models, each frame; before the frame's record goes to the renderer: the nodes
// left out of the game's draw so far this frame told (Msg::GameModelsFrame) when more came
void setOn(bool on);
void beforeRecord();
// 3.0.20-beta.22 (R-8UPTJW's gate, R-7VAPER's warehouse - their walls missing): the console's «Записати моделі
// навколо» - the next whole frame's nodes the game draws itself near the player written into the log (where they
// stand in the world, their faces, mirrored or not), ours counted, the renderer asked for its side with that frame
void dumpAround();

}  // namespace uagfx::win::modelcensus
