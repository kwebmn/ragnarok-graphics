#include "core/d3d_map.hpp"

#include <algorithm>

#include "core/d3d7.hpp"

namespace uagfx::d3dmap {

Sampler samplerOf(const StageSetup& s, int aniso) {
    Sampler d;
    const bool linearMin = s.minFilter == d3d7::TFN_LINEAR || s.minFilter == d3d7::TFN_ANISOTROPIC;
    const bool linearMag = s.magFilter == d3d7::TFG_LINEAR || s.magFilter == d3d7::TFG_ANISOTROPIC;
    const bool linearMip = s.mipFilter == d3d7::TFP_LINEAR;
    d.mips = s.mipFilter == d3d7::TFP_POINT || linearMip;
    d.filter = (linearMin ? 0x10u : 0u) | (linearMag ? 0x4u : 0u) | (linearMip ? 0x1u : 0u);
    const auto address = [](uint8_t a) {
        return a >= d3d7::TADDRESS_WRAP && a <= d3d7::TADDRESS_BORDER ? uint32_t(a) : uint32_t(d3d7::TADDRESS_WRAP);
    };
    d.addressU = address(s.addressU);
    d.addressV = address(s.addressV);
    if (aniso >= 2 && linearMin && linearMag) {  // mips, when the texture has them, are linear then
        d.filter = 0x55;                         // ANISOTROPIC
        d.maxAnisotropy = uint32_t(std::min(aniso, 16));
    }
    d.border[0] = float((s.borderColor >> 16) & 255) / 255.0f;
    d.border[1] = float((s.borderColor >> 8) & 255) / 255.0f;
    d.border[2] = float(s.borderColor & 255) / 255.0f;
    d.border[3] = float(s.borderColor >> 24) / 255.0f;
    return d;
}

uint32_t compareOf(uint8_t func) {
    return func >= d3d7::CMP_NEVER && func <= d3d7::CMP_ALWAYS ? uint32_t(func) : uint32_t(d3d7::CMP_ALWAYS);
}

uint32_t cullOf(uint8_t cull) {
    return cull >= d3d7::CULL_NONE && cull <= d3d7::CULL_CCW ? uint32_t(cull) : uint32_t(d3d7::CULL_NONE);
}

uint32_t blendOf(uint8_t factor, uint8_t none) { return factor ? uint32_t(factor) : uint32_t(none); }

}  // namespace uagfx::d3dmap
