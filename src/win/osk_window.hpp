#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::oskwin {

// 3.0.11-beta.214 (the owner: «3. можно»): the controller's on-screen keyboard (core/osk) - the chat wheel's A opens
// it; it types into the game's own line that has the keys (a whisper card's, the chat room's) or else the chat's
// (given the keys): the line's words read, a letter put after them in the game's code page (the edit's SetText,
// vtable + 0x98) as long as the line takes it; Start sends it as Enter does (its window's SendMsg 6 / 0x76); B closes
// it with the line kept. On the game's thread.
void open();
bool isOpen();
void close();
enum class Command : uint8_t { Left, Right, Up, Down, Press, Erase, Space, LangBack, LangOn, Shift, Send, Close };
void command(Command c);
void frame(HWND game, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::oskwin
