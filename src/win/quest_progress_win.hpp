#pragma once
#include <vector>

#include "core/quest_progress.hpp"

namespace uagfx::win::questprogresswin {

// 3.0.19-beta.4 (the owner: «давай» - a quest's step in «Довідник»; the testers: «щоб відстежувалось від серва де
// ти, на чому застряг»): the current character's quests' steps as the server keeps them, from the site
// (core/quest_progress, GET /api/client/quests/progress with the device's token) - asked while «Довідник» is open: at
// once for another character, then once a minute. Null: none known (not signed in to the site on this computer, not
// asked yet, the site's «not yours»). On the game's thread.
const std::vector<questprogress::State>* states();
void frame(bool open, unsigned codePage);  // each frame «Довідник» is drawn (open) or not
int generation();                          // grows with each answer read (a quest's card made again)

}  // namespace uagfx::win::questprogresswin
