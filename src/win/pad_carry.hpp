#pragma once
#include <windows.h>

#include <string>

#include "core/frame.hpp"
#include "core/pad_carry.hpp"
#include "core/window_panel.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::padcarrywin {

// 3.0.11-beta.121 (the concept «Концепция окон и инвентаря», stage 5 - question 7 «ок»; the owner «приступай»): X
// «Взяти» on a thing of «Речі», «Навички» or «Склад» takes it up - the places that take it lit (the drag layer's gold:
// win/window_drag), a gold frame on one of them with the thing's icon, the D-pad (or the left stick) from place to
// place across the windows (core/pad_carry nextPlace), A «Покласти» as the mouse's drop does it (worn - the
// equipment's slot, a hotbar cell given it, into the storage, the trade, the bag), B or Start cancel. On the game's
// thread.
bool active();
// `x`, `y` where it was taken (the first place the nearest to it)
void begin(const uagfx::padcarry::Thing& t, float x, float y, const panel::Picture* icon, const std::string& name);
enum class Command : uint8_t { Up, Down, Left, Right, Put, Cancel };
void command(Command c);
void frame(HWND game, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::padcarrywin
