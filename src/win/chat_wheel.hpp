#pragma once
#include <windows.h>

#include <array>
#include <cstdint>
#include <string>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::chatwheelwin {

// 3.0.11-beta.214 (the owner: «посмотри, как сделать удобно начинать общение с геймпада»): the chat wheel
// (core/chat_wheel) at the screen's middle while the controller's Talk button is held (src/pad/win/runtime opens it
// and gives it the pad). A phrase goes as a line typed into the game's own chat line and sent by its Enter (the chat
// window's SendMsg 6 / 0x76) on the channel chosen (the send mode 0x772D60 for that line, the whisper's name kept
// aside); an emotion as its command the same way (the game's parser); the voice - the chat's line given the keys and
// Windows' voice typing (Win+H). The player's words in the chat line before are put back after. On the game's thread.
struct Setup {
    std::array<std::string, 8> phrases;  // UTF-8, "" the interface's
    std::string emotes;                  // the commands by spaces, "" the game's own
};
void open(const Setup& setup);
bool isOpen();
void close();
// The controller while it is open: the right stick (x right, y up) points; LB / RB the page; LT / RT the channel; Y
// the voice; B closes; `held` false - the Talk button let go: the pointed one sent and the wheel closed
struct Pad {
    float rx = 0, ry = 0;
    bool held = true, pageBack = false, pageOn = false, channelBack = false, channelOn = false, voice = false,
         cancel = false, keyboard = false;
};
void pad(const Pad& p);
void frame(HWND game, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::chatwheelwin
