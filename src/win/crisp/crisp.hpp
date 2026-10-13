#pragma once
#include <string>

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
// For the 30-second stats line: "" while crisp text is off.
std::string stats();

}  // namespace uagfx::win::crisp
