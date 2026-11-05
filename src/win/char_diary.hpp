#pragma once
#include <string>

#include "core/char_diary.hpp"

namespace uagfx::win::chardiarywin {

// 3.0.11-beta.273 (the owner's features 3 and 4): the characters' diary kept as they play - each present while the
// game shows its world: the character's name, map, levels, job and the time played, written to
// uaro-graphics\characters.txt once a minute and at a map's or a level's change
void frame(unsigned codePage);
const chardiary::Entry* find(const std::string& nameUtf8);  // nullptr: never seen here
long long nowUnix();

}  // namespace uagfx::win::chardiarywin
