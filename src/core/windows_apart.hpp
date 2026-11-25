#pragma once
#include <vector>

namespace uagfx::windowsapart {

// 3.0.24-beta.1 (the owner: «посмотри чтобы старые и новые окна между собой не конфликтовали, особенно наложение одно
// на другое»): the game's own windows (its things' windows again) are drawn over the driver's panels and take their
// clicks - one that opens over a panel (the character block, the chat, the hotbar, the minimap) is moved, once as it
// opens, to the nearest place on the screen that covers none of them; nowhere free: where it was
struct Box {
    int x = 0, y = 0, w = 0, h = 0;
};
bool overlaps(const Box& a, const Box& b);
// Its new top-left (`moved` false: as it is - free already, or no free place)
Box apart(const Box& window, const std::vector<Box>& panels, int screenW, int screenH, bool& moved);

}  // namespace uagfx::windowsapart
