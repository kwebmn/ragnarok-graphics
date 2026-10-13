#include "core/device_caps.hpp"

#include "core/d3d7.hpp"

namespace uagfx {
namespace {

// The stage operations combine() in the pixel shader implements (Task 7): all of 1-21 except PREMODULATE.
uint32_t shaderTextureOps() {
    uint32_t ops = 0;
    for (uint32_t op = d3d7::TOP_DISABLE; op <= d3d7::TOP_MODULATEINVCOLOR_ADDALPHA; ++op)
        if (op != d3d7::TOP_PREMODULATE) ops |= 1u << (op - 1);
    return ops;
}

}  // namespace

DeviceCaps deviceCaps() {
    using namespace d3dcaps;
    DeviceCaps c;
    c.devCaps = DEV_FLOATTLVERTEX | DEV_TEXTUREVIDEOMEMORY | DEV_DRAWPRIMTLVERTEX | DEV_CANRENDERAFTERFLIP |
                DEV_DRAWPRIMITIVES2 | DEV_DRAWPRIMITIVES2EX | DEV_HWRASTERIZATION;
    c.miscCaps = MISC_CULLNONE | MISC_CULLCW | MISC_CULLCCW;
    c.rasterCaps = RASTER_DITHER | RASTER_ZTEST | RASTER_SUBPIXEL | RASTER_FOGVERTEX;
    c.zCmpCaps = CMP_ALL;
    c.srcBlendCaps = BLEND_ALL;
    c.destBlendCaps = BLEND_ALL;
    c.alphaCmpCaps = CMP_ALL;
    c.shadeCaps = SHADE_COLORGOURAUDRGB | SHADE_SPECULARGOURAUDRGB | SHADE_ALPHAGOURAUDBLEND | SHADE_FOGGOURAUD;
    c.textureCaps = TEXTURE_PERSPECTIVE | TEXTURE_ALPHA | TEXTURE_TRANSPARENCY | TEXTURE_BORDER;
    c.filterCaps = FILTER_NEAREST | FILTER_LINEAR | FILTER_MIPNEAREST | FILTER_MIPLINEAR | FILTER_LINEARMIPNEAREST |
                   FILTER_LINEARMIPLINEAR | FILTER_MINFPOINT | FILTER_MINFLINEAR | FILTER_MIPFPOINT |
                   FILTER_MIPFLINEAR | FILTER_MAGFPOINT | FILTER_MAGFLINEAR;
    c.addressCaps = ADDRESS_WRAP | ADDRESS_MIRROR | ADDRESS_CLAMP | ADDRESS_BORDER | ADDRESS_INDEPENDENTUV;
    c.renderBitDepth = BITDEPTH_16 | BITDEPTH_32;
    c.zBufferBitDepth = BITDEPTH_16;
    c.minTextureSize = 1;
    c.maxTextureSize = 4096;
    c.maxTextureRepeat = 8192;
    c.maxAspectRatio = 0;
    c.maxAnisotropy = 1;  // anisotropic filtering arrives with stage 4 of the spec
    c.fvfCaps = 2;        // two texture coordinate sets (FVF 0x2C4)
    c.textureOpCaps = shaderTextureOps();
    c.maxBlendStages = 2;
    c.maxSimultaneousTextures = 2;
    return c;
}

const std::vector<PixelFormat>& textureFormats() {
    static const std::vector<PixelFormat> formats = {PixelFormat::A1R5G5B5, PixelFormat::A4R4G4B4,
                                                     PixelFormat::R5G6B5, PixelFormat::X1R5G5B5};
    return formats;
}

}  // namespace uagfx
