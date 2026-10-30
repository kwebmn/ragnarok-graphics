#pragma once
#include <string>

#include "core/window_panel.hpp"

namespace uagfx::win::headpicture {

// 3.0.11-beta.80 (the owner: «иконки можно сделать как лица персонажей?»): a character's head as the character
// block's portrait has it (3.0.11-beta.36) - the head sprite of its sex and hair style (the session's tables of their
// folders, names and endings; else the game's plain "%d_남"), its first frame (standing, facing down) in its hair
// colour's palette (data\palette\머리\머리<name>_<colour>.pal), fitted into `side`, premultiplied. `from`: what it was
// made of, or why not (for the log). False: no such sprite in the game's files.
bool make(int sex, int head, int colour, int side, panel::Picture& out, std::string* from = nullptr);

}  // namespace uagfx::win::headpicture
