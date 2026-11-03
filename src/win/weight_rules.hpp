#pragma once

namespace uagfx::win::weightrules {

// 3.0.11-beta.202 (the tester: «бреше в чат на 50%»): uaRO's 70 % for the game's own line about HP and SP not coming
// back (core/weight_rules) - its threshold in the game's weight update (install, once; another build of the exe: left
// as it is) and its number in the loaded msgstringtable (frame, till done). On the game's thread.
bool install();
void frame();

}  // namespace uagfx::win::weightrules
