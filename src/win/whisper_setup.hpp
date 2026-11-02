#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::whispersetupwin {

// 3.0.11-beta.168 (task 5 of the remaining windows: the whisper's settings): [interface] whisper_setup - the game's
// whisper settings (the friends' «Налаштування»; UI window 0x44 at g_windowMgr + 0x18C, class 0x69BC4C, SendMsg
// FUN_004c4010) as a card of switches (core/switch_card): a whisper's own window for strangers (0x771950) and for
// friends (0x771958), the new whisper's sound (0x771954) - each its checkbox's own (6 / 0x93 with its new state); ✕
// its close (6 / 0x87); the game's window hidden (its shown flag 0). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
enum class PadCommand : uint8_t { Up, Down, Press, Close };
void padCommand(PadCommand c);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::whispersetupwin
