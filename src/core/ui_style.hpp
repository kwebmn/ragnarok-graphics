#pragma once

#include "core/window_panel.hpp"

namespace uagfx::uistyle {

// 3.0.11-beta.47 (the concept's «Непрозрачность»; the owner: «занимаемся доработками интерфейса»): how dense the new
// interface's grounds are - the HUD's panels (the character block, the minimap's bar and list, the buffs, the party,
// the target's card, the hotbar) [interface] panel_opacity, 50-100 % (80); the windows («Речі», «Характеристики»,
// «Довідник») [interface] window_opacity, 80-100 % (90); the chat as the panels but never under 80 % (its channels'
// colours fall to 3.5-4.4:1 at 70 %). The words, the icons and the tips stay whole.
constexpr int kPanelLeast = 50, kPanelDefault = 80, kWindowLeast = 80, kWindowDefault = 90;

void set(int panelPercent, int windowPercent);  // each clamped to its own range
float panel();   // 0.5..1
float window();  // 0.8..1
float chat();    // the panels', at least 0.8
int generation();  // another number each time they change: the parts draw their pictures again

// 3.0.11-beta.49 (the concept: «тень только у окон»): a window's shadow - dark at a rounded box's edge (`alpha`),
// fading out `spread` away, none under the box itself (its glass stays as it is); the picture (w + 2 spread) x
// (h + 2 spread), premultiplied, the box at (spread, spread)
panel::Picture shadow(int w, int h, float radius, float spread, float alpha);
// 3.0.11-beta.50 (the concept's low HP: «кромка экрана — по выбору, выключена»): the screen's edge in `rgb` - `alpha`
// at the very edge, fading in to nothing a `depth` share of the shorter side away; w x h to be stretched over the
// screen (smooth), premultiplied
panel::Picture vignette(int w, int h, uint32_t rgb, float alpha, float depth);

}  // namespace uagfx::uistyle
