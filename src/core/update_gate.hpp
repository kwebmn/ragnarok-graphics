#pragma once
#include <cstddef>
#include <string>

namespace uagfx::update_gate {

// 3.0.24-beta.45 (the owner: «нажал руками обновить, оно не закрыло лаунчер и начало устанавливать, а установка при
// открытом лаунчере не работает»): what the update does before its files go in - wait for the game (the player
// closes it), ask the settings app to close (once a window: its own question, if it has one, is the player's), wait
// for it; then the install
enum class Next { Install, WaitGame, CloseApp, WaitApp };
Next next(bool gameRuns, bool appOpen, bool appAsked);

// The update's own log (uaro-graphics\logs\uaro-graphics-update.log): a step's line - its time, then the words on
// one line; the log past `most` bytes keeps its newest half from a line's start
std::string logLine(const std::string& stamp, const std::string& words);
std::string capped(const std::string& log, size_t most);

}  // namespace uagfx::update_gate
