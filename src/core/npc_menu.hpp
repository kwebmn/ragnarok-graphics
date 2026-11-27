#pragma once
#include "core/window_panel.hpp"

namespace uagfx::npcmenu {

// 3.0.24-beta.27 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Разговор с NPC»: «выбор —
// отдельный список»; the old game's own menu window): the choice's list beside the talk window - right of it kGap
// apart, else left of it, else under it; always inside the screen. The parts that need no Windows (win/npc_talk).
constexpr float kGap = 8;
panel::Box place(const panel::Box& talk, float w, float h, int screenW, int screenH);
// A key's row: '1'..'9' or the numpad's 1..9 -> 0..8 while there are that many rows; else -1
int byKey(unsigned key, int count);

}  // namespace uagfx::npcmenu
