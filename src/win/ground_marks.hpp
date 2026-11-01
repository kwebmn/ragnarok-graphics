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
void skillCells(bool on);
void skillCast(uint16_t skill, int lv, int x, int y);
void newFrame();
void draw(FrameRecord& record, int width, int height);

}  // namespace uagfx::win::groundmarks
