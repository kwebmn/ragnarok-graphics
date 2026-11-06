#pragma once
#include <cstdint>

namespace uagfx::skill_jokes_exe {

// 3.0.13-beta.8 (the owner's pick 23 of «50 идей»: «Убрать болтовню Frost Joke и Scream из чата»): the game mode's
// message for Scream (0) and Frost Joke (1) opens DC_scream.txt or BA_frostjoke.txt (english\ ones at langtype 10),
// takes a line and says it in the public chat; its file not opened, it closes the reader and says nothing. The `jne`
// past that (0x577748, 75 27) made two NOPs: the skill as the server has it, no line from the player
constexpr uint32_t kOpened = 0x577748;
constexpr uint8_t kGame[2] = {0x75, 0x27};
constexpr uint8_t kSilent[2] = {0x90, 0x90};

}  // namespace uagfx::skill_jokes_exe
