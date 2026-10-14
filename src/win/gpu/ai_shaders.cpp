#include "win/gpu/ai_textures.hpp"

namespace uagfx::gpu {

// Neural textures' compute shaders (cs_5_0). Every buffer is raw (byte addressed) and bound as a root descriptor, so
// no descriptor heap is touched: t0 the network's weights, u0/u1/u2 by shader as noted. The constants (b0) are
// sixteen words whose meaning each entry point names.
const char* aiShaderSource() {
    return R"HLSL(
cbuffer Params : register(b0) {
    uint4 c0;
    uint4 c1;
    uint4 c2;
    uint4 c3;
};
ByteAddressBuffer W : register(t0);
RWByteAddressBuffer U0 : register(u0);
RWByteAddressBuffer U1 : register(u1);
RWByteAddressBuffer U2 : register(u2);

float weight(uint i) { return asfloat(W.Load(i * 4)); }

// One 3x3 convolution over rows [rowBegin, rowEnd) of the input, 16 output channels per thread (z picks which 16),
// PReLU after it when preluAt is not ~0. U0: input planes, U1: output planes. Past an edge it reads the other side
// (ai::Border::Wrap): a texture repeats on the ground, and so must the enlarged one - or zeros (zero not 0: a cell of
// the sprite cache, a picture of its own, ai::Border::Zero).
#define CONV_WIDTH c0.x
#define CONV_HEIGHT c0.y
#define CONV_IN c0.z
#define CONV_OUT c0.w
#define CONV_BIAS c1.x
#define CONV_WEIGHT c1.y
#define CONV_PRELU c1.z
#define CONV_ROW_BEGIN c1.w
#define CONV_ROW_END c2.x
#define CONV_ZERO c2.y

groupshared float tile[10][10];

[numthreads(8, 8, 1)]
void ai_conv(uint3 g : SV_GroupID, uint3 t : SV_GroupThreadID) {
    const uint plane = CONV_WIDTH * CONV_HEIGHT;
    const int x = int(g.x * 8 + t.x);
    const int y = int(CONV_ROW_BEGIN + g.y * 8 + t.y);
    const uint o0 = g.z * 16;
    float acc[16];
    [unroll] for (uint k = 0; k < 16; ++k) acc[k] = o0 + k < CONV_OUT ? weight(CONV_BIAS + o0 + k) : 0.0;
    const uint flat = t.y * 8 + t.x;
    for (uint i = 0; i < CONV_IN; ++i) {
        for (uint l = flat; l < 100; l += 64) {
            const int lx = int(g.x * 8) + int(l % 10) - 1;
            const int ly = int(CONV_ROW_BEGIN + g.y * 8) + int(l / 10) - 1;
            const bool outside = lx < 0 || ly < 0 || lx >= int(CONV_WIDTH) || ly >= int(CONV_HEIGHT);
            const uint sx = uint(lx + int(CONV_WIDTH)) % CONV_WIDTH;
            const uint sy = uint(ly + int(CONV_HEIGHT)) % CONV_HEIGHT;
            tile[l / 10][l % 10] =
                CONV_ZERO != 0 && outside ? 0.0 : asfloat(U0.Load((i * plane + sy * CONV_WIDTH + sx) * 4));
        }
        GroupMemoryBarrierWithGroupSync();
        float v9[9];
        [unroll] for (uint q = 0; q < 9; ++q) v9[q] = tile[t.y + q / 3][t.x + q % 3];
        [unroll] for (uint k2 = 0; k2 < 16; ++k2) {
            const uint o = o0 + k2;
            if (o < CONV_OUT) {
                const uint base = CONV_WEIGHT + (o * CONV_IN + i) * 9;
                float s = 0.0;
                [unroll] for (uint q2 = 0; q2 < 9; ++q2) s += weight(base + q2) * v9[q2];
                acc[k2] += s;
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (x < int(CONV_WIDTH) && y < int(CONV_ROW_END) && y < int(CONV_HEIGHT)) {
        [unroll] for (uint k3 = 0; k3 < 16; ++k3) {
            const uint o = o0 + k3;
            if (o < CONV_OUT) {
                float a = acc[k3];
                if (CONV_PRELU != 0xFFFFFFFF && a < 0.0) a *= weight(CONV_PRELU + o);
                U1.Store((o * plane + uint(y) * CONV_WIDTH + uint(x)) * 4, asuint(a));
            }
        }
    }
}

// The last step: pixel shuffle by 4 of the last convolution's 48 planes (U0), plus the input planes (U2: R, G, B, A)
// enlarged by nearest, clamped; alpha bilinear between texel centres and cut at one half. U1: the image, ARGB
// words, pitch words a row from word imageAt.
#define SH_WIDTH c0.x
#define SH_HEIGHT c0.y
#define SH_PITCH c0.z
#define SH_AT c0.w

[numthreads(8, 8, 1)]
void ai_shuffle(uint3 id : SV_DispatchThreadID) {
    const uint ow = SH_WIDTH * 4, oh = SH_HEIGHT * 4;
    if (id.x >= ow || id.y >= oh) return;
    const uint plane = SH_WIDTH * SH_HEIGHT;
    const uint sx = id.x / 4, sy = id.y / 4, i = id.y % 4, j = id.x % 4;
    uint rgb[3];
    [unroll] for (uint c = 0; c < 3; ++c) {
        const float n = asfloat(U0.Load(((c * 16 + i * 4 + j) * plane + sy * SH_WIDTH + sx) * 4));
        const float b = asfloat(U2.Load((c * plane + sy * SH_WIDTH + sx) * 4));
        rgb[c] = uint(saturate(n + b) * 255.0 + 0.5);
    }
    const float fx = clamp((float(id.x) + 0.5) / 4.0 - 0.5, 0.0, float(SH_WIDTH - 1));
    const float fy = clamp((float(id.y) + 0.5) / 4.0 - 0.5, 0.0, float(SH_HEIGHT - 1));
    const uint x0 = uint(fx), y0 = uint(fy);
    const uint x1 = min(x0 + 1, SH_WIDTH - 1), y1 = min(y0 + 1, SH_HEIGHT - 1);
    const float tx = fx - float(x0), ty = fy - float(y0);
    const uint ap = 3 * plane;
    const float a00 = asfloat(U2.Load((ap + y0 * SH_WIDTH + x0) * 4));
    const float a10 = asfloat(U2.Load((ap + y0 * SH_WIDTH + x1) * 4));
    const float a01 = asfloat(U2.Load((ap + y1 * SH_WIDTH + x0) * 4));
    const float a11 = asfloat(U2.Load((ap + y1 * SH_WIDTH + x1) * 4));
    const float av = (a00 * (1.0 - tx) + a10 * tx) * (1.0 - ty) + (a01 * (1.0 - tx) + a11 * tx) * ty;
    const uint a = av >= 0.5 ? 255u : 0u;
    U1.Store((SH_AT + id.y * SH_PITCH + id.x) * 4, (a << 24) | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2]);
}

// 0.13.1, a texture with transparent texels: its alpha through the network as a grey picture (Real-ESRGAN's way for
// RGBA pictures; U2 holds A in the R, G and B planes), the mean of the three channels cut at one half - into the
// image's alpha, its colour kept. A thin line stays a line and a staircase becomes an edge, where the bilinear
// alpha of ai_shuffle broke lines into dots.
[numthreads(8, 8, 1)]
void ai_alpha(uint3 id : SV_DispatchThreadID) {
    const uint ow = SH_WIDTH * 4, oh = SH_HEIGHT * 4;
    if (id.x >= ow || id.y >= oh) return;
    const uint plane = SH_WIDTH * SH_HEIGHT;
    const uint sx = id.x / 4, sy = id.y / 4, i = id.y % 4, j = id.x % 4;
    float v = 0.0;
    [unroll] for (uint c = 0; c < 3; ++c) {
        const float n = asfloat(U0.Load(((c * 16 + i * 4 + j) * plane + sy * SH_WIDTH + sx) * 4));
        const float b = asfloat(U2.Load((c * plane + sy * SH_WIDTH + sx) * 4));
        v += saturate(n + b);
    }
    const uint at = (SH_AT + id.y * SH_PITCH + id.x) * 4;
    U1.Store(at, (v >= 1.5 ? 0xFF000000u : 0u) | (U1.Load(at) & 0x00FFFFFFu));
}

// One mip level from the one above it, both in the image (U1): the average of up to four texels, alpha again all
// or nothing so that the colour key's edge stays an edge further away.
#define MIP_SRC_W c0.x
#define MIP_SRC_H c0.y
#define MIP_SRC_PITCH c0.z
#define MIP_SRC_AT c0.w
#define MIP_DST_W c1.x
#define MIP_DST_H c1.y
#define MIP_DST_PITCH c1.z
#define MIP_DST_AT c1.w

uint4 unpack(uint v) { return uint4((v >> 16) & 255, (v >> 8) & 255, v & 255, v >> 24); }

[numthreads(8, 8, 1)]
void ai_mip(uint3 id : SV_DispatchThreadID) {
    if (id.x >= MIP_DST_W || id.y >= MIP_DST_H) return;
    uint4 s = 0;
    uint n = 0;
    [unroll] for (uint dy = 0; dy < 2; ++dy)
        [unroll] for (uint dx = 0; dx < 2; ++dx) {
            const uint x = id.x * 2 + dx, y = id.y * 2 + dy;
            if (x < MIP_SRC_W && y < MIP_SRC_H) {
                s += unpack(U1.Load((MIP_SRC_AT + y * MIP_SRC_PITCH + x) * 4));
                ++n;
            }
        }
    const uint4 m = (s + n / 2) / max(n, 1u);
    const uint a = m.w >= 128 ? 255u : 0u;
    U1.Store((MIP_DST_AT + id.y * MIP_DST_PITCH + id.x) * 4, (a << 24) | (m.x << 16) | (m.y << 8) | m.z);
}
)HLSL";
}

}  // namespace uagfx::gpu
