#pragma once
#include <windows.h>

#include <cstdint>

#include "core/config.hpp"
#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::mobhpwin {

// 3.0.11-beta.95 (the owner: «Полоски HP мобов - всем показывай»; the concept «Контроллер в интерфейсе uaRO»,
// «Полоски HP монстров»): HP bars over the monsters' heads (core/mob_hp) - the per cent from their name lines (0x0195
// through win/packet_tap), each bar over its sprite's box from the game's pick list, under the driver's panels and
// the game's cursor; the target's wider with its per cent beside. [interface] mob_hp. On the game's thread.
void packet(const packets::Event& e);
void frame(HWND game, MobHp mode, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
int hp(uint32_t id);  // the per cent the bars show for it; -1 none (not known, or the bars off)
// The target's bar's top in the game's pixels over its box, for the target's chevron to stand over it; false: none
bool targetBarTop(uint32_t id, const float box[4], int screenHeight, float& top);

}  // namespace uagfx::win::mobhpwin
