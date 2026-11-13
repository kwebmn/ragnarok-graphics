#pragma once
#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>

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
// 3.0.18-beta.241 (the reference's pad: Y the search with the keyboard): a window of the driver's own field instead -
// its words read and set by the window (UTF-8, at most `longest` letters), Start its Enter; B closes the keyboard with
// the words kept
struct Field {
    std::function<std::string()> read;
    std::function<void(const std::string&)> write;
    std::function<void()> enter;
    size_t longest = 64;
};
void openFor(const Field& f);
bool isOpen();
void close();
// Reply (3.0.18-beta.134, the pad analysis: chat): «/r » put before the line's words - the chat whispers back to the
// last whisperer
enum class Command : uint8_t {
    Left, Right, Up, Down, Press, Erase, Space, LangBack, LangOn, Shift, Send, Close, Reply
};
void command(Command c);
void frame(HWND game, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::oskwin
