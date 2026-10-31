#pragma once
#include <windows.h>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::padtileswin {

// 3.0.11-beta.90 (the concept «Контроллер в интерфейсе uaRO», stage 6): [interface] pad_tiles - Start on the controller
// opens the tile of the game's windows (core/pad_tiles) in the middle of the screen: the D-pad or the left stick moves
// its gold frame, A opens the window framed (the game's own through its Basic Info window's buttons, the new big map,
// «Довідник», the controller's own menu), B or Start closes it; the mouse clicks its tiles too. On the game's thread.
bool isOpen();
void open();
enum class Command : uint8_t { Up, Down, Left, Right, Enter, Close };
void command(Command c);
void frame(HWND game, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::padtileswin
