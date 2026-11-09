#pragma once

namespace uagfx::win {

// 3.0.17-beta.18: uaRO HD's screen (core/settings/game_screen, the settings app's uaro-graphics\game-screen.txt): the
// game's RegQueryValueExA of its screen's values answered from the file, its RegSetValueExA of them written there -
// the shared registry key keeps the old client's screen. DllMain hooks the two import slots only; the file is read at
// the game's first query. No file (the old client, an app before 3.0.17-beta.18): the registry's, as before.
void installGameScreen();
void tellGameScreen();  // its line into the log once that is open

}  // namespace uagfx::win
