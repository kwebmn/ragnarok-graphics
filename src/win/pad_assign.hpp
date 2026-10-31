#pragma once
#include <windows.h>

#include <string>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::padassignwin {

// 3.0.11-beta.91 (the concept «Контроллер в интерфейсе uaRO», stage 6): «Призначити» - X on an item in «Речі» opens the
// list of the controller's buttons that use the skill bar's cells (core/pad_assign): the D-pad chooses, A puts `value`
// (the item's number's words, a skill's "##AEGIS") at `level` into that cell (hotbarwin::assign), B cancels. On the
// game's thread.
bool isOpen();
void open(const std::string& value, int level, const std::string& what);
enum class Command : uint8_t { Up, Down, Enter, Close };
void command(Command c);
void frame(HWND game, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::padassignwin
