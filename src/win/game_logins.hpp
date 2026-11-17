#pragma once
#include <string>
#include <vector>

#include "core/game_logins.hpp"

namespace uagfx::win::gamelogins {

// 3.0.11-beta.178 (the login concept: several game windows): this game's login in the session's shared block
// (core/game_logins) - put when the server accepted it, taken away back at the login screen (and at the game's end:
// its entry counts for nothing once it is gone); the other games' logins for the shelf's «вже в грі». On the game's
// thread.
void accepted(const std::string& login);
void left();
std::vector<uagfx::gamelogins::Busy> others();  // looked at once a second at most
// 3.0.21-beta.5 (R-KQRYFF): the game window of this session that entered with this game's login within `withinMs`
// of `ms` (the session's clock, GetTickCount64) - it threw this one out; 0 none
int takenOver(unsigned long long ms, unsigned long long withinMs);

}  // namespace uagfx::win::gamelogins
