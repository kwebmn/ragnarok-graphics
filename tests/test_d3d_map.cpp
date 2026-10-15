#include "core/d3d7.hpp"
#include "core/d3d_map.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(d3d_map_samplers_as_direct3d_12_makes_them) {
    // 0.46: the Direct3D 11 renderer's samplers follow pipeline.cpp's samplerFor - the same D3D1x_FILTER numbers.
    StageSetup s;
    s.minFilter = d3d7::TFN_LINEAR;
    s.magFilter = d3d7::TFG_LINEAR;
    s.mipFilter = d3d7::TFP_LINEAR;
    s.addressU = d3d7::TADDRESS_CLAMP;
    s.addressV = 0;  // not set: wrap
    s.borderColor = 0x80FF4000;
    d3dmap::Sampler d = d3dmap::samplerOf(s, 0);
    CHECK_EQ(d.filter, 0x15u);  // MIN_MAG_MIP_LINEAR
    CHECK(d.addressU == 3 && d.addressV == 1 && d.maxAnisotropy == 1 && d.mips);
    CHECK(d.border[0] == 1.0f && d.border[1] > 0.25f && d.border[1] < 0.26f && d.border[2] == 0.0f);
    CHECK(d.border[3] > 0.50f && d.border[3] < 0.51f);
    // Anisotropy for a world texture filtered linearly both ways.
    d = d3dmap::samplerOf(s, 8);
    CHECK(d.filter == 0x55u && d.maxAnisotropy == 8);
    CHECK_EQ(d3dmap::samplerOf(s, 32).maxAnisotropy, 16u);
    // Point sampling, no mips.
    StageSetup p;
    p.minFilter = d3d7::TFN_POINT;
    p.magFilter = d3d7::TFG_POINT;
    p.mipFilter = d3d7::TFP_NONE;
    d = d3dmap::samplerOf(p, 8);
    CHECK(d.filter == 0u && d.maxAnisotropy == 1 && !d.mips);
    // Linear magnification alone, point mips.
    p.magFilter = d3d7::TFG_LINEAR;
    p.mipFilter = d3d7::TFP_POINT;
    d = d3dmap::samplerOf(p, 0);
    CHECK(d.filter == 0x4u && d.mips);
}

TEST(d3d_map_states_keep_direct3d_7s_numbers) {
    CHECK_EQ(d3dmap::compareOf(d3d7::CMP_LESSEQUAL), uint32_t(d3d7::CMP_LESSEQUAL));
    CHECK_EQ(d3dmap::compareOf(0), uint32_t(d3d7::CMP_ALWAYS));
    CHECK_EQ(d3dmap::compareOf(9), uint32_t(d3d7::CMP_ALWAYS));
    CHECK_EQ(d3dmap::cullOf(d3d7::CULL_CCW), 3u);
    CHECK_EQ(d3dmap::cullOf(0), 1u);
    CHECK_EQ(d3dmap::blendOf(d3d7::BLEND_SRCALPHA, d3d7::BLEND_ONE), uint32_t(d3d7::BLEND_SRCALPHA));
    CHECK_EQ(d3dmap::blendOf(0, d3d7::BLEND_ONE), uint32_t(d3d7::BLEND_ONE));
    CHECK_EQ(d3dmap::blendOf(0, d3d7::BLEND_ZERO), uint32_t(d3d7::BLEND_ZERO));
}
