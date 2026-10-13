#pragma once
#include <cstdint>
#include <vector>

#include "core/pixel.hpp"

namespace uagfx {

// D3DDEVICEDESC7 bits (d3dcaps.h / ddraw.h values); src/win/dx7/caps.cpp checks them against the real headers.
namespace d3dcaps {
constexpr uint32_t DEV_FLOATTLVERTEX = 0x1;
constexpr uint32_t DEV_TEXTUREVIDEOMEMORY = 0x200;
constexpr uint32_t DEV_DRAWPRIMTLVERTEX = 0x400;
constexpr uint32_t DEV_CANRENDERAFTERFLIP = 0x800;
constexpr uint32_t DEV_DRAWPRIMITIVES2 = 0x2000;
constexpr uint32_t DEV_DRAWPRIMITIVES2EX = 0x8000;
constexpr uint32_t DEV_HWRASTERIZATION = 0x80000;
constexpr uint32_t MISC_CULLNONE = 0x10;
constexpr uint32_t MISC_CULLCW = 0x20;
constexpr uint32_t MISC_CULLCCW = 0x40;
constexpr uint32_t RASTER_DITHER = 0x1;
constexpr uint32_t RASTER_ZTEST = 0x10;
constexpr uint32_t RASTER_SUBPIXEL = 0x20;
constexpr uint32_t RASTER_FOGVERTEX = 0x80;
constexpr uint32_t RASTER_ZBUFFERLESSHSR = 0x8000;
constexpr uint32_t CMP_ALL = 0xFF;      // NEVER ... ALWAYS
constexpr uint32_t BLEND_ALL = 0x1FFF;  // ZERO ... BOTHINVSRCALPHA
constexpr uint32_t SHADE_COLORGOURAUDRGB = 0x8;
constexpr uint32_t SHADE_SPECULARGOURAUDRGB = 0x200;
constexpr uint32_t SHADE_ALPHAGOURAUDBLEND = 0x4000;
constexpr uint32_t SHADE_FOGGOURAUD = 0x80000;
constexpr uint32_t TEXTURE_PERSPECTIVE = 0x1;
constexpr uint32_t TEXTURE_ALPHA = 0x4;
constexpr uint32_t TEXTURE_TRANSPARENCY = 0x8;
constexpr uint32_t TEXTURE_BORDER = 0x10;
constexpr uint32_t FILTER_NEAREST = 0x1;
constexpr uint32_t FILTER_LINEAR = 0x2;
constexpr uint32_t FILTER_MIPNEAREST = 0x4;
constexpr uint32_t FILTER_MIPLINEAR = 0x8;
constexpr uint32_t FILTER_LINEARMIPNEAREST = 0x10;
constexpr uint32_t FILTER_LINEARMIPLINEAR = 0x20;
constexpr uint32_t FILTER_MINFPOINT = 0x100;
constexpr uint32_t FILTER_MINFLINEAR = 0x200;
constexpr uint32_t FILTER_MIPFPOINT = 0x10000;
constexpr uint32_t FILTER_MIPFLINEAR = 0x20000;
constexpr uint32_t FILTER_MAGFPOINT = 0x1000000;
constexpr uint32_t FILTER_MAGFLINEAR = 0x2000000;
constexpr uint32_t ADDRESS_WRAP = 0x1;
constexpr uint32_t ADDRESS_MIRROR = 0x2;
constexpr uint32_t ADDRESS_CLAMP = 0x4;
constexpr uint32_t ADDRESS_BORDER = 0x8;
constexpr uint32_t ADDRESS_INDEPENDENTUV = 0x10;
constexpr uint32_t TEXOP_PREMODULATE = 0x10000;
constexpr uint32_t TEXOP_MODULATEALPHA_ADDCOLOR = 0x20000;
constexpr uint32_t TEXOP_BUMPENVMAP = 0x200000;
constexpr uint32_t BITDEPTH_16 = 0x400;  // DDBD_16
constexpr uint32_t BITDEPTH_32 = 0x100;  // DDBD_32
}  // namespace d3dcaps

struct DeviceCaps {
    uint32_t devCaps = 0;
    uint32_t miscCaps = 0;
    uint32_t rasterCaps = 0;
    uint32_t zCmpCaps = 0;
    uint32_t srcBlendCaps = 0;
    uint32_t destBlendCaps = 0;
    uint32_t alphaCmpCaps = 0;
    uint32_t shadeCaps = 0;
    uint32_t textureCaps = 0;
    uint32_t filterCaps = 0;
    uint32_t addressCaps = 0;
    uint32_t renderBitDepth = 0;
    uint32_t zBufferBitDepth = 0;
    uint32_t minTextureSize = 0;
    uint32_t maxTextureSize = 0;
    uint32_t maxTextureRepeat = 0;
    uint32_t maxAspectRatio = 0;
    uint32_t maxAnisotropy = 0;
    uint32_t fvfCaps = 0;
    uint32_t textureOpCaps = 0;
    uint16_t maxBlendStages = 0;
    uint16_t maxSimultaneousTextures = 0;
};

// What IDirect3DDevice7::GetCaps reports: what the client checks (surfaces-present-caps.md §5) and what the
// renderer really does (states.md §8): two texture stages, the shader's stage operations, every blend factor.
DeviceCaps deviceCaps();

// EnumTextureFormats: only 16-bit formats, like the cards of its time, so the client picks what it always did
// (spec §4.4); the client's A1R5G5B5 and A4R4G4B4 first. The GPU keeps every texture as RGBA8 anyway.
const std::vector<PixelFormat>& textureFormats();

}  // namespace uagfx
