#pragma once
#include <windows.h>

namespace uagfx::win::recapwin {

// 3.0.18-beta.120 (the concept «Ігровий інтерфейс»: «Що мене вбило» from the character block and Ctrl+K): the last
// death's hits (win/death_recap_watch, as the death screen showed them) in a small card at the top middle - its ✕ or
// Esc closes it; this run's only (none yet: a line saying so). Drawn by uaUI, passive: its ✕ by its box. On the game's
// thread.
void frame(HWND game, bool on, double k, int targetWidth, int targetHeight);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(UINT m, WPARAM wp, LPARAM lp);  // Esc while it is open
bool isOpen();
void toggle(const char* why);

}  // namespace uagfx::win::recapwin
