#pragma once
#include <windows.h>

#include "core/config.hpp"

namespace uagfx::win::keyhintwin {

// 3.0.21-beta.13 (the owner: «надо еще подсказку сделать что мышка работает при нажатии на альт ... надо пройтись и в
// таких моментах наделать подсказки»): the key hints' line (core/key_hints) at the bottom middle over the skill bar -
// the 3D view (Alt for the windows, the wheel, Shift, Tab, Ctrl+Shift+V), a window open while the mouse turns the view
// (hold Alt), the free camera's keys, a skill waiting its target in the 3D view, the storage's and the cart's picks;
// over the controller's line when that shows. What has shown and what the player has used kept in
// uaro-graphics\cache\key-hints.txt; [interface] control_hints turned off and on again - all of them anew. On the
// game's thread.
void frame(HWND game, const Config& config, double k, int targetWidth, int targetHeight);

}  // namespace uagfx::win::keyhintwin
