#include <vector>

#include "core/d3d7.hpp"
#include "core/device_caps.hpp"
#include "test.hpp"

using namespace uagfx;

// surfaces-present-caps.md §5: the fields the client reads and what it does with them.
TEST(devcaps_what_the_client_checks) {
    const DeviceCaps c = deviceCaps();
    CHECK((c.rasterCaps & d3dcaps::RASTER_FOGVERTEX) != 0);        // otherwise "does not support fog effect"
    CHECK((c.rasterCaps & d3dcaps::RASTER_ZBUFFERLESSHSR) == 0);   // otherwise the client creates no Z-buffer
    CHECK((c.filterCaps & d3dcaps::FILTER_LINEARMIPLINEAR) != 0);  // mipmaps with TRILINEARFILTER
    CHECK(c.minTextureSize <= 4);                                  // the 4x4 calibration texture stays 4x4
    CHECK_EQ(c.maxTextureSize, 4096u);
    CHECK_EQ(c.maxAspectRatio, 0u);                                // 0: the client does not clamp the aspect
    CHECK((c.textureOpCaps & d3dcaps::TEXOP_MODULATEALPHA_ADDCOLOR) != 0);  // ground + lightmap in one pass
    CHECK(c.maxBlendStages >= 2);
}

TEST(devcaps_texture_ops_are_the_shaders) {
    // combine() in the pixel shader (Task 7) does operations 1-16 and 18-21; report exactly those.
    const uint32_t ops = deviceCaps().textureOpCaps;
    for (uint32_t op = d3d7::TOP_DISABLE; op <= d3d7::TOP_MODULATEINVCOLOR_ADDALPHA; ++op)
        CHECK(((ops >> (op - 1)) & 1u) == (op == d3d7::TOP_PREMODULATE ? 0u : 1u));
    CHECK((ops >> d3d7::TOP_MODULATEINVCOLOR_ADDALPHA) == 0u);  // no bump mapping, no dot product
    CHECK_EQ(deviceCaps().maxBlendStages, uint16_t(2));         // the shader has two stages
}

TEST(devcaps_texture_formats_start_with_the_clients) {
    const std::vector<PixelFormat>& f = textureFormats();
    CHECK(f.size() >= 2);
    CHECK(f[0] == PixelFormat::A1R5G5B5);  // format codes 0 and 1 of the client (surfaces-present-caps.md §6)
    CHECK(f[1] == PixelFormat::A4R4G4B4);
    for (PixelFormat p : f) CHECK_EQ(bytesPerPixel(p), 2);  // 16-bit only (spec §4.4)
}
