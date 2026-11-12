#pragma once
#include <cstdint>

namespace uagfx::netsmooth {

// 3.0.11-beta.123 (the network plan's step 3; the owner: «персонаж и мобы телепортируются», «начинай по очереди»): the
// server puts a unit on a cell (0x0088) where the client, a round trip behind, shows it a cell or two away - before a
// chasing monster's each attack, at a walking one's cast, at a stop; the game jumps it there. A fix of 1-3 cells is
// glided to instead - the unit drawn along the way for 160-240 ms (the game's own position kept, only where it is
// drawn eased, win/net_smooth); farther fixes jump as before (a teleport, a knock-back). No Windows here.

bool eases(int cells);          // 1..3 cells apart
uint32_t glideMs(int cells);   // 120 + 40 a cell
float eased(float t);          // 0..1 slowing at its end (cubic), clamped

struct Glide {
    uint32_t id = 0;  // the unit
    float fromX = 0, fromZ = 0, toX = 0, toZ = 0;  // where it was shown, its cell's middle (the world's)
    uint64_t startMs = 0;
    uint32_t durationMs = 0;
};
// Where the unit is drawn at `nowMs`; false when the glide is over (there: its cell)
bool at(const Glide& g, uint64_t nowMs, float& x, float& z);

// 3.0.11-beta.124 (step 4): a move come late - the game walks the unit on from where it should be by now (its server
// clock, FUN_0062dbb0: more than 144 ms late) - or from a cell other than the one shown: a jump of 0.4 to 4 cells
// drawn away - an offset from the game's own place, where it was shown at first, fading out (`at` false: gone)
bool easesJump(float cells);
struct Offset {
    uint32_t id = 0;
    float offX = 0, offZ = 0;  // where it was shown less the game's place (the world's)
    uint64_t startMs = 0;
    uint32_t durationMs = 0;
};
bool offsetAt(const Offset& o, uint64_t nowMs, float& x, float& z);

// 3.0.11-beta.251 (R-UCE7J8, R-YB4S2K: «моб бьет стоя гдето далеко», then «телепортировался»): uaRO's server deals a
// hit at once (delay_battle_damage: no) and walks a monster on after its flinch; the game shows the hit after the
// swing and its flinch stops the walk the server had already resumed - the unit left standing. The server's stop
// (0x0088) is the game's «stop the walk here» (msg 0x13): a unit already standing ignores it. A unit standing this long
// off the cell its last walk or stop put it on is stranded - its walk given to the game again
constexpr uint64_t kStrandMs = 300;
bool stranded(int shownX, int shownY, int toX, int toY, uint64_t stillMs);
constexpr int kResendMost = 2;  // a walk or stop given again at most this many times
// 3.0.18-beta.179 (R-WKEDPY, the .177 log the same: a Minorous shown 2 cells off for 8 s, its walk given again 20
// times in vain): the server's walk from a cell to the same one - its chase on after a flinch, its target next to it -
// says the unit stands there; the game takes no walk of no length, so it is given again as a stop on that cell
bool standsStill(int fromX, int fromY, int toX, int toY);
bool easesFar(float cells);     // a stranded unit put back: 0.4-12 cells drawn away
uint32_t farFadeMs(float cells);

// A cell's middle in the world as uaRO.exe counts cells (FUN_0055c220's x / cell + w / 2, the other way)
void cellMiddle(int cellX, int cellY, int width, int height, int cellSize, float& x, float& z);

}  // namespace uagfx::netsmooth
