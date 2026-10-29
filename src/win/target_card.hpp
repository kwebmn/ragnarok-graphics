#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::targetpanel {

// 3.0.11-beta.38 (stage 7 of the new interface; the owner: «делай карточку цели»): [interface] target - the monster
// the character fights in a card at the top middle (core/target_card). The game keeps no target of its own the
// driver can read, so the hits tell it: the one the character hits (0x008a, 0x01de, 0x0114 from its own id), else
// the one hitting it; gone when it vanishes (0x0080) or after 12 seconds without a hit. Which monster: the world's
// actor of that id (world + 0x18, actor + 0x208 its id, + 0x20C its class - FUN_004ebd10, FUN_0054aee0) by the
// reference pack. Under the game's own windows when one stands at the top middle (the hotkeys' bar). On the game's
// thread.
void packet(const packets::Event& e);
void frame(HWND game, bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::targetpanel
