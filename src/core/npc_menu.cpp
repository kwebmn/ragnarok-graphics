#include "core/npc_menu.hpp"

#include <algorithm>

namespace uagfx::npcmenu {

panel::Box place(const panel::Box& talk, float w, float h, int screenW, int screenH) {
    const float sw = float(screenW), sh = float(screenH);
    const float y = std::clamp(talk.y, 0.0f, std::max(0.0f, sh - h));
    if (talk.x + talk.w + kGap + w <= sw) return panel::Box{talk.x + talk.w + kGap, y, w, h};
    if (talk.x - kGap - w >= 0) return panel::Box{talk.x - kGap - w, y, w, h};
    const float x = std::clamp(talk.x, 0.0f, std::max(0.0f, sw - w));
    return panel::Box{x, std::clamp(talk.y + talk.h + kGap, 0.0f, std::max(0.0f, sh - h)), w, h};
}

int byKey(unsigned key, int count) {
    int row = -1;
    if (key >= '1' && key <= '9') row = int(key - '1');
    if (key >= 0x61 && key <= 0x69) row = int(key - 0x61);  // VK_NUMPAD1 .. VK_NUMPAD9
    return row >= 0 && row < count ? row : -1;
}

}  // namespace uagfx::npcmenu
