#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::netpredict {

// 3.0.11-beta.130 (the network plan's step 5, the own walk at once; the owner «делаем» - a switch of its own, off at
// first): the client walks the character only after the server's answer (ZC_NOTIFY_PLAYERMOVE 0x0087, a round trip
// after the click). Guessed instead: at the click the game's player told the walk itself (its message 0, as the
// client's FUN_0057e1f0 tells it: the server's clock now, from its cell to the one asked); the server's answer, the
// same cells both ends, joined - its tick made the guess's, so the game walks on from where it is; another way: the
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

// 3.0.11-beta.132 (R-6D43Y6: a walk guessed while five Seals hit the character - no answer, put back): the server
// holds a unit still after it was hit (its damage motion; not when endured, missed or dodged) and after it attacked
// (its attack motion) - a walk asked then waits there; no guess until 50 ms after that hold (the server's clock)
struct Act {  // ZC_NOTIFY_ACT (0x008a, 29 bytes)
    uint32_t src = 0, dst = 0, tick = 0;  // who +2, whom +6, the server's tick +10
    int srcSpeed = 0, dstSpeed = 0;      // the attack's motion +14, the damage's +18 (ms)
    // the damage +22, the kind +26 (0 a hit, 4 endured, 8 hits, 9 endured, 10 a critical one)
    int damage = 0, type = 0;
};
bool readAct(const uint8_t* packet, size_t size, Act& a);
uint32_t holdUntil(uint32_t until, const Act& a, uint32_t own);  // the server's clock the own walk is held to
bool heldNow(uint32_t serverNow, uint32_t until);

// 3.0.11-beta.134 (R-U436CU: guessed from 286, 124 while the server walked it from 290, 124 - the client still showed
// it standing): when the server's walk of the character ends - its tick, 10 a straight cell and 14 a diagonal one at
// the character's speed (ms a cell, 0x00b0's; 150 when not told); no guess before then
uint32_t walkEnds(const Move& m, int speedMs);

// 3.0.11-beta.135 (R-DWCBHA): the server still walks it - a walk known (0: none) whose end is less than 300 ms ago
bool walking(uint32_t serverNow, uint32_t walkEnd);
// An earlier walk request still waiting for its answer (asked after the last answer, its wait not over) - no guess
// then: the server's answers to older clicks would come while the guess waits (a minute of lost packets)
bool inFlight(uint64_t askedMs, uint64_t answeredMs, uint64_t nowMs, uint32_t waitMs);

// 3.0.11-beta.136 (R-JMHDUY: 6 of 12 guesses walked «another way» - the game showed the character short of where the
// server's last walk had put it, hit by Seals on the way): where the server last put the character - its last walk's
// end, or its position fix (0x0088) when that came after the walk; guessed only from that cell (none known: guessed)
struct Placed {
    Move move;
    bool moveKnown = false;
    int fixX = 0, fixY = 0;
    bool fixKnown = false, fixAfterMove = false;
};
bool caughtUp(int shownX, int shownY, const Placed& at);

// 3.0.11-beta.137 (R-QTN6TF: both walks put back were guessed in a calm moment of a lossy line): a calm line - no
// packet lost for 30 s (0: none seen) and a round trip under 150 ms (0: not known yet)
bool calm(uint64_t nowMs, uint64_t lastLossMs, int rttMs);

}  // namespace uagfx::netpredict
