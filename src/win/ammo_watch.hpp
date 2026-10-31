#pragma once
#include <windows.h>

namespace uagfx::win::ammowatch {

// 3.0.11-beta.125 (the owner: «когда играю арчером, есть стрелы, и не видно сколько осталось»): the ammo worn, for the
// hotbar's pill and the character block (moved from win/char_block, 3.0.11-beta.114-.120) - its count in the bag by
// its place (the session's +0xEEC, set by the server's 0x013c - at the login too), read every half a second; gone from
// the bag - ran out, 0 for half a minute; taken off or none: count < 0. An empty or unreadable bag (a map loading)
// decides nothing; out of the world forgotten. On the game's thread.
struct Ammo {
    int count = -1;      // < 0 none worn
    int id = 0;          // its item's number
    bool arrows = true;  // arrows (else bullets, thrown things - «Боєприпаси»)
};
const Ammo& read(ULONGLONG now);

}  // namespace uagfx::win::ammowatch
