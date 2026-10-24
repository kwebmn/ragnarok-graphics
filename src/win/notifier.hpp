#pragma once
#include <windows.h>

#include "core/config.hpp"
#include "core/game_packets.hpp"

namespace uagfx::win::notifier {

// 3.0.4-beta.3 (the user's idea 2): what wants the player while the game window is behind others or minimised
// (core/notify.hpp) - its taskbar button flashes until it is back in front, and Windows shows a notification: a tray
// icon's balloon (shell32 loaded by name) that a click brings the game back with. The icon goes as the window comes
// back to the front.
constexpr UINT kMessage = WM_APP + 0x3A7;  // the tray icon's word to the game's window (win/window_switch.cpp)

// Each frame (and behind, by the clock): the game's window, whether it is in front, the settings
void frame(HWND window, bool front, const Config& config);
// A packet the game took (win/packet_tap.cpp)
void packet(const packets::Event& e);
// The tray icon's word: a click on the notification - the game to the front. True: it was its.
bool message(HWND window, UINT m, WPARAM wp, LPARAM lp);
// The game's window going: its icon taken away
void gone(HWND window);

}  // namespace uagfx::win::notifier
