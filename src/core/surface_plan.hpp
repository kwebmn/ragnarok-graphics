#pragma once
#include <cstdint>
#include <string>

#include "core/pixel.hpp"

namespace uagfx {

// DDSCAPS / DDSD bits the client uses (ddraw.h values).
namespace ddcaps {
constexpr uint32_t BACKBUFFER = 0x4;
constexpr uint32_t COMPLEX = 0x8;
constexpr uint32_t FLIP = 0x10;
constexpr uint32_t OFFSCREENPLAIN = 0x40;
constexpr uint32_t PRIMARYSURFACE = 0x200;
constexpr uint32_t SYSTEMMEMORY = 0x800;
constexpr uint32_t TEXTURE = 0x1000;
constexpr uint32_t D3DDEVICE = 0x2000;
constexpr uint32_t VIDEOMEMORY = 0x4000;
constexpr uint32_t ZBUFFER = 0x20000;
constexpr uint32_t MIPMAP = 0x400000;
constexpr uint32_t CAPS2_TEXTUREMANAGE = 0x10;
}  // namespace ddcaps

namespace ddsd {
constexpr uint32_t CAPS = 0x1;
constexpr uint32_t HEIGHT = 0x2;
constexpr uint32_t WIDTH = 0x4;
constexpr uint32_t PITCH = 0x8;
constexpr uint32_t BACKBUFFERCOUNT = 0x20;
constexpr uint32_t LPSURFACE = 0x800;
constexpr uint32_t PIXELFORMAT = 0x1000;
constexpr uint32_t CKSRCBLT = 0x10000;
constexpr uint32_t MIPMAPCOUNT = 0x20000;
constexpr uint32_t TEXTURESTAGE = 0x100000;
}  // namespace ddsd

// What a CreateSurface call is for. Primary: what the window shows (Flip / Blt target), with the back buffer
// attached in full screen. RenderTarget: the client draws into it (lives on the GPU). Offscreen: CPU memory
// (the screen copy, the calibration readback, big pictures). ZBuffer: depth for the render target.
enum class SurfaceRole { Primary, RenderTarget, Offscreen, ZBuffer, Texture, Unsupported };

struct SurfaceRequest {
    uint32_t flags = 0;  // DDSURFACEDESC2::dwFlags
    uint32_t caps = 0;
    uint32_t caps2 = 0;
    int width = 0;
    int height = 0;
    uint32_t backBuffers = 0;
    uint32_t mipLevels = 0;
    PixelFormat format = PixelFormat::Unknown;  // from ddpfPixelFormat when DDSD_PIXELFORMAT is set
    uint32_t zBits = 0;
    uint32_t keyLow = 0;  // ddckCKSrcBlt when DDSD_CKSRCBLT is set
    uint32_t keyHigh = 0;
};

struct SurfacePlan {
    SurfaceRole role = SurfaceRole::Unsupported;
    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::Unknown;
    int mipLevels = 1;
    bool backBuffer = false;  // Primary: create the attached back buffer (render target)
    bool colorKey = false;
    uint32_t keyLow = 0;
    uint32_t keyHigh = 0;
    std::string error;  // why the request is Unsupported
};

SurfacePlan planSurface(const SurfaceRequest& r, int screenWidth, int screenHeight, PixelFormat screenFormat);

}  // namespace uagfx
