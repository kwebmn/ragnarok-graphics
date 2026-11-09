#pragma once

namespace uagfx::win {

// 3.0.17-beta.13: a client of its own (uaro-client.json: uaRO HD) keeps the game's settings in its own key
// (core/settings/game_key.hpp): its name written over the game's string before the game reads it. The first time,
// everything the game kept in the shared key is copied into it, and the shared key's personal copy gives its screen
// up when a setup.exe's is under it - the old client on dgVoodoo starts again; 3.0.17-beta.14: that check at every
// start, unless the old client has uaRO Graphics of its own. The old uaRO: nothing changes.
void installOwnGameKey();
// 3.0.17-beta.15: its lines into the log once it is open (DllMain runs before it); the key the game reads is in
// uaro-graphics\game-key.txt for the settings app - the shared one when the game could not read its own
void tellOwnGameKey();

}  // namespace uagfx::win
