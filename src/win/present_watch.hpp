#pragma once
#include <windows.h>

#include <string>

#include "core/config.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::presentwatchwin {

// 3.0.19-beta.8 (R-T8FGMM; core/present_watch): the renderer's presents watched while the game's world is in front -
// ten seconds of slow ones (a frame crossing from the card drawing to the one the screen hangs off) turn the drawing
// over from the next start: [renderer] api to Direct3D 11, or on Direct3D 11 [renderer] gpu to the screen's card; the
// player told in the chat. The cards are logged once: the one drawing and the one the game's monitor hangs off.
void frame(gpu::Renderer& renderer, const Config& config, const std::string& iniPath, HWND game, unsigned codePage);

// The card the game's monitor hangs off (its DXGI output); "" when none says so
std::string screenCard(HWND game);

}  // namespace uagfx::win::presentwatchwin
