#pragma once
#include <windows.h>

namespace uagfx::win::gamepress {

// 3.0.11-beta.115 (the concept «Концепция окон и инвентаря», question 5 «да» - the game's own windows over the
// driver's: their clicks too): a press (or the wheel) with a window of the game's own under the game's cursor - its
// count window over «Склад», its trade, its options - is the game's, not the driver's part drawn under it. The game's
// own hit test (FUN_004cb550: shown, inside, taking the mouse) over its windows' list (g_windowMgr + 0x38), the top one
// first, as its drop looks for a window (FUN_004ccef0). On the game's thread.
bool pressOver(UINT m);

}  // namespace uagfx::win::gamepress
