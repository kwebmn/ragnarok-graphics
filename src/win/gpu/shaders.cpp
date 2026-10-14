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
    float jitterX;  // part 3 (DLSS): how far the world's geometry moves this frame, in target pixels
    float jitterY;
    float r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11;  // motion only: this view -> the previous one, 4 x 3
    float f0, f1, cx, cy, nearZ, farZ;  // motion only: the game's projection (core/game_camera.hpp)
};
Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
// 0.19, modern water: the picture and its depth as they were before the frame's first water (water_ps), and the
// depth with several samples it is copied from (depth_copy_ms_ps). One table of three (kRootScene).
Texture2D sceneColor : register(t2);
Texture2D<float> sceneDepth : register(t3);
Texture2DMS<float> depthMs : register(t4);
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
    float x = (i.pos.x * scale + 0.5 + jitterX) * 2.0 / targetSize.x - 1.0;
    float y = 1.0 - (i.pos.y * scale + 0.5 + jitterY) * 2.0 / targetSize.y;
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

float4 shade(PsIn i) {
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
    // Part 3 (DLSS): an opaque interface draw covers the world wholly in the layer, whatever alpha it carries.
    if (((alphaTest >> 18) & 1) != 0) c.a = 1.0;
    return c;
}

float4 draw_ps(PsIn i) : SV_Target { return shade(i); }

// 0.18, the glow of effects: world draws also write the light they add into a second target, blended the same way.
// A draw that adds light (alphaTest bit 19; also one that multiplies, which scales the light under it alike) writes
// its colour; any other writes no colour with its alpha, so whatever covers the light takes it away as it hides it.
struct GlowOut {
    float4 c : SV_Target0;
    float4 g : SV_Target1;
};

GlowOut draw_glow_ps(PsIn i) {
    GlowOut o;
    o.c = shade(i);
    o.g = ((alphaTest >> 19) & 1) != 0 ? o.c : float4(0.0, 0.0, 0.0, o.c.a);
    return o;
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

// Part 3 (DLSS): where each world pixel was in the previous frame, from the game's camera alone (core/game_camera:
// its depth turned back into a point of this frame's view, then shown with the previous view). In target pixels, the
// jitter taken out: the pixel shows the game point x with x * scale + 0.5 + jitter = pixel centre.
float2 motion_ps(PresentOut i) : SV_Target {
    int2 p = int2(i.pos.xy);
    float sz = tex0.Load(int3(p, 0)).r;
    float gx = (float(p.x) - jitterX) / scale;
    float gy = (float(p.y) - jitterY) / scale;
    float zc = farZ * nearZ / (farZ - sz * (farZ - nearZ));
    float xc = (gx - cx) * zc / f0;
    float yc = (gy - cy) * zc / f1;
    float vx = xc * r0 + yc * r3 + zc * r6 + r9;
    float vy = xc * r1 + yc * r4 + zc * r7 + r10;
    float vz = xc * r2 + yc * r5 + zc * r8 + r11;
    if (vz < 1e-4) return float2(0, 0);  // behind the previous camera
    return float2(vx / vz * f0 + cx - gx, vy / vz * f1 + cy - gy) * scale;
}

// Part 3: a copy that keeps alpha - the premultiplied interface layer over the picture (present_ps writes alpha 1,
// which would black the world out under the layer), and the layer for frame generation.
float4 copy_ps(PresentOut i) : SV_Target {
    float2 dx = ddx(i.uv);
    float2 dy = ddy(i.uv);
    return tex0.SampleGrad(samp0, clamp(i.uv, sourceClamp.xy, sourceClamp.zw), dx, dy);
}

float4 present_ps(PresentOut i) : SV_Target {
    float2 dx = ddx(i.uv);  // one destination pixel in the source; outside any branch
    float2 dy = ddy(i.uv);
    float3 c = fetch(i.uv, dx, dy);
    if (sharpen >= 0.0)
        c = cas(c, fetch(i.uv - dy, dx, dy), fetch(i.uv + dy, dx, dy), fetch(i.uv - dx, dx, dy), fetch(i.uv + dx, dx, dy));
    return float4(c, 1);
}

// 0.18, the glow: the light of the world's additive draws shrunk level by level, blurred on the way back up and
// added to the picture (Jimenez, "Next generation post processing in Call of Duty: Advanced Warfare", 2014). For
// these passes sourceClamp.xy is a texel of the source, footprint the glow's strength, sharpen 1 for the first level.
float3 glowTap(float2 uv) { return min(tex0.SampleLevel(samp0, uv, 0).rgb, 16.0); }

float glowWeight(float3 c) { return 1.0 / (1.0 + max(c.r, max(c.g, c.b))); }

// Thirteen taps over a 4 x 4 texel area; the first level weighs its five boxes by brightness (Karis), so that a lone
// spark does not flicker as it moves across the pixels.
float4 glow_down_ps(PresentOut i) : SV_Target {
    float2 t = sourceClamp.xy;
    float2 uv = i.uv;
    float3 a = glowTap(uv + t * float2(-2, -2)), b = glowTap(uv + t * float2(0, -2));
    float3 c = glowTap(uv + t * float2(2, -2));
    float3 d = glowTap(uv + t * float2(-1, -1)), e = glowTap(uv + t * float2(1, -1));
    float3 f = glowTap(uv + t * float2(-2, 0)), g = glowTap(uv), h = glowTap(uv + t * float2(2, 0));
    float3 j = glowTap(uv + t * float2(-1, 1)), k = glowTap(uv + t * float2(1, 1));
    float3 l = glowTap(uv + t * float2(-2, 2)), m = glowTap(uv + t * float2(0, 2)), n = glowTap(uv + t * float2(2, 2));
    float3 inner = (d + e + j + k) * 0.25;
    float3 b1 = (a + b + f + g) * 0.25, b2 = (b + c + g + h) * 0.25, b3 = (f + g + l + m) * 0.25;
    float3 b4 = (g + h + m + n) * 0.25;
    if (sharpen > 0.5) {
        float w0 = glowWeight(inner) * 0.5, w1 = glowWeight(b1) * 0.125, w2 = glowWeight(b2) * 0.125;
        float w3 = glowWeight(b3) * 0.125, w4 = glowWeight(b4) * 0.125;
        return float4((inner * w0 + b1 * w1 + b2 * w2 + b3 * w3 + b4 * w4) / (w0 + w1 + w2 + w3 + w4), 1.0);
    }
    return float4(inner * 0.5 + (b1 + b2 + b3 + b4) * 0.125, 1.0);
}

// A 3 x 3 tent over the smaller level, one of its texels apart.
float3 glowTent(float2 uv) {
    float2 t = sourceClamp.xy;
    float3 s = glowTap(uv + t * float2(-1, -1)) + glowTap(uv + t * float2(1, -1)) + glowTap(uv + t * float2(-1, 1)) +
               glowTap(uv + t * float2(1, 1));
    s += 2.0 * (glowTap(uv + t * float2(0, -1)) + glowTap(uv + t * float2(-1, 0)) + glowTap(uv + t * float2(1, 0)) +
                glowTap(uv + t * float2(0, 1)));
    s += 4.0 * glowTap(uv);
    return s / 16.0;
}

// Into the larger level, blended with what it holds by the blend factor: how far the glow spreads.
float4 glow_up_ps(PresentOut i) : SV_Target { return float4(glowTent(i.uv), 1.0); }

// Onto the picture, added.
float4 glow_add_ps(PresentOut i) : SV_Target { return float4(glowTent(i.uv) * footprint, 1.0); }

// 0.19, the depth the frame has so far, one sample per pixel, for the water: from a target of one sample (t3) or of
// several (t4, the first sample).
float depth_copy_ps(PresentOut i) : SV_Target { return sceneDepth.Load(int3(int2(i.pos.xy), 0)); }

float depth_copy_ms_ps(PresentOut i) : SV_Target { return depthMs.Load(int2(i.pos.xy), 0); }

// 0.19, modern water (core/water.hpp: the cells of a map's water). Its draws carry the game's camera: r0-r11 its
// matrix from view to world (the rows: the view's x, y and z axes in the world, then its place), f0-farZ its
// projection; sharpen is the time in seconds, footprint the waves, twinX the reflections, twinY the clarity (0-1).
// uaRO's world has its up along -y; a cell is ten units.
struct WaterIn {
    float4 pos : SV_Position;
    float4 diffuse : COLOR0;
    float2 uv : TEXCOORD0;
    float3 world : TEXCOORD1;
    float3 view : TEXCOORD2;
};

// The game's projection backwards (ProjectVertex: x / z * f0 + cx, 1 / z its RHW), then into the world.
WaterIn water_vs(VsIn i) {
    WaterIn o;
    PsIn d = draw_vs(i);
    o.pos = d.pos;
    o.diffuse = i.diffuse;
    o.uv = i.uv0;
    float z = i.pos.w > 0.0 ? 1.0 / i.pos.w : nearZ;
    o.view = float3((i.pos.x - cx) * z / f0, (i.pos.y - cy) * z / f1, z);
    o.world = o.view.x * float3(r0, r1, r2) + o.view.y * float3(r3, r4, r5) + o.view.z * float3(r6, r7, r8) +
              float3(r9, r10, r11);
    return o;
}

float viewDepth(float d) { return farZ * nearZ / (farZ - d * (farZ - nearZ)); }

// The slope of a few travelling waves at a point of the water (world units): long swells and short ripples, each
// its own way, so that the pattern never repeats on screen.
float2 waveSlope(float2 p, float t) {
    const float2 dir[5] = {float2(0.96, 0.28), float2(-0.51, 0.86), float2(0.2, -0.98), float2(-0.93, -0.37),
                           float2(0.66, 0.75)};
    const float len[5] = {23.0, 13.0, 7.3, 4.1, 2.3};
    const float speed[5] = {2.1, 1.6, 1.2, 0.9, 0.7};
    const float amp[5] = {0.30, 0.18, 0.08, 0.04, 0.02};
    float2 g = float2(0.0, 0.0);
    [unroll] for (int k = 0; k < 5; ++k) {
        float w = 6.2831853 / len[k];
        g += dir[k] * (amp[k] * w * cos(w * (dot(dir[k], p) - speed[k] * t)));
    }
    return g;
}

// 0.21, light on the bed: where the moving surface focuses the sun (its height field concave), bright and narrow.
float causticAt(float2 p, float t) {
    const float2 dir[4] = {float2(0.83, 0.56), float2(-0.37, 0.93), float2(-0.95, -0.31), float2(0.45, -0.89)};
    const float len[4] = {8.7, 6.1, 4.4, 3.2};
    const float speed[4] = {1.3, 1.1, 0.9, 0.8};
    float focus = 0.0;
    [unroll] for (int k = 0; k < 4; ++k) {
        float w = 6.2831853 / len[k];
        focus += sin(w * (dot(dir[k], p) - speed[k] * t));
    }
    return pow(saturate(focus * 0.25 + 0.5), 7.0) * 2.5;
}

float hash12(float2 p) {
    float3 q = frac(float3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return frac((q.x + q.y) * q.z);
}

float valueNoise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float bottom = lerp(hash12(i), hash12(i + float2(1, 0)), u.x);
    float top = lerp(hash12(i + float2(0, 1)), hash12(i + float2(1, 1)), u.x);
    return lerp(bottom, top, u.y);
}

// A target pixel for a point of the game's view space; false when it is off the target or behind the camera.
bool viewToPixel(float3 q, out float2 tp) {
    tp = float2(0.0, 0.0);
    if (q.z <= nearZ) return false;
    tp = float2(q.x / q.z * f0 + cx, q.y / q.z * f1 + cy) * scale + 0.5 + float2(jitterX, jitterY);
    return all(tp >= 0.0) && all(tp < targetSize);
}

// What the reflected ray meets first in the picture before the water (screen-space reflection), and how sure that
// is: 0 for the sky (the ray left the picture or met nothing).
float3 waterReflection(float3 p, float3 r, out float weight) {
    weight = 0.0;
    float dist = 2.0;
    float2 tp;
    [loop] for (int k = 0; k < 28; ++k) {
        float3 q = p + r * dist;
        if (!viewToPixel(q, tp)) break;
        float behind = q.z - viewDepth(sceneDepth.Load(int3(int2(tp), 0)));
        if (behind > 0.0 && behind < 3.0 + dist * 0.12) {
            float2 edge = min(tp, targetSize - tp) / (0.08 * targetSize);
            weight = saturate(min(edge.x, edge.y)) * saturate(1.5 - dist / 400.0);
            return sceneColor.Load(int3(int2(tp), 0)).rgb;
        }
        dist *= 1.22;
    }
    return float3(0.0, 0.0, 0.0);
}

float4 water(WaterIn i) {
    int2 px = int2(i.pos.xy);
    float t = sharpen;
    float waves = footprint;
    // 0.22.2: the world's axes turned, for the noise: no rows along them.
    float2 fp = float2(dot(i.world.xz, float2(0.8, 0.6)), dot(i.world.xz, float2(-0.6, 0.8)));
    float2 slope = waveSlope(i.world.xz, t);
    // Ripples of noise on the swells, so that their glints do not line up.
    float2 rq = fp * 0.55 + t * float2(0.21, 0.13);
    float n0 = valueNoise(rq);
    float2 g = float2(valueNoise(rq + float2(0.15, 0.0)) - n0, valueNoise(rq + float2(0.0, 0.15)) - n0) / 0.15;
    slope += float2(0.8 * g.x - 0.6 * g.y, 0.6 * g.x + 0.8 * g.y) * 0.05;
    slope *= 0.15 + 1.6 * waves;
    float3 n = normalize(float3(-slope.x, -1.0, -slope.y));  // up is -y
    float3 eye = float3(r9, r10, r11);
    float3 toEye = normalize(eye - i.world);
    float ndv = saturate(dot(n, toEye));
    float3 nv = float3(dot(n, float3(r0, r1, r2)), dot(n, float3(r3, r4, r5)), dot(n, float3(r6, r7, r8)));
    float waterZ = i.view.z;
    float2 dz = float2(ddx(waterZ), ddy(waterZ));  // the water's depth from pixel to pixel (outside any branch)
    float thick = max(viewDepth(sceneDepth.Load(int3(px, 0))) - waterZ, 0.0);
    // The bed through the waves - shifted by how far the waves tilt the water from flat (its up in the view:
    // -y of the world); where the shifted pixel is in front of the water, straight through.
    float2 tilt = nv.xy + float2(r1, r4);
    float2 shift = tilt * (36.0 * scale) * saturate(thick / 6.0) * (0.2 + waves);
    int2 rp = clamp(int2(i.pos.xy + shift), int2(0, 0), int2(targetSize) - 1);
    float bedZ = viewDepth(sceneDepth.Load(int3(rp, 0)));
    if (bedZ < waterZ) {
        rp = px;
        bedZ = waterZ + thick;
    }
    float3 bed = sceneColor.Load(int3(rp, 0)).rgb;
    float path = max(bedZ - waterZ, 0.0);
    // 0.21: the sun's light focused by the waves on the bed, where it is shallow and light.
    float3 ray = normalize(i.world - eye);
    float2 bedXz = i.world.xz + ray.xz * (path / max(dot(ray, float3(r6, r7, r8)), 0.2));
    float shallow = exp(-path * lerp(0.3, 0.05, twinY));
    bed *= 1.0 + causticAt(bedXz, t) * shallow * waves;
    // The game's own water picture gives the water its colour.
    float3 tint = tex0.Sample(samp0, i.uv + slope * 0.04).rgb * i.diffuse.rgb;
    float absorb = 1.0 - exp(-path * lerp(0.22, 0.012, twinY));
    float3 body = lerp(bed * lerp(float3(1.0, 1.0, 1.0), tint * 1.3, 0.3), tint * 0.45, absorb);
    body = lerp(body, tint, 0.1);
    // The place's light, from the bed's brightness: night, dungeons.
    float light = clamp(dot(bed, float3(0.299, 0.587, 0.114)) * 2.2, 0.25, 1.0);
    float3 sky = (fogEnable != 0 ? fogColor.rgb : float3(0.62, 0.74, 0.88)) * light;
    float hit;
    float3 seen = waterReflection(i.view, reflect(normalize(i.view), nv), hit);
    float3 reflection = lerp(sky, seen, hit);
    float f0r = lerp(0.02, 0.3, twinX);
    float fresnel = f0r + (1.0 - f0r) * pow(1.0 - ndv, 5.0);
    float3 c = lerp(body, reflection, fresnel);
    // The sun's glints.
    float3 h = normalize(normalize(float3(-0.45, -0.8, 0.4)) + toEye);
    float ndh = saturate(dot(n, h));
    float sun = fogEnable != 0 ? 0.25 : 1.0;  // in a fog (dungeons, caves) hardly any sun
    c += (pow(ndh, 600.0) * 3.0 + pow(ndh, 60.0) * 0.12) * light * (0.25 + 0.75 * waves) * sun;
    // 0.22.2: foam where the water meets the shore - near a pixel whose ground stands at or above the water there
    // (the water's own depth carried over by its slope on the screen), not over the whole of shallow water -
    // broken by noise that drifts with the waves.
    float shore = 0.0;
    float reach = 5.0 * scale;
    [unroll] for (int k = 0; k < 4; ++k) {
        float2 o = k == 0 ? float2(reach, 0.0) : k == 1 ? float2(-reach, 0.0) : k == 2 ? float2(0.0, reach)
                                                                                    : float2(0.0, -reach);
        int2 q = clamp(int2(i.pos.xy + o), int2(0, 0), int2(targetSize) - 1);
        float above = viewDepth(sceneDepth.Load(int3(q, 0))) - (waterZ + dot(o, dz));
        shore = max(shore, saturate(1.0 - above / 0.6));
    }
    shore *= saturate(1.0 - thick / 3.0);
    float froth = smoothstep(0.45, 0.8, valueNoise(fp * 0.4 + t * 0.25) * 0.6 +
                                            valueNoise(fp.yx * 1.1 - t * 0.45) * 0.4);
    c = lerp(c, float3(0.92, 0.95, 0.97) * light, shore * froth * 0.6);
    return float4(c, saturate(thick / 0.8));  // the shore line fades in, no hard edge
}

// 0.20, soft shadows where surfaces meet (screen-space ambient occlusion): on the map and its models once they are
// drawn, before the characters and the effects. f0-farZ are the game's projection, scale and jitter the world's;
// uiPixels the world's size and the shadows' (half of it); footprint the radius in world units, sharpen the
// strength (0-1); sourceRect.xy the blur's direction.
float3 viewAt(int2 wp) {  // the point of the view the world pixel wp shows (its centre: game (wp - jitter) / scale)
    float z = viewDepth(sceneDepth.Load(int3(wp, 0)));
    float2 g = (float2(wp) - float2(jitterX, jitterY)) / scale;
    return float3((g.x - cx) * z / f0, (g.y - cy) * z / f1, z);
}

float ao_ps(PresentOut i) : SV_Target {
    const float3 kernel[12] = {
        float3(-0.0933, 0.0078, 0.0503), float3(-0.1224, 0.0021, 0.0252), float3(-0.0230, -0.1494, 0.0394),
        float3(-0.0421, 0.1821, 0.0711), float3(0.0986, -0.0906, 0.2185), float3(0.2057, -0.0826, 0.2377),
        float3(0.1547, -0.0847, 0.3660), float3(0.3701, 0.2501, 0.2247), float3(0.0994, 0.0337, 0.5971),
        float3(-0.7008, -0.0222, 0.1846), float3(0.3223, 0.5069, 0.6102), float3(0.3189, 0.2700, 0.9085)};
    int2 wp = int2(i.pos.xy) * 2;
    int2 last = int2(uiPixels.xy) - 1;
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) return 1.0;  // nothing drawn there
    float3 p = viewAt(wp);
    // The normal from the neighbours, each on the side nearer in depth, so that an edge does not bend it.
    float3 l = viewAt(max(wp - int2(2, 0), int2(0, 0))), r = viewAt(min(wp + int2(2, 0), last));
    float3 u = viewAt(max(wp - int2(0, 2), int2(0, 0))), d = viewAt(min(wp + int2(0, 2), last));
    float3 dx = abs(r.z - p.z) < abs(p.z - l.z) ? r - p : p - l;
    float3 dy = abs(d.z - p.z) < abs(p.z - u.z) ? d - p : p - u;
    float3 n = normalize(cross(dy, dx));
    if (dot(n, p) > 0.0) n = -n;  // towards the camera
    // The kernel turned around the normal by an angle of its own for each pixel; the blur evens the noise out.
    float a = hash12(float2(wp)) * 6.2831853;
    float3 t0 = normalize(abs(n.x) < 0.9 ? cross(n, float3(1, 0, 0)) : cross(n, float3(0, 1, 0)));
    float3 t1 = cross(n, t0);
    float3 tx = t0 * cos(a) + t1 * sin(a);
    float3 ty = cross(n, tx);
    float radius = footprint;
    float occlusion = 0.0;
    [unroll] for (int k = 0; k < 12; ++k) {
        float3 q = p + (tx * kernel[k].x + ty * kernel[k].y + n * kernel[k].z) * radius;
        if (q.z <= nearZ) continue;
        float2 t = float2(q.x / q.z * f0 + cx, q.y / q.z * f1 + cy) * scale + 0.5 + float2(jitterX, jitterY);
        if (any(t < 0.0) || any(t >= uiPixels.xy)) continue;
        float zs = viewDepth(sceneDepth.Load(int3(int2(t), 0)));
        if (zs < q.z - 0.02 * radius) occlusion += saturate(radius / max(abs(p.z - zs), 1e-3));  // near ones only
    }
    return 1.0 - occlusion / 12.0;
}

// Nine taps along one direction, weighed by distance and by how close in depth they are: the shadows stay on their
// side of an edge.
float ao_blur_ps(PresentOut i) : SV_Target {
    int2 p = int2(i.pos.xy);
    int2 dir = int2(sourceRect.xy);
    int2 last = int2(uiPixels.zw) - 1;
    float z0 = viewDepth(sceneDepth.Load(int3(min(p * 2, int2(uiPixels.xy) - 1), 0)));
    float sum = 0.0, weights = 0.0;
    [unroll] for (int k = -4; k <= 4; ++k) {
        int2 q = clamp(p + dir * k, int2(0, 0), last);
        float zq = viewDepth(sceneDepth.Load(int3(min(q * 2, int2(uiPixels.xy) - 1), 0)));
        float w = exp(-float(k * k) / 8.0) * saturate(1.0 - abs(zq - z0) / (0.04 * z0 + 1.0));
        sum += tex0.Load(int3(q, 0)).r * w;
        weights += w;
    }
    return sum / max(weights, 1e-4);
}

// Onto the picture: multiplied (blend ZERO, SRCCOLOR).
float4 ao_apply_ps(PresentOut i) : SV_Target {
    float ao = tex0.SampleLevel(samp0, i.pos.xy * 0.5 / uiPixels.zw, 0).r;
    float f = lerp(1.0, ao, sharpen);
    return float4(f, f, f, 1.0);
}

// 0.21, the picture's colours (grade): from its copy (t2) into the picture, before the interface. sharpen the
// saturation (0-2), footprint the contrast and twin.x the brightness (0.5-1.5), twin.y the warmth (-1 colder to 1
// warmer), twin.z the vignette (0-1); uiPixels.xy the world's size.
float4 grade_ps(PresentOut i) : SV_Target {
    float3 c = sceneColor.Load(int3(int2(i.pos.xy), 0)).rgb;
    c = pow(max(c, 0.0), 1.0 / twinX);  // brighter middle tones, the white stays white
    float k = footprint;
    c = k >= 1.0 ? lerp(c, c * c * (3.0 - 2.0 * c), saturate((k - 1.0) * 2.0))  // an S curve: nothing clips
                 : lerp(float3(0.5, 0.5, 0.5), c, k);
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(float3(l, l, l), c, sharpen);
    c *= float3(1.0 + 0.1 * twinY, 1.0 + 0.02 * twinY, 1.0 - 0.1 * twinY);
    float2 q = (i.pos.xy / uiPixels.xy - 0.5) * float2(uiPixels.x / uiPixels.y, 1.0);
    c *= 1.0 - twinW * smoothstep(0.35, 1.0, length(q) * 1.15) * 0.8;
    return float4(saturate(c), 1.0);
}

// 0.22, depth of field (tilt-shift): sharp around what the camera looks at - the character in the middle of the
// screen - softer with the distance in depth from it, near and far, as if the map were a model. uiPixels the
// world's size and its blurred copy's (a quarter); footprint where the blur starts (world units of depth from the
// focus), sharpen its strength (0-1); sourceRect.xy the blur's direction.
float4 dof_down_ps(PresentOut i) : SV_Target {  // four by four of the picture: four bilinear taps
    float2 c = i.pos.xy * 4.0;
    float2 t = 1.0 / uiPixels.xy;
    float3 s = sceneColor.SampleLevel(samp0, (c + float2(-1.0, -1.0)) * t, 0).rgb +
               sceneColor.SampleLevel(samp0, (c + float2(1.0, -1.0)) * t, 0).rgb +
               sceneColor.SampleLevel(samp0, (c + float2(-1.0, 1.0)) * t, 0).rgb +
               sceneColor.SampleLevel(samp0, (c + float2(1.0, 1.0)) * t, 0).rgb;
    return float4(s * 0.25, 1.0);
}

float4 dof_blur_ps(PresentOut i) : SV_Target {  // nine taps along one direction
    int2 p = int2(i.pos.xy);
    int2 dir = int2(sourceRect.xy);
    int2 last = int2(uiPixels.zw) - 1;
    float3 sum = float3(0.0, 0.0, 0.0);
    float weights = 0.0;
    [unroll] for (int k = -4; k <= 4; ++k) {
        float w = exp(-float(k * k) / 12.5);
        sum += tex0.Load(int3(clamp(p + dir * k, int2(0, 0), last), 0)).rgb * w;
        weights += w;
    }
    return float4(sum / weights, 1.0);
}

float4 dof_apply_ps(PresentOut i) : SV_Target {
    int2 p = int2(i.pos.xy);
    float3 sharp = sceneColor.Load(int3(p, 0)).rgb;
    float3 soft = tex0.SampleLevel(samp0, i.pos.xy * 0.25 / uiPixels.zw, 0).rgb;
    // The focus: the depth in the middle of the screen, a few pixels around it averaged.
    int2 mid = int2(uiPixels.xy * 0.5);
    int r = int(4.0 * scale);
    float focus = (viewDepth(sceneDepth.Load(int3(mid, 0))) + viewDepth(sceneDepth.Load(int3(mid + int2(r, 0), 0))) +
                   viewDepth(sceneDepth.Load(int3(mid - int2(r, 0), 0))) +
                   viewDepth(sceneDepth.Load(int3(mid + int2(0, r), 0))) +
                   viewDepth(sceneDepth.Load(int3(mid - int2(0, r), 0)))) * 0.2;
    float z = viewDepth(sceneDepth.Load(int3(p, 0)));
    float blur = smoothstep(footprint, footprint * 3.0, abs(z - focus)) * sharpen;
    return float4(lerp(sharp, soft, blur), 1.0);
}

float4 water_ps(WaterIn i) : SV_Target { return water(i); }

GlowOut water_glow_ps(WaterIn i) {
    GlowOut o;
    o.c = water(i);
    o.g = float4(0.0, 0.0, 0.0, o.c.a);  // the water covers the light under it
    return o;
}
)HLSL";
}

}  // namespace uagfx::gpu
