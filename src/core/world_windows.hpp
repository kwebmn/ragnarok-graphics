#pragma once
#include <cstdint>

namespace uagfx::worldwindows {

// 3.0.11-beta.191 (R-FG9GCX), 3.0.11-beta.195 (the tester: a skill's shout over a monk's head over «Навички»):
// uaRO.exe's windows of the world, not of the interface - each put at its list's front for a character on the map
// (FUN_004cca30: drawn first): its HP/SP gauge (60 x 9, class 0x696D4C), its other gauge (60 x 5, 0x696CB4), its
// cast's (60 x 6, 0x696C18; the actor's message 0x52), the words over its head (the chat and a skill's shout, the
// actor's message 7; the hovered one's name, the game mode's + 0x2E4: 0x696068), its chat room's sign ("%s (%d/%d)",
// message 0x20: 0x6966A4) and its shop's (message 0x37: 0x69673C). Drawn under the driver's windows, as the world is.
bool worldWindow(uint32_t vtable);

}  // namespace uagfx::worldwindows
