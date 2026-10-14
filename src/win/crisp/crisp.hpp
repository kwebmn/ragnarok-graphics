#pragma once
#include <string>

#include "core/frame.hpp"

namespace uagfx::win::crisp {

// Stage 6, crisp text (docs/superpowers/specs/2026-10-13-stage6-crisp-text-design.md): the driver keeps a twin of
// every game window picture at the window's real pixels and draws the windows' tiles from it. Called under the
// driver's lock; the hooks take it themselves.

// At the uaRO renderer's start: checks the exe and puts the hooks in place; logs "crisp text: on" or why it is off.
void install(bool enabled);
// When the renderer closes: the game's methods and imports back, every twin and its textures gone.
void uninstall();
// At every present, between the game's frames: the scale twins are made at - k with every improvement on (Pause
// does not change it). A new scale drops the twins and has the game redraw every window.
void setScale(double K);
// Device7::record: a draw whose stage 0 texture is a tile of a window picture with a twin carries that twin.
void tileDraw(const void* surface, DrawCmd& cmd);
// Surface7's destructor: that surface is no tile any more.
void forgetSurface(const void* surface);
// For the 30-second stats line: "" while crisp text is off.
std::string stats();
// AI icons (0.14): on - an icon the game copies whole into a window (a square of 16 to 64 pixels: items, skills) goes
// to the renderer to be made larger, and the larger one goes into the twin from then on.
void setAiIcons(bool on, bool pieces);  // pieces (0.15): the windows' buttons, tabs, slots too
// Renderer::IconSink: an icon came back; the windows are redrawn with it at a present soon.
void iconReady(void* context, uint64_t key, int width, int height, const uint32_t* argb);

}  // namespace uagfx::win::crisp
