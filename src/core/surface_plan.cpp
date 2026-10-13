#include "core/surface_plan.hpp"

#include <algorithm>

namespace uagfx {
namespace {

int maxLevels(int w, int h) {
    int levels = 1;
    for (int side = std::min(w, h); side > 1; side >>= 1) ++levels;
    return levels;
}

SurfacePlan unsupported(const std::string& why) {
    SurfacePlan p;
    p.error = why;
    return p;
}

}  // namespace

SurfacePlan planSurface(const SurfaceRequest& r, int screenWidth, int screenHeight, PixelFormat screenFormat) {
    SurfacePlan p;
    if (!(r.flags & ddsd::CAPS)) return unsupported("no DDSD_CAPS");
    if (r.flags & ddsd::CKSRCBLT) {
        p.colorKey = true;
        p.keyLow = r.keyLow;
        p.keyHigh = r.keyHigh;
    }
    if (r.caps & ddcaps::PRIMARYSURFACE) {
        p.role = SurfaceRole::Primary;
        p.width = screenWidth;
        p.height = screenHeight;
        p.format = screenFormat;
        p.backBuffer = (r.caps & ddcaps::FLIP) && (r.flags & ddsd::BACKBUFFERCOUNT) && r.backBuffers >= 1;
        return p;
    }
    if (!(r.flags & ddsd::WIDTH) || !(r.flags & ddsd::HEIGHT) || r.width <= 0 || r.height <= 0)
        return unsupported("no size");
    p.width = r.width;
    p.height = r.height;
    if (r.caps & ddcaps::ZBUFFER) {
        p.role = SurfaceRole::ZBuffer;
        return p;
    }
    if (r.caps & ddcaps::TEXTURE) {
        if (!(r.flags & ddsd::PIXELFORMAT) || r.format == PixelFormat::Unknown)
            return unsupported("texture without a known pixel format");
        p.role = SurfaceRole::Texture;
        p.format = r.format;
        if ((r.caps & ddcaps::MIPMAP) && (r.flags & ddsd::MIPMAPCOUNT) && r.mipLevels > 1)
            p.mipLevels = std::min(int(r.mipLevels), maxLevels(r.width, r.height));
        return p;
    }
    p.format = (r.flags & ddsd::PIXELFORMAT) && r.format != PixelFormat::Unknown ? r.format : screenFormat;
    if (r.caps & ddcaps::D3DDEVICE) {
        p.role = SurfaceRole::RenderTarget;
        return p;
    }
    if (r.caps & ddcaps::OFFSCREENPLAIN) {
        p.role = SurfaceRole::Offscreen;
        return p;
    }
    return unsupported("caps the client never uses");
}

}  // namespace uagfx
