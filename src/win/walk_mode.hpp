#pragma once
#include <windows.h>

#include <string>

#include "core/config.hpp"

namespace uagfx::win::walkmodewin {

// 3.0.18-beta.208 (the keyboard concept https://claude.ai/artifact/SxvWxL5BM2tHtSWAvYPh9J, stage 2; the owner: «а что
// если мы специальный режим сделаем для ходьбы на клавиатуре?»): the walk mode in the game window (core/walk_mode) -
// ` (Ё) switches it, its W A S D / arrows / numpad walk through win/key_walk, the letters do not type while the chat
// is closed, Enter opens and sends the chat line, Esc clears it. Only with the chat's field in focus (the game's other
// fields - an NPC's number, a name - keep their keys). [interface] walk_mode keeps it over the starts.

bool key(UINT m, WPARAM wp, LPARAM lp);  // true: the mode's (the game sees neither the key nor its letter)
void lost();                             // the window lost the keys
// Each frame: the settings' state (a change from «Налаштування» switches it) and where the toggle is written
void frame(const Config& config, const std::string& iniPath);
bool on();
// 3.0.19-beta.6 (win/view3d): on while the 3D view is - W A S D walk; its file left as it is
void setOn(bool on);
bool typing();  // the chat line opened by Enter

}  // namespace uagfx::win::walkmodewin
