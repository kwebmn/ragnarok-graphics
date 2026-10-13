#include "core/pixel.hpp"
#include "core/surface_plan.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
SurfaceRequest req(uint32_t flags, uint32_t caps, int w = 0, int h = 0) {
    SurfaceRequest r;
    r.flags = flags;
    r.caps = caps;
    r.width = w;
    r.height = h;
    return r;
}
constexpr PixelFormat kScreen = PixelFormat::X8R8G8B8;
}  // namespace

// The rows of surfaces-present-caps.md §1.
TEST(plan_windowed_primary) {
    const SurfacePlan p = planSurface(req(ddsd::CAPS, ddcaps::PRIMARYSURFACE), 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::Primary);
    CHECK_EQ(p.width, 1024);
    CHECK_EQ(p.height, 768);
    CHECK(p.format == kScreen);
    CHECK(!p.backBuffer);
}

TEST(plan_fullscreen_flip_chain) {
    SurfaceRequest r = req(ddsd::CAPS | ddsd::BACKBUFFERCOUNT,
                           ddcaps::PRIMARYSURFACE | ddcaps::D3DDEVICE | ddcaps::FLIP | ddcaps::COMPLEX);
    r.backBuffers = 1;
    const SurfacePlan p = planSurface(r, 800, 600, PixelFormat::R5G6B5);
    CHECK(p.role == SurfaceRole::Primary);
    CHECK(p.backBuffer);
    CHECK(p.format == PixelFormat::R5G6B5);
}

TEST(plan_windowed_back_buffer_is_the_render_target) {
    const SurfacePlan p = planSurface(req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT,
                                          ddcaps::D3DDEVICE | ddcaps::OFFSCREENPLAIN, 1600, 863),
                                      1920, 1080, kScreen);
    CHECK(p.role == SurfaceRole::RenderTarget);
    CHECK_EQ(p.width, 1600);
    CHECK_EQ(p.height, 863);
    CHECK(p.format == kScreen);
}

TEST(plan_system_memory_copy_and_keyed_offscreen) {
    const SurfacePlan sys = planSurface(req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT,
                                            ddcaps::OFFSCREENPLAIN | ddcaps::SYSTEMMEMORY, 1024, 768),
                                        1024, 768, kScreen);
    CHECK(sys.role == SurfaceRole::Offscreen);
    CHECK(sys.format == kScreen);
    SurfaceRequest keyed = req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT | ddsd::CKSRCBLT, ddcaps::OFFSCREENPLAIN, 1600, 864);
    keyed.keyLow = keyed.keyHigh = 0xF81F;
    const SurfacePlan k = planSurface(keyed, 1024, 768, PixelFormat::R5G6B5);
    CHECK(k.role == SurfaceRole::Offscreen);
    CHECK(k.colorKey);
    CHECK_EQ(k.keyLow, 0xF81Fu);
    CHECK(k.format == PixelFormat::R5G6B5);  // no DDSD_PIXELFORMAT: the screen's format
}

TEST(plan_z_buffer) {
    SurfaceRequest r = req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT | ddsd::PIXELFORMAT,
                           ddcaps::ZBUFFER | ddcaps::VIDEOMEMORY, 1024, 768);
    r.zBits = 16;
    const SurfacePlan p = planSurface(r, 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::ZBuffer);
    CHECK_EQ(p.width, 1024);
}

TEST(plan_textures_with_and_without_mipmaps) {
    SurfaceRequest r = req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT | ddsd::PIXELFORMAT | ddsd::TEXTURESTAGE,
                           ddcaps::TEXTURE, 256, 128);
    r.caps2 = ddcaps::CAPS2_TEXTUREMANAGE;
    r.format = PixelFormat::A1R5G5B5;
    SurfacePlan p = planSurface(r, 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::Texture);
    CHECK(p.format == PixelFormat::A1R5G5B5);
    CHECK_EQ(p.mipLevels, 1);
    r.flags |= ddsd::MIPMAPCOUNT;
    r.caps |= ddcaps::MIPMAP | ddcaps::COMPLEX;
    r.mipLevels = 7;
    p = planSurface(r, 1024, 768, kScreen);
    CHECK_EQ(p.mipLevels, 7);
    r.mipLevels = 20;  // more than the size allows: clamp to log2(min side) + 1
    p = planSurface(r, 1024, 768, kScreen);
    CHECK_EQ(p.mipLevels, 8);
}

TEST(plan_rejects_what_the_client_never_asks_for) {
    SurfacePlan p = planSurface(req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT, ddcaps::TEXTURE, 64, 64), 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::Unsupported);  // a texture without a pixel format
    CHECK(!p.error.empty());
    p = planSurface(req(ddsd::CAPS, ddcaps::OFFSCREENPLAIN), 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::Unsupported);  // no size
}

TEST(target_to_client_floors_into_16_bits) {
    CHECK_EQ(targetToClient(PixelFormat::R5G6B5, 0xFFFFFFFF), 0xFFFFu);
    CHECK_EQ(targetToClient(PixelFormat::R5G6B5, 0xFF000000), 0x0000u);
    // 253 is a bilinear 0.9921875 rounded to 8 bits: floor(253 * 31 / 255) = 30, not 31 (calibration.md §5).
    CHECK_EQ(targetToClient(PixelFormat::R5G6B5, 0xFFFD0000), uint32_t(30u << 11));
    CHECK_EQ(targetToClient(PixelFormat::R5G6B5, 0xFF00FD00), uint32_t(62u << 5));
    CHECK_EQ(targetToClient(PixelFormat::X1R5G5B5, 0xFFFFFFFF), 0x7FFFu);
    CHECK_EQ(targetToClient(PixelFormat::X8R8G8B8, 0xFF123456), 0x123456u);
}
