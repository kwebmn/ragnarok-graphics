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
// 3.0.11-beta.74: the panels' pictures' source (win/pack_pictures' iconsStamp: the AI pack read, the switch) - another
// one changes generation(), the icons made again
void setPictures(int source);

// 3.0.11-beta.54 (R-JK5455: «взял маленький екран а там не все елементы интерфейса влезли»): the new interface's
// size - its parts are the concept's at the game's 1920 x 1080 (a concept pixel a game pixel); a game smaller than
// 1280 x 720 has them smaller in its share (896 x 672: 70 %), never below 60 %, nor so small that the words' 14 come
// out under 11 of the screen's pixels (`windowScale`: the screen's pixels a game pixel); [interface] size names one
constexpr int kFullW = 1280, kFullH = 720;
float autoSize(int gameW, int gameH, double windowScale);
void setSize(float size);  // a part's size against the concept's (0.5-1.5); changes draw the parts again
float size();
float partScale(double k);  // a part's scale in its picture's pixels: k times size(), 0.5-6
// 3.0.11-beta.67 (the concept: «Масштаб текста — отдельная настройка 80–150 %»; [interface] text_size): the windows one
// reads in - the chat, «Довідник», «Речі», «Характеристики» - larger or smaller as a whole, their words with them
void setText(float text);  // 0.8-1.5; changes draw the parts again
float text();
float readingScale(double k);  // their scale: partScale's times text(), 0.5-6

// 3.0.11-beta.49 (the concept: «тень только у окон»): a window's shadow - dark at a rounded box's edge (`alpha`),
// fading out `spread` away, none under the box itself (its glass stays as it is); the picture (w + 2 spread) x
// (h + 2 spread), premultiplied, the box at (spread, spread)
panel::Picture shadow(int w, int h, float radius, float spread, float alpha);
// 3.0.11-beta.108 (the concept «Концепция окон и инвентаря», question 6): the top window's edge - a rounded ring
// `width` thick at a w x h box's edge in `rgb` at `alpha` (its outer and inner sides smoothed), nothing inside;
// premultiplied, w x h (only the band at the edge painted: a large window's picture made fast)
panel::Picture edge(int w, int h, float radius, float width, uint32_t rgb, float alpha);
// 3.0.11-beta.50 (the concept's low HP: «кромка экрана — по выбору, выключена»): the screen's edge in `rgb` - `alpha`
// at the very edge, fading in to nothing a `depth` share of the shorter side away; w x h to be stretched over the
// screen (smooth), premultiplied
panel::Picture vignette(int w, int h, uint32_t rgb, float alpha, float depth);

}  // namespace uagfx::uistyle
