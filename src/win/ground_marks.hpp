#pragma once
#include "core/config.hpp"
#include "core/frame.hpp"

namespace uagfx::win::groundmarks {

// 3.0.11-beta.87 (the concept «Контроллер в интерфейсе uaRO», stage 3): [interface] target_ring - the pad's target in a
// gold ring on the ground with a chevron over its head (its sprite's box the game keeps for the mouse), the monster a
// light tilt of the right stick points at in a dashed white ring, the party member the pad's heal would go on in a
// green notched one; «always» - also the monster the character fights with the mouse (the target card's). The rings
// against the depth with the world (the characters before them hide them), before the interface, as win/ground_way;
// the chevron over the world. On the game's thread.
void look(TargetRing ring);
// 3.0.11-beta.103: [interface] walk_arrow - the look of the stick's arrow (core/ground_way walkShape): its way turned
// softly (some 80 ms), faded in over 120 ms and out over 200 ms after letting go, the chevrons running on to the cell.
// 3.0.11-beta.105: [interface] walk_mark - the mark at its end where the character goes
void walkLook(WalkLook look, bool mark);
// 3.0.11-beta.142 (the owner's concept «маг, прист и хант на геймпаде», answer 9): [interface] skill_cells - a ground
// skill cast (0x0113: the pad's or the game's own, the mouse's) flashes its cells as the server makes them
// (core/skill_area, from the character's cell then) on the ground for 0.45 s, coloured by the skill: fire, ice and
// water, holy and the shields, wind and lightning, traps
// 3.0.11-beta.144 (answer 9 «дать»): while the game waits for a ground skill's place (the mouse's: its mode's state
// +0x3C4 1 a place, 5 a trap; the skill +0x3C8, its level +0x3D0) the cells it would make on the cell under the mouse
// (win/pick_hook lastPick) stand on the ground until the click
void skillCells(bool on);
// 3.0.16-beta.7 (the owner: «делай все» - ground cells): [interface] mouse_cell - the game's cell under the mouse
// (grid.tga, hidden by win/grid_cell_hook while this is on) drawn as a skill's cell: a soft light square with a thin
// edge on the ground's heights, where the game would show its own (its flag 0x76E780 0), not while the pad plays or
// a ground skill's cells stand under the mouse
void mouseCell(bool on);
// 3.0.16-beta.21 (the owner's pick 7 of «20 идей»): [interface] ground_timers - the character's own Safety Wall,
// Pneuma, Sanctuary and traps each with a ring on the ground in its skill's colour, its bright part running down
// round from the north as its time goes (core/ground_timers), pulsing its last 3 s; gone with its units
void groundTimers(bool on);
void skillCast(uint16_t skill, int lv, int x, int y);
// 3.0.11-beta.146 (R-VYV3AG «фаерволл постоянно активный»: Fire Wall clicked on the bar with the mouse, then cast with
// the pad - the game kept waiting for its place, «Select a Target» and the cells under the hidden cursor to the end):
// the game's own wait for a skill's target ended as its Esc ends it (the game mode's message 0x4e: +0x3C4, +0x3C8,
// +0x3D0 to 0) - when the pad casts or aims itself; false: none waited
bool cancelGameAim(const char* why);
void newFrame();
void draw(FrameRecord& record, int width, int height);

}  // namespace uagfx::win::groundmarks
