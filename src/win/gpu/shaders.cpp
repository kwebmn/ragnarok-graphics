#include "win/gpu/shaders.hpp"

namespace uagfx::gpu {

const char* shaderSource() {
    return R"HLSL(
cbuffer Params : register(b0) {
    float2 targetSize;
    uint2 stage0;
    uint2 stage1;
    uint alphaTest;
    uint fogEnable;
    float4 fogColor;
    uint colorKey;
    uint uiMode;
    float scale;
    float footprint;
    float4 sourceRect;
    float4 uiPixels;
    float4 sourceClamp;
    float sharpen;
    float twinX;  // uiMode 3 (stage 6): the twin texel origin O and the twin texture's size
    float twinY;
    float twinW;
    float twinH;
};
Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
SamplerState samp0 : register(s0);
SamplerState samp1 : register(s1);

struct VsIn {
    float4 pos : POSITION;       // x, y in D3D7 pixels, z in [0, 1], rhw
    float4 diffuse : COLOR0;
    float4 specular : COLOR1;    // alpha = vertex fog factor
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};
struct PsIn {
    float4 pos : SV_Position;
    float4 diffuse : COLOR0;
    float4 specular : COLOR1;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

// D3D7 puts pixel centres on integer coordinates, D3D12 on half-integers, and the target is `scale` times the
// game's size: a game x lands at x * scale + 0.5 (stage 3 spec §4; at scale 1 that is calibration.md §3).
PsIn draw_vs(VsIn i) {
    PsIn o;
    float w = i.pos.w > 0.0 ? 1.0 / i.pos.w : 1.0;
    float x = (i.pos.x * scale + 0.5) * 2.0 / targetSize.x - 1.0;
    float y = 1.0 - (i.pos.y * scale + 0.5) * 2.0 / targetSize.y;
    o.pos = float4(x * w, y * w, i.pos.z * w, w);
    o.diffuse = i.diffuse;
    o.specular = i.specular;
    o.uv0 = i.uv0;
    o.uv1 = i.uv1;
    return o;
}

float4 arg(uint a, float4 diffuse, float4 current, float4 tex, float4 specular) {
    uint sel = a & 15;
    float4 v = float4(1, 1, 1, 1);  // TFACTOR: the client never sets it, D3D7's default is white
    if (sel == 0) v = diffuse;
    else if (sel == 1) v = current;
    else if (sel == 2) v = tex;
    else if (sel == 4) v = specular;
    if (a & 16) v = 1.0 - v;   // COMPLEMENT
    if (a & 32) v = v.aaaa;    // ALPHAREPLICATE
    return v;
}

float4 combine(uint op, float4 a1, float4 a2, float4 diffuse, float4 tex, float4 current) {
    switch (op) {
        case 2: return a1;                                  // SELECTARG1
        case 3: return a2;                                  // SELECTARG2
        case 4: return a1 * a2;                             // MODULATE
        case 5: return saturate(a1 * a2 * 2);               // MODULATE2X
        case 6: return saturate(a1 * a2 * 4);               // MODULATE4X
        case 7: return saturate(a1 + a2);                   // ADD
        case 8: return saturate(a1 + a2 - 0.5);             // ADDSIGNED
        case 9: return saturate((a1 + a2 - 0.5) * 2);       // ADDSIGNED2X
        case 10: return saturate(a1 - a2);                  // SUBTRACT
        case 11: return saturate(a1 + a2 - a1 * a2);        // ADDSMOOTH
        case 12: return lerp(a2, a1, diffuse.a);            // BLENDDIFFUSEALPHA
        case 13: return lerp(a2, a1, tex.a);                // BLENDTEXTUREALPHA
        case 14: return a1;                                 // BLENDFACTORALPHA, factor alpha 1
        case 15: return saturate(a1 + a2 * (1 - tex.a));    // BLENDTEXTUREALPHAPM
        case 16: return lerp(a2, a1, current.a);            // BLENDCURRENTALPHA
        case 18: return saturate(a1 + a1.aaaa * a2);        // MODULATEALPHA_ADDCOLOR
        case 19: return saturate(a1 * a2 + a1.aaaa);        // MODULATECOLOR_ADDALPHA
        case 20: return saturate((1 - a1.aaaa) * a2 + a1);  // MODULATEINVALPHA_ADDCOLOR
        case 21: return saturate((1 - a1) * a2 + a1.aaaa);  // MODULATEINVCOLOR_ADDALPHA
    }
    return current;
}

float4 stage(uint2 s, float4 diffuse, float4 specular, float4 current, float4 tex) {
    uint colorOp = s.x & 255, colorArg1 = (s.x >> 8) & 255, colorArg2 = (s.x >> 16) & 255, alphaOp = s.x >> 24;
    uint alphaArg1 = s.y & 255, alphaArg2 = (s.y >> 8) & 255;
    float3 rgb = combine(colorOp, arg(colorArg1, diffuse, current, tex, specular),
                         arg(colorArg2, diffuse, current, tex, specular), diffuse, tex, current).rgb;
    float a = current.a;  // ALPHAOP DISABLE with COLOROP on: the alpha so far passes through
    if (alphaOp != 1)
        a = combine(alphaOp, arg(alphaArg1, diffuse, current, tex, specular),
                    arg(alphaArg2, diffuse, current, tex, specular), diffuse, tex, current).a;
    return float4(rgb, a);
}

bool passes(uint func, float a, float ref) {
    switch (func) {
        case 1: return false;
        case 2: return a < ref;
        case 3: return a == ref;
        case 4: return a <= ref;
        case 5: return a > ref;
        case 6: return a != ref;
        case 7: return a >= ref;
    }
    return true;
}

// Interface draws at k > 1 (stage 3 spec §6, review C1; core uiTaps does the same): target pixel p belongs to game
// pixel floor(p / k) and shows the game pixels it overlaps, each sampled exactly where Direct3D 7 sampled it (target
// g * k + 0.5) - never past the quad. Returns the two game pixels and the weight of the second.
float3 uiTaps(float p, float first, float last) {
    if (uiMode == 1) {  // sharp: k x k blocks, a one-pixel area blend where a game pixel edge crosses the pixel
        float g = clamp(floor(p / scale + 1e-4), first, last);
        float next = min(g + 1.0, last);
        return float3(g, next, next > g ? saturate(p + 1.0 - (g + 1.0) * scale) : 0.0);
    }
    float a = (p + 0.5) / scale - 0.5;  // smooth: bilinear between game pixel centres
    float g = floor(a);
    return float3(clamp(g, first, last), clamp(g + 1.0, first, last), a - g);
}

// Game pixel (gx, gy) as Direct3D 7 sampled it, premultiplied so a transparent neighbour does not darken an edge.
float4 uiTexel(float2 uv, float2 du, float2 dv, float2 pos, float gx, float gy) {
    float2 at = uv + du * (gx * scale + 0.5 - pos.x) + dv * (gy * scale + 0.5 - pos.y);
    float4 t = tex0.SampleGrad(samp0, at, du * scale, dv * scale);
    return float4(t.rgb * t.a, t.a);
}

float4 uiSample(float2 uv, float2 du, float2 dv, float2 pos) {
    float2 p = floor(pos);
    float3 x = uiTaps(p.x, uiPixels.x, uiPixels.z);
    float3 y = uiTaps(p.y, uiPixels.y, uiPixels.w);
    float4 top = lerp(uiTexel(uv, du, dv, pos, x.x, y.x), uiTexel(uv, du, dv, pos, x.y, y.x), x.z);
    float4 bottom = lerp(uiTexel(uv, du, dv, pos, x.x, y.y), uiTexel(uv, du, dv, pos, x.y, y.y), x.z);
    float4 c = lerp(top, bottom, y.z);
    return c.a > 0.0 ? float4(c.rgb / c.a, c.a) : float4(0.0, 0.0, 0.0, 0.0);
}

// Stage 6 (crisp text spec §3, §6): a window tile drawn from its twin. Target pixel p shows texel p - O of the
// tile's twin texture - one texel per pixel; past the texture the last column and row repeat. The twin is
// premultiplied; the game's blending wants the plain colour.
float4 twinTexel(float2 pos) {
    float2 t = clamp(floor(pos) - float2(twinX, twinY), float2(0.0, 0.0), float2(twinW, twinH) - 1.0);
    float4 c = tex0.Load(int3(int2(t), 0));
    return c.a > 0.0 ? float4(c.rgb / c.a, c.a) : float4(0.0, 0.0, 0.0, 0.0);
}

float4 draw_ps(PsIn i) : SV_Target {
    float2 uv0 = ((stage0.y >> 16) & 7) == 1 ? i.uv1 : i.uv0;
    float2 du = ddx(uv0);  // outside any branch; the interface path only uses explicit gradients
    float2 dv = ddy(uv0);
    float4 t0 = tex0.Sample(samp0, uv0);
    if (uiMode == 3)
        t0 = twinTexel(i.pos.xy);
    else if (uiMode != 0)
        t0 = uiSample(uv0, du, dv, i.pos.xy);
    float4 t1 = tex1.Sample(samp1, ((stage1.y >> 16) & 7) == 0 ? i.uv0 : i.uv1);
    if (colorKey != 0) {  // colour-keyed textures carry the key as alpha 0 (TextureLevels, Task 10)
        if (((stage0.y >> 24) & 1) != 0 && t0.a == 0) discard;
        if (((stage1.y >> 24) & 1) != 0 && t1.a == 0) discard;
    }
    float4 c = i.diffuse;  // stage 0 off: diffuse
    if ((stage0.x & 255) != 1) {
        c = stage(stage0, i.diffuse, i.specular, i.diffuse, t0);
        if ((stage1.x & 255) != 1) c = stage(stage1, i.diffuse, i.specular, c, t1);
    }
    if (fogEnable != 0) c.rgb = lerp(fogColor.rgb, c.rgb, i.specular.a);
    float alphaStep = fwidth(c.a);  // outside any branch
    if (((alphaTest >> 17) & 1) != 0) {
        // Stage 4, alpha to coverage: the test's edge (alpha >= ref) as coverage over about one pixel, so the edge
        // stays where the test put it and is smooth.
        float ref = float((alphaTest >> 8) & 255) / 255.0;
        c.a = saturate((c.a - ref) / max(alphaStep, 1e-4) + 0.5);
    } else if (((alphaTest >> 16) & 1) != 0 && !passes(alphaTest & 255, round(c.a * 255.0), (alphaTest >> 8) & 255)) {
        discard;
    }
    return c;
}

struct PresentOut {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

PresentOut present_vs(uint id : SV_VertexID) {
    PresentOut o;
    float2 t = float2((id << 1) & 2, id & 2);  // a triangle covering the viewport
    o.pos = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = lerp(sourceRect.xy, sourceRect.zw, t);
    return o;
}

// Copies part of a texture: one bilinear sample when it is not shrunk, else the average of n x n samples over the
// destination pixel's footprint (supersampling; the target read back at the game's size).
// One destination pixel of a copy: a bilinear sample when the source is not shrunk, else the average of n x n
// samples over the pixel's footprint (supersampling; the target read back at the game's size). Every tap stays
// inside the source rectangle (stage 3 review M6).
float3 fetch(float2 uv, float2 dx, float2 dy) {
    if (footprint <= 1.0001) return tex0.SampleGrad(samp0, clamp(uv, sourceClamp.xy, sourceClamp.zw), dx, dy).rgb;
    int n = min((int)ceil(footprint), 4);
    float3 sum = 0;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            float2 o = (float2(x, y) + 0.5) / n - 0.5;
            sum += tex0.SampleGrad(samp0, clamp(uv + o.x * dx + o.y * dy, sourceClamp.xy, sourceClamp.zw), dx, dy).rgb;
        }
    return sum / (n * n);
}

// Contrast adaptive sharpening (after AMD FidelityFX CAS, the 5-tap cross; core casSharpen does the same): more local
// contrast where there is detail, flat and linear areas as they are, never beyond the neighbours - no halos.
float3 cas(float3 c, float3 n, float3 s, float3 w, float3 e) {
    float3 mn = min(c, min(min(n, s), min(w, e)));
    float3 mx = max(c, max(max(n, s), max(w, e)));
    float3 amp = sqrt(saturate(min(mn, 1.0 - mx) / max(mx, 1e-5)));
    float3 wgt = amp * (-1.0 / lerp(8.0, 5.0, sharpen));
    return clamp((c + wgt * (n + s + w + e)) / (1.0 + 4.0 * wgt), mn, mx);
}

float4 present_ps(PresentOut i) : SV_Target {
    float2 dx = ddx(i.uv);  // one destination pixel in the source; outside any branch
    float2 dy = ddy(i.uv);
    float3 c = fetch(i.uv, dx, dy);
    if (sharpen >= 0.0)
        c = cas(c, fetch(i.uv - dy, dx, dy), fetch(i.uv + dy, dx, dy), fetch(i.uv - dx, dx, dy), fetch(i.uv + dx, dx, dy));
    return float4(c, 1);
}
)HLSL";
}

}  // namespace uagfx::gpu
