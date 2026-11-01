#pragma once
#include <cstdint>

namespace uagfx::win::netpredictwin {

// 3.0.11-beta.130 (the network plan's step 5, the own walk at once; the owner «делаем»): [interface] predict_walk, an
// experiment (off) - the game's walk request (0x00a7, seen by the pad's send hook) guessed at the next frame's
// beginning when the character stands still and casts nothing: the game's player told the walk (its message 0, the
// struct FUN_0057e1f0 hands it: the server's clock now, from its cell to the one asked - core/net_predict); the
// server's 0x0087 the same way both ends - its tick made the guess's before the game reads it (the game walks on from
// where it is); another - the server's as before; none within four round trips and 200 ms (at least 600) - the player
// put back on its cell (message 0x13). On the game's thread.
void setOn(bool on);
void walkAsked(int x, int y);   // the game sent its walk request - the pad's send hook
void ownMove(uint8_t* packet);  // ZC_NOTIFY_PLAYERMOVE before the game reads it - the packet tap
void beginScene();              // each frame before the scene (Device7::BeginScene)

}  // namespace uagfx::win::netpredictwin
