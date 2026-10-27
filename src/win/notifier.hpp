#pragma once
#include <windows.h>

#include <string>

#include "core/config.hpp"
#include "core/game_packets.hpp"

namespace uagfx::win::notifier {

// 3.0.4-beta.3 (the user's idea 2): what wants the player while the game window is behind others or minimised
// (core/notify.hpp) - its taskbar button flashes until it is back in front, and Windows shows a notification: a tray
// icon's balloon (shell32 loaded by name) that a click brings the game back with. The icon goes as the window comes
// back to the front.
constexpr UINT kMessage = WM_APP + 0x3A7;  // the tray icon's word to the game's window (win/window_switch.cpp)

// Each frame (and behind, by the clock): the game's window, whether it is in front, whether it is the main game window
// (3.0.7-beta.6: what the party and the guild hear told by the main one alone), the settings
void frame(HWND window, bool front, bool main, const Config& config);
// A packet the game took (win/packet_tap.cpp)
void packet(const packets::Event& e);
// 3.0.7-beta.6 (the user's notification 9): the server closed the connection (the game's poll of it failed,
// win/packet_tap.cpp)
void disconnected();
// 3.0.9-beta.4: the developer answered the report `code` (its status's name, UTF-8) - told as the rest, from behind
// (win/answer_card: in front its card in the game tells of it)
void answer(const std::string& code, const std::string& status);
// The tray icon's word: a click on the notification - the game to the front. True: it was its.
bool message(HWND window, UINT m, WPARAM wp, LPARAM lp);
// The game's window going: its icon taken away
void gone(HWND window);

}  // namespace uagfx::win::notifier
