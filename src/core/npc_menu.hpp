#pragma once
#include "core/window_panel.hpp"

namespace uagfx::npcmenu {

// 3.0.24-beta.27 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Разговор с NPC»: «выбор —
// отдельный список»; the old game's own menu window): the choice's list beside the talk window - right of it kGap
// apart, else left of it, else under it; always inside the screen. The parts that need no Windows (win/npc_talk).
constexpr float kGap = 8;
// 3.0.24-beta.51 (the audit 28.11, N4: the Kafra's picture over the menu at the talk's right): the screen's side the
// NPC's picture stands at (the server's cut-in type: 0 bottom left, 2 bottom right; the others none) - kept free,
// the menu on the other side when there is room there
enum class Side : uint8_t { None, Left, Right };
Side sideOf(int cutinType);
panel::Box place(const panel::Box& talk, float w, float h, int screenW, int screenH, Side keepFree = Side::None);
// A key's row: '1'..'9' or the numpad's 1..9 -> 0..8 while there are that many rows; else -1
int byKey(unsigned key, int count);

}  // namespace uagfx::npcmenu
