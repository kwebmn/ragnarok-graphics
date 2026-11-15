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

}  // namespace uagfx::win::modelcensus
