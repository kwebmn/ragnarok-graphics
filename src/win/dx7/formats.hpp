#pragma once
#include <windows.h>

#include <ddraw.h>

#include "core/pixel.hpp"

namespace uagfx::dx7 {

void fillPixelFormat(DDPIXELFORMAT& pf, PixelFormat f);
PixelFormat readPixelFormat(const DDPIXELFORMAT& pf);  // Unknown when it is not one of ours
// What Lock and GetSurfaceDesc report (surfaces-present-caps.md §2: the full size and pitch, the format).
void fillSurfaceDesc(DDSURFACEDESC2& d, int width, int height, int pitch, PixelFormat f, DWORD caps, void* bits);

}  // namespace uagfx::dx7
