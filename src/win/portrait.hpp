#pragma once
#include <string>

#include "core/window_panel.hpp"

namespace uagfx::win::portraitwin {

// 3.0.11-beta.273 (the owner's feature 1, from nostalRE's snapshots): the character's picture as it stands in the game
// - every 10 minutes at most (the first a minute after it enters the world), when it has stood still 3 seconds with
// no window of the new interface open, the part of the next screenshot round it (its sprite's box, or the screen's
// middle) kept as uaro-graphics\portraits\<its name's number>.bgra
// The screenshot target for this present ("" none): called with the character's name (UTF-8), the game's size
std::string wanted(bool on, const std::string& nameUtf8, int gameWidth, int gameHeight);
// Its picture, its upper part as a side x side square (premultiplied, opaque); nullptr: none kept
const panel::Picture* face(const std::string& nameUtf8, int side);

}  // namespace uagfx::win::portraitwin
