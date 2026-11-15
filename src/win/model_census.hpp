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

}  // namespace uagfx::win::modelcensus
