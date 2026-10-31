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
void newFrame();
void draw(FrameRecord& record, int width, int height);

}  // namespace uagfx::win::groundmarks
