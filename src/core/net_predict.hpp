#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::netpredict {

// 3.0.11-beta.130 (the network plan's step 5, the own walk at once; the owner «делаем» - a switch of its own, off at
// first): the client walks the character only after the server's answer (ZC_NOTIFY_PLAYERMOVE 0x0087, a round trip
// after the click). Guessed instead: at the click the game's player told the walk itself (its message 0, as the
// client's FUN_0057e1f0 tells it: the server's clock now, from its cell to the one asked); the server's answer, the same
// cells both ends, joined - its tick made the guess's, so the game walks on from where it is; another way: the
// server's, as before; none in time: the server did not walk it - put back. No Windows here (win/net_predict).

struct Move {  // as FUN_0057e1f0 hands it to the player (its message 0's struct, seven dwords in this order)
    uint32_t tick = 0;
    int srcX = 0, srcY = 0, subX = 8, subY = 8, dstX = 0, dstY = 0;
};
// The packet (12 bytes): its tick at +2, the way at +6 - x0 y0 x1 y1 10 bits each, the sub-cell x then y 4 each
bool readOwnMove(const uint8_t* packet, size_t size, Move& m);
void writeTick(uint8_t* packet, uint32_t tick);

struct Guess {
    bool pending = false;  // told, the server's answer not here yet
    Move move;
    uint64_t atMs = 0;
};
enum class Answer : uint8_t { Join, Other };
Answer answer(const Guess& g, const Move& server);  // Join: the same cells both ends
bool predicts(int fromX, int fromY, int toX, int toY);  // another cell, at most 20 cells away
uint32_t waitMs(int rttMs);  // four round trips and 200 ms, at least 600 (the round trip not known: 600)

}  // namespace uagfx::netpredict
