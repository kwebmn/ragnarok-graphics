#include "win/dx7/formats.hpp"

#include <cstring>

namespace uagfx::dx7 {

void fillPixelFormat(DDPIXELFORMAT& pf, PixelFormat f) {
    std::memset(&pf, 0, sizeof pf);
    pf.dwSize = sizeof pf;
    const Masks m = masksOf(f);
    pf.dwFlags = DDPF_RGB | (m.a ? DDPF_ALPHAPIXELS : 0);
    pf.dwRGBBitCount = m.bits;
    pf.dwRBitMask = m.r;
    pf.dwGBitMask = m.g;
    pf.dwBBitMask = m.b;
    pf.dwRGBAlphaBitMask = m.a;
}

PixelFormat readPixelFormat(const DDPIXELFORMAT& pf) {
    if (!(pf.dwFlags & DDPF_RGB)) return PixelFormat::Unknown;
    return formatFromMasks(pf.dwRGBBitCount, pf.dwRBitMask, pf.dwGBitMask, pf.dwBBitMask,
                           (pf.dwFlags & DDPF_ALPHAPIXELS) ? pf.dwRGBAlphaBitMask : 0);
}

void fillSurfaceDesc(DDSURFACEDESC2& d, int width, int height, int pitch, PixelFormat f, DWORD caps, void* bits) {
    const DWORD size = d.dwSize ? d.dwSize : sizeof d;
    std::memset(&d, 0, sizeof d);
    d.dwSize = size;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | (bits ? DDSD_LPSURFACE : 0);
    d.dwWidth = DWORD(width);
    d.dwHeight = DWORD(height);
    d.lPitch = pitch;
    d.lpSurface = bits;
    fillPixelFormat(d.ddpfPixelFormat, f);
    d.ddsCaps.dwCaps = caps;
}

}  // namespace uagfx::dx7
