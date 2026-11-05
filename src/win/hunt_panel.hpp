#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::huntwin {

// 3.0.11-beta.278 (the owner's features 6 and 10): «Полювання» - a panel of the interface (its place and size in
// «Розміщення інтерфейсу», default at the right under the middle): the hunt's time, the damage a second (10 s, a
// minute) and the skills' shares, the experience an hour and the time to the level, the zeny an hour, the kills; its
// «↺» begins it anew. [interface] hunt_panel
void packet(const packets::Event& e);
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::huntwin
