#pragma once
#include <windows.h>

#include "core/config.hpp"

namespace uagfx::win {

// 2.6.17, [connection]: the legacy client's way onto a map made sure (core/map_entry.hpp). Once a frame on the game's
// thread: patient_entry - the client waits for the map server a minute instead of 15 s, a connect that failed made
// again; entry_pause - after an entry that failed, the next character's selection waits until the map server has
// forgotten it (its auth node), the char server kept awake meanwhile and a countdown over the game. Nothing done
// when uaRO.exe is another build.
void mapEntryFrame(const ConnectionSettings& settings, HWND game);

}  // namespace uagfx::win
