#pragma once
#include <string>

namespace uagfx::ai {

// 2.5.28: whether a monster's sprite (its file's name without ".spr", any case) is a boss's or an MVP's - by the
// server's monster database (boss_sprites.inc, tools/boss_sprites.py), as the client's files cannot tell.
bool bossSprite(const std::string& name);

}  // namespace uagfx::ai
