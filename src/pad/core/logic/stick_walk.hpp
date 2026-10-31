#pragma once
#include <cstdint>
#include <optional>
#include <utility>

#include "pad/core/bytes.hpp"
#include "pad/core/world/world.hpp"

namespace uapad::stickwalk {

// 3.0.11-beta.102 (the owner: «проработал передвижение с контролером, чтобы стрелочка показывалась как в концепции и
// оно не глючило при хождении. ранее у нас курсор мышки просто тыкал на клетку, если маштаб маленький то были рывки
// при хождение а если большой маштаб то шел дальше чем нужно»): walking by the stick in the map's cells, not by a
// point of the screen (the cursor held out so many pixels: few cells at a near camera - the character got there and
// stood between the asks, many at a far one - it walked on past letting go). The cell asked for is `lead` cells
// along the stick's way, asked again before the character gets there; letting go asks for the cell it steps into.

struct Way {  // along the ground, a unit: east (x) and north (y) of the map's cells
    double east = 0, north = 0;
};
// The stick (x right, y up, past its dead zone) on the ground: the camera's look along the ground (east, north) is
// the screen's up, a quarter turn clockwise its right; no look known - north up
Way wayOf(double sx, double sy, std::optional<std::pair<double, double>> look);
double turnBetween(Way a, Way b);  // radians, 0 .. pi

// How far ahead the cell asked for is: about 0.6 s of walking at the character's speed (ms a cell; 0 not known: the
// game's 150) and a cell more - 3 to 6 cells
int lead(uint16_t speedMs);

// The cell to ask for: the last walkable one along the way within `lead` cells (none known: `lead` cells on); blocked
// at once - slid along the wall: the way turned 45° either side or along either axis, the one walking furthest (the
// nearer the stick's way on a tie); nowhere - `here`. 3.0.11-beta.103: `cut` (when given) - whether the march that
// gave it stopped at a cell that does not walk short of the lead (the arrow's red bar across its end)
Cell destination(Cell here, Way way, int lead, const GroundGrid* ground, bool* cut = nullptr);

// Letting go: the cell the character steps into along its way (the server stops it there, no step back), where it is
// when that one does not walk
Cell stopCell(Cell here, Way way, const GroundGrid* ground);

struct Asked {
    Cell to;
    Way way;
    uint32_t atMs = 0;
};
constexpr uint32_t kMinGapMs = 100, kRefreshMs = 1000;
constexpr double kTurnAgain = 22.5 * 3.14159265358979323846 / 180.0;
// Whether to ask for `dest` now: the first ask; the way turned past 22.5°, or the cell along it moved aside as much (a
// post, a wall slid along); the character within half the lead of the cell asked for (on before it arrives and
// stands); a second on anyway (a lost ask) - never the same cell again, never sooner than kMinGapMs after the last
bool askAgain(const std::optional<Asked>& last, Cell here, Cell dest, Way way, int lead, uint32_t nowMs);

}  // namespace uapad::stickwalk
