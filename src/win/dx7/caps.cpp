#include "win/dx7/caps.hpp"

#include <cstddef>
#include <cstring>

#include "core/device_caps.hpp"

namespace uagfx::dx7 {

// The client reads these fields by offset (surfaces-present-caps.md §5): the header has to agree.
static_assert(offsetof(D3DDEVICEDESC7, dpcTriCaps) + offsetof(D3DPRIMCAPS, dwRasterCaps) == 0x44, "RasterCaps");
static_assert(offsetof(D3DDEVICEDESC7, dpcTriCaps) + offsetof(D3DPRIMCAPS, dwTextureFilterCaps) == 0x60, "FilterCaps");
static_assert(offsetof(D3DDEVICEDESC7, dwMinTextureWidth) == 0x7C, "dwMinTextureWidth");
static_assert(offsetof(D3DDEVICEDESC7, dwMaxTextureWidth) == 0x84, "dwMaxTextureWidth");
static_assert(offsetof(D3DDEVICEDESC7, dwMaxTextureAspectRatio) == 0x90, "dwMaxTextureAspectRatio");
static_assert(offsetof(D3DDEVICEDESC7, dwTextureOpCaps) == 0xB4, "dwTextureOpCaps");
static_assert(offsetof(D3DDEVICEDESC7, wMaxTextureBlendStages) == 0xB8, "wMaxTextureBlendStages");
// The core's numbers are the headers'.
static_assert(d3dcaps::DEV_HWRASTERIZATION == D3DDEVCAPS_HWRASTERIZATION, "");
static_assert(d3dcaps::DEV_DRAWPRIMITIVES2EX == D3DDEVCAPS_DRAWPRIMITIVES2EX, "");
static_assert(d3dcaps::MISC_CULLCCW == D3DPMISCCAPS_CULLCCW, "");
static_assert(d3dcaps::RASTER_FOGVERTEX == D3DPRASTERCAPS_FOGVERTEX, "");
static_assert(d3dcaps::RASTER_ZBUFFERLESSHSR == D3DPRASTERCAPS_ZBUFFERLESSHSR, "");
static_assert(d3dcaps::BLEND_ALL == (D3DPBLENDCAPS_BOTHINVSRCALPHA << 1) - 1, "");
static_assert(d3dcaps::CMP_ALL == (D3DPCMPCAPS_ALWAYS << 1) - 1, "");
static_assert(d3dcaps::SHADE_FOGGOURAUD == D3DPSHADECAPS_FOGGOURAUD, "");
static_assert(d3dcaps::TEXTURE_BORDER == D3DPTEXTURECAPS_BORDER, "");
static_assert(d3dcaps::FILTER_LINEARMIPLINEAR == D3DPTFILTERCAPS_LINEARMIPLINEAR, "");
static_assert(d3dcaps::FILTER_MAGFLINEAR == D3DPTFILTERCAPS_MAGFLINEAR, "");
static_assert(d3dcaps::ADDRESS_INDEPENDENTUV == D3DPTADDRESSCAPS_INDEPENDENTUV, "");
static_assert(d3dcaps::TEXOP_PREMODULATE == D3DTEXOPCAPS_PREMODULATE, "");
static_assert(d3dcaps::TEXOP_MODULATEALPHA_ADDCOLOR == D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR, "");
static_assert(d3dcaps::TEXOP_BUMPENVMAP == D3DTEXOPCAPS_BUMPENVMAP, "");
static_assert(d3dcaps::BITDEPTH_16 == DDBD_16 && d3dcaps::BITDEPTH_32 == DDBD_32, "");

void fillDeviceDesc(D3DDEVICEDESC7& d) {
    const DeviceCaps c = deviceCaps();
    std::memset(&d, 0, sizeof d);
    d.dwDevCaps = c.devCaps;
    D3DPRIMCAPS p;
    std::memset(&p, 0, sizeof p);
    p.dwSize = sizeof p;
    p.dwMiscCaps = c.miscCaps;
    p.dwRasterCaps = c.rasterCaps;
    p.dwZCmpCaps = c.zCmpCaps;
    p.dwSrcBlendCaps = c.srcBlendCaps;
    p.dwDestBlendCaps = c.destBlendCaps;
    p.dwAlphaCmpCaps = c.alphaCmpCaps;
    p.dwShadeCaps = c.shadeCaps;
    p.dwTextureCaps = c.textureCaps;
    p.dwTextureFilterCaps = c.filterCaps;
    p.dwTextureAddressCaps = c.addressCaps;
    d.dpcLineCaps = p;
    d.dpcTriCaps = p;
    d.dwDeviceRenderBitDepth = c.renderBitDepth;
    d.dwDeviceZBufferBitDepth = c.zBufferBitDepth;
    d.dwMinTextureWidth = c.minTextureSize;
    d.dwMinTextureHeight = c.minTextureSize;
    d.dwMaxTextureWidth = c.maxTextureSize;
    d.dwMaxTextureHeight = c.maxTextureSize;
    d.dwMaxTextureRepeat = c.maxTextureRepeat;
    d.dwMaxTextureAspectRatio = c.maxAspectRatio;
    d.dwMaxAnisotropy = c.maxAnisotropy;
    d.dwFVFCaps = c.fvfCaps;
    d.dwTextureOpCaps = c.textureOpCaps;
    d.wMaxTextureBlendStages = c.maxBlendStages;
    d.wMaxSimultaneousTextures = c.maxSimultaneousTextures;
    d.dvMaxVertexW = 1e10f;
    d.deviceGUID = IID_IDirect3DHALDevice;
}

}  // namespace uagfx::dx7
