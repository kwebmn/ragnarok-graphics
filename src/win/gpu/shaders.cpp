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
#ifdef UAGFX_FIXED
// 2.5.16: the draw shader made for one state of the game's draws (core/shader_variants.hpp): its Direct3D 7 stages,
// tests, fog and interface mode as constants - the compiler folds the branches that choose among them away (an old
// Radeon's VLIW cores spend on each branch, and the one shader for every state has many). The buffer stays as it is.
static const uint2 fixedStage0 = uint2(UAGFX_S0X, UAGFX_S0Y);
static const uint2 fixedStage1 = uint2(UAGFX_S1X, UAGFX_S1Y);
static const uint fixedAlphaTest = UAGFX_ALPHA;
static const uint fixedFog = UAGFX_FOG;
static const uint fixedColorKey = UAGFX_KEY;
static const uint fixedUiMode = UAGFX_UI;
#define stage0 fixedStage0
#define stage1 fixedStage1
#define alphaTest fixedAlphaTest
#define fogEnable fixedFog
#define colorKey fixedColorKey
#define uiMode fixedUiMode
#endif
Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
// 0.19, modern water: the picture and its depth as they were before the frame's first water (water_ps), and the
// depth with several samples it is copied from (depth_copy_ms_ps). One table of three (kRootScene).
Texture2D sceneColor : register(t2);
Texture2D<float> sceneDepth : register(t3);
Texture2DMS<float> depthMs : register(t4);
// 1.16: the world mirrored under the water (mirror.cpp), for water_ps - the table's fourth
Texture2D mirrorTex : register(t5);
SamplerState samp0 : register(s0);
SamplerState samp1 : register(s1);
// The distance from the camera of a depth the game's projection gave (water, the effects).
float viewDepth(float d) { return farZ * nearZ / (farZ - d * (farZ - nearZ)); }
// 3.0.4-beta.20: a depth of the own world past the game's far plane as own_ground::depthOf squeezes it (the game's own
// up to farZ - kSqueeze, 10; the rest below 1) back into the view - the far water's bed (water()); the game's as above
float bedDepth(float d) {
    float split = farZ - 10.0;
    float at = (farZ - farZ * nearZ / split) / (farZ - nearZ);
    if (d <= at) return viewDepth(d);
    return split / max(1.0 - (d - at) / (1.0 - at), 0.0001);
}

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
    float gx = i.pos.x;
    // 0.64: the leaves the wind sways (core/foliage.cpp's sway) - a world draw of leaves: twinX the swing (game
    // pixels), twinY the draw's bottom, twinW its height, twinH the time; the top swings, the trunk stays.
    if (uiMode == 0 && twinX > 0.0) {
        float up = saturate((twinY - i.pos.y) / max(twinW, 1.0));
        float wave = sin(twinH * 1.6 + i.pos.x * 0.013 + i.pos.y * 0.007) * 0.7 +
                     sin(twinH * 2.9 + i.pos.x * 0.031) * 0.3;
        gx += twinX * up * up * wave;
    }
    float gy = i.pos.y;
    float gz = i.pos.z;
    float x = (gx * scale + 0.5 + jitterX) * 2.0 / targetSize.x - 1.0;
    float y = 1.0 - (gy * scale + 0.5 + jitterY) * 2.0 / targetSize.y;
    o.pos = float4(x * w, y * w, gz * w, w);
    o.diffuse = i.diffuse;
    o.specular = i.specular;
    o.uv0 = i.uv0;
    o.uv1 = i.uv1;
    return o;
}

// 3.0.5-beta.13 (the user: «Швы на видеокарте»): a piece of a strip across the ground's seams, kept in the card's
// memory in the world's places (core/seams.hpp Store) - the camera put on it here as applySeams put it on the
// processor, a little towards the camera, then as draw_vs; its fog from its cell's four corners this frame by each
// one's share kept with the vertex. An instance a piece: its slot's first vertex and index, its indices (the
// instance's corners past them draw nothing) and the corners' fog. Direct3D 11's (its slots t6, t7 of the vertex
// shader's own).
Buffer<uint4> seamVertices : register(t6);  // three a vertex (core/seams.hpp CardVertex)
Buffer<uint> seamIndices : register(t7);
struct SeamIn {
    uint corner : SV_VertexID;
    uint4 piece : TEXCOORD0;
};
float4 colourOf(uint c) {  // a Direct3D 7 colour (A8R8G8B8) as a vertex's COLOR reads it
    return float4(float((c >> 16) & 255), float((c >> 8) & 255), float(c & 255), float(c >> 24)) / 255.0;
}
PsIn seam_vs(SeamIn i) {
    PsIn o = (PsIn)0;
    o.pos = float4(2.0, 2.0, 0.5, 1.0);  // past the target's right
    if (i.corner >= i.piece.z) return o;
    uint at = (i.piece.x + seamIndices.Load(int(i.piece.y + i.corner))) * 3;
    uint4 a = seamVertices.Load(int(at));
    uint4 b = seamVertices.Load(int(at + 1));
    uint4 c = seamVertices.Load(int(at + 2));
    // into the view: the world less the camera's place, through the inverse of its ways (r0-r8, the view's in the
    // world, its rows) - the inverse's columns the rows' crossings over the determinant
    float3 ra = float3(r0, r1, r2), rb = float3(r3, r4, r5), rc = float3(r6, r7, r8);
    float3 w = asfloat(a.xyz) - float3(r9, r10, r11);
    float3 v = float3(dot(w, cross(rb, rc)), dot(w, cross(rc, ra)), dot(w, cross(ra, rb))) / dot(ra, cross(rb, rc));
    float zv = max(v.z, nearZ + 0.01);
    float gx = v.x / zv * f0 + cx;
    float gy = v.y / zv * f1 + cy;
    float zd = max(zv - 0.15, nearZ + 0.005);  // on its cell's surface, not under it
    float gz = farZ * (1.0 - nearZ / zd) / (farZ - nearZ);
    float x = (gx * scale + 0.5 + jitterX) * 2.0 / targetSize.x - 1.0;
    float y = 1.0 - (gy * scale + 0.5 + jitterY) * 2.0 / targetSize.y;
    o.pos = float4(x * zv, y * zv, gz * zv, zv);
    o.diffuse = colourOf(a.w);
    o.specular = colourOf(c.x);
    float4 share = float4(float(c.y & 65535), float(c.y >> 16), float(c.z & 65535), float(c.z >> 16)) / 65535.0;
    uint f = i.piece.w;
    float4 corners = float4(float(f & 255), float((f >> 8) & 255), float((f >> 16) & 255), float(f >> 24));
    o.specular.a = floor(dot(share, corners) + 0.5) / 255.0;  // whole steps of 255, as the game's
    o.uv0 = asfloat(b.xy);
    o.uv1 = asfloat(b.zw);
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

// 0.22.6, the map's baked light (the ground's lightmap): 8x8 texel cells in 256x256 atlases, the tile showing the
// middle 6x6 and the border texels its neighbours' (0x4e32e5). RO bakes the torches' coloured light (RGB) as sparse
// dots and the cells are coarse: magnified they show as blotches with edges. Blurred within the cell - the colour
// much, the shadow (alpha) a little.
// 3.0.4-beta.26: any such atlas (the own ground's is 2048 wide): its size from the texture itself
float4 smoothLightmap(float2 uv, float4 sharp) {
    float2 size;
    tex1.GetDimensions(size.x, size.y);
    float2 t = uv * size;
    float2 lo = floor(t / 8.0) * 8.0 + 0.5;
    float2 hi = lo + 7.0;
    float4 sum = float4(0.0, 0.0, 0.0, 0.0);
    float weights = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x) {
            float w = (x == 0 ? 2.0 : 1.0) * (y == 0 ? 2.0 : 1.0);
            sum += tex1.SampleLevel(samp1, clamp(t + float2(x, y) * 1.5, lo, hi) / size, 0) * w;
            weights += w;
        }
    float4 soft = sum / weights;
    return float4(soft.rgb, lerp(sharp.a, soft.a, 0.3));
}

// 0.71, the ground's relief: its picture's brightness as height (the dark cracks low), its slope across a texel
// turned into the screen's directions (du, dv: the texture's steps a pixel) and lit from up left - a factor about 1
// for the ground's colour; strength uiPixels.x (a world draw has no interface quad). 1.5.10: uiPixels.y the game
// texture's width - its slope taken across one of the game's texels, so that an AI picture (2 or 4 times as many,
// smooth between them) under it keeps the relief the game's texture gives (x4 had a quarter of it).
#ifndef UAGFX_SAFE  // 0.76: the newest features, left out when a compiler will not take them (pipeline.cpp)
float relief(float2 uv, float2 du, float2 dv, float seen) {
    float w, h;
    tex0.GetDimensions(w, h);
    float2 size = float2(w, h);
    const float k = uiPixels.y > 0.0 ? max(1.0, floor(w / uiPixels.y + 0.5)) : 1.0;  // AI texels a game texel
    float2 tx = du * size, ty = dv * size;  // texels a screen pixel
    float area = abs(tx.x * ty.y - tx.y * ty.x);
    // 2.6.6: not nearer than a screen pixel - the texture seen smaller than its texels (uaRO's are 1024 a side) is
    // sampled from a blurred mip, where a texel either way gave nearly the same height and no relief at all
    float2 o = max(k, sqrt(area)) / size;
    const float3 luma = float3(0.2126, 0.7152, 0.0722);
    float hl = dot(tex0.SampleGrad(samp0, uv - float2(o.x, 0.0), du, dv).rgb, luma);
    float hr = dot(tex0.SampleGrad(samp0, uv + float2(o.x, 0.0), du, dv).rgb, luma);
    float hu = dot(tex0.SampleGrad(samp0, uv - float2(0.0, o.y), du, dv).rgb, luma);
    float hd = dot(tex0.SampleGrad(samp0, uv + float2(0.0, o.y), du, dv).rgb, luma);
    // The height across a game texel (or a screen pixel) against the picture's texels a screen pixel (tx, ty) and
    // their area: the texels go out, the relief the game's texture gives.
    float2 g = float2(hr - hl, hd - hu) * 0.5;
    float2 gs = float2(dot(g, tx), dot(g, ty)) / sqrt(max(area, 1e-6));
    // 2.2.2: a face seen nearly edge-on or its texture stretched (a step's side: the texels a pixel nearly in one
    // line, their area near nothing) had its slope blown up - dark and light flickered there as the camera turned: the
    // relief fades out where the pixel sees it poorly (shade()'s measure; a slope clamp would not help: the light
    // below is at its limits from |gs| 0.05 on)
    gs *= seen;
    float3 n = normalize(float3(-gs * 3.5 * uiPixels.x, 1.0));
    const float3 light = float3(-0.4706, -0.5752, 0.6690);  // normalised (-0.45, -0.55, 0.64)
    // 0.83.1: mostly darker on the sides away from the light - lit sides brightened little: light ground (paved
    // squares) went white at 1.3
    return clamp(dot(n, light) / light.z, 0.7, 1.1);
}
#endif

#ifndef UAGFX_SAFE
// 1.17, living characters (draw.cpp spriteConstants): a sprite of a character or a monster lit by the sun as a body.
// 2.6.4: its silhouette seen from a little way off - its alpha 3 and 6 of the game's texels around, eight ways - as a
// height, whose slope is a normal across the whole figure: the side towards the sun (twinX, twinY on the screen, twinW
// its height) lighter and warmer, the far side in a soft shade. (1.17 lit a rim two texels wide at the edge: unseen at
// 60 %, a white outline at 200 %.) uiPixels.x the strength, .y the sun's light here, .z the game texture's width.
float3 spriteLight(float2 uv, float2 du, float2 dv, float3 c) {
    float w, h;
    tex0.GetDimensions(w, h);
    float2 size = float2(w, h);
    float per = uiPixels.z > 0.0 ? max(1.0, floor(w / uiPixels.z + 0.5)) : 1.0;  // its texels a game texel
    float2 ex = du / max(length(du * size), 1e-4) * per;  // a game texel along the screen's x, then its y
    float2 ey = dv / max(length(dv * size), 1e-4) * per;
    float2 g = float2(0.0, 0.0);
    [unroll] for (int way = 0; way < 8; ++way) {
        float2 d = float2(cos(float(way) * 0.785398), sin(float(way) * 0.785398));
        float2 o = ex * d.x + ey * d.y;
        float out6 = tex0.SampleGrad(samp0, uv + o * 6.0, du, dv).a;  // 6 game texels away, then 3
        float out3 = tex0.SampleGrad(samp0, uv + o * 3.0, du, dv).a;
        g += d * (out6 + 0.5 * out3);
    }
    float3 n = normalize(float3(-g * 0.9, 1.0));  // the figure's height falls outwards: its normal leans out
    float2 ls = float2(twinX, twinY);
    float3 L = normalize(float3(ls * sqrt(saturate(1.0 - twinW * twinW)), 0.35 + twinW));
    float amb = lerp(0.85, 0.6, uiPixels.y);
    float k = (amb + (1.0 - amb) * saturate(dot(n, L))) / (amb + (1.0 - amb) * L.z);
    float s = uiPixels.x;
    float f = clamp(1.0 + (clamp(k, 0.72, 1.3) - 1.0) * s, 0.6, 1.45);
    float warm = saturate(k - 1.0) * s;
    return c * f * float3(1.0 + 0.06 * warm, 1.0, 1.0 - 0.08 * warm);
}

// 3.0.4-beta.31 (R-KZBBTN: a straight line across alberta's paving, warm on its one side and cold on the other): a
// lattice point's hash from its whole coordinates in integers (pcg2d, Jarzynski and Olano 2020) - the same however
// a compiler orders the sums. The sine's (frac(sin(dot(p, k)) * 43758)) had a point reached from the cell on its
// left and from the cell on its right as two sums rounded apart in their last bit: two values, the noise broken
// along the lattice's line.
float mhash(float2 p) {
    uint2 v = asuint(int2(p)) * 1664525u + 1013904223u;
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v ^= v >> 16u;
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v ^= v >> 16u;
    return float(v.x >> 8) * (1.0 / 16777216.0);
}
float mnoise(float2 p) {
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(mhash(i), mhash(i + float2(1, 0)), f.x), lerp(mhash(i + float2(0, 1)), mhash(i + 1.0), f.x), f.y);
}

// 1.7, the trees (draw_constants.hpp leafShading): a draw of leaves, its box sourceRect (game pixels). uiPixels.xy
// the way to the sun on the screen, .z the crown's roundness (tree_volume) - lit as a ball from the sun's side, darker
// in its middle; r4 the light through the leaves (leaf_light): a leaf with a hole a few pixels towards the sun is on
// the crown's edge and glows warm green; sourceClamp.rgb the tree's own shade, r0-r2 the autumn's colour and r3 how
// much (tree_tints), r5 rime on the crown's top.
// 1.24: a smooth noise over the crown's own box (q: -1 to 1 across it) - the same patches however near the camera.
// 3.0.4-beta.31: the ground's (mnoise), its hash in integers - the sine's broke it along its lattice here too
float leafNoise(float2 p) { return mnoise(p); }

float3 leafLight(float2 pos, float2 uv, float2 du, float2 dv, float3 c) {
    float2 lo = sourceRect.xy * scale + 0.5 + float2(jitterX, jitterY);
    float2 hi = sourceRect.zw * scale + 0.5 + float2(jitterX, jitterY);
    float2 q = clamp((pos - (lo + hi) * 0.5) / max((hi - lo) * 0.5, 1.0), -1.0, 1.0);
    float2 sun = uiPixels.xy;
    float luma = dot(c, float3(0.299, 0.587, 0.114));
    c *= sourceClamp.rgb;
    // 1.24: clumps of the crown each a little their own - lighter and yellower, deeper and bluer - the tree's own
    // patches (its tint the seed): the same picture repeated on every layer of leaves shows less. 3.0.4-beta.19: on
    // the leaves (their texture's place), not over the box on the screen - the leaves turned under them with the
    // camera and the patches stood (R-E3A6C2 «когда кручу карту меняются цвет деревьев»)
    float2 seed = sourceClamp.rg * 37.0;
    float2 onLeaf = uv * 2.0 - 1.0;
    float clump = leafNoise(onLeaf * 3.2 + seed) * 0.65 + leafNoise(onLeaf * 7.5 + seed * 1.7) * 0.35;
    c *= lerp(float3(0.86, 0.9, 0.98), float3(1.1, 1.08, 0.94), clump);
    // 1.8.2: golden, not brown; 1.24: in patches - some of the crown still green - and by the leaf's own light
    // (luma times the colour: the shade between the leaves stays, the crown is no flat orange)
    if (r3 > 0.0) {
        float turned = r3 * smoothstep(0.25, 0.65, leafNoise(onLeaf * 2.4 + seed * 0.7 + 5.3));
        c = lerp(c, luma * 1.75 * float3(r0, r1, r2), turned);
    }
    if (r5 > 0.0) c = lerp(c, float3(0.86, 0.92, 1.0) * (0.55 + luma), r5 * saturate(0.35 - q.y * 0.65));
    if (uiPixels.z > 0.0) {
        // 1.9.8: and each leaf's own relief - the picture's brightness as height, lit from the sun's side
        float tw, th;
        tex0.GetDimensions(tw, th);
        float2 o = 1.5 / float2(tw, th);
        const float3 lw = float3(0.299, 0.587, 0.114);
        float2 grad = float2(dot(tex0.SampleGrad(samp0, uv + float2(o.x, 0.0), du, dv).rgb, lw) -
                                 dot(tex0.SampleGrad(samp0, uv - float2(o.x, 0.0), du, dv).rgb, lw),
                             dot(tex0.SampleGrad(samp0, uv + float2(0.0, o.y), du, dv).rgb, lw) -
                                 dot(tex0.SampleGrad(samp0, uv - float2(0.0, o.y), du, dv).rgb, lw));
        float2 gs = float2(dot(grad, du * float2(tw, th)), dot(grad, dv * float2(tw, th)));
        c *= clamp(1.0 + dot(gs, sun) * 1.4 * uiPixels.z, 0.75, 1.25);
        float3 n = normalize(float3(q, sqrt(saturate(1.0 - dot(q, q) * 0.85)) + 0.3));
        float lit = dot(n, normalize(float3(sun, 0.8)));
        float hollow = 1.0 - 0.16 * saturate(1.0 - length(q)) * saturate(0.6 + q.y);
        // 1.24: the crown's depth - its lower layers (the box's lower part on the screen) in the shade of the upper
        // ones, darker and cooler; the top in the sky's light
        float under = saturate(q.y * 0.5 + 0.5);
        float3 depth = lerp(float3(1.08, 1.06, 1.0), float3(0.68, 0.74, 0.82), under * under);
        c *= lerp(float3(1.0, 1.0, 1.0), (0.93 + 0.27 * lit) * hollow * depth, uiPixels.z);
    }
    if (r4 > 0.0) {
        float2 o = (du * sun.x + dv * sun.y) * 4.0 * scale;
        float open = 1.0 - tex0.SampleGrad(samp0, uv + o, du, dv).a;
        float edge = open * saturate(dot(q, sun) * 0.8 + 0.45);
        c += float3(0.42, 0.55, 0.12) * (edge * r4 * (0.4 + luma));
    }
    return c;
}
#endif

#ifndef UAGFX_SAFE
// 1.8, the materials (materials.cpp, effects.materials). A draw of the ground or a model with bit 28: its texture's
// material in t2 (relief as a normal xy, height, gloss); the point and the surface from the game's projection
// (f0-farZ) and the draw's depth; twinX-twinW the way to the sun in the view and twinH the strength; uiPixels.x the
// sun's light here (0 indoors and at night), .y the gloss's. The game's light stays: what the relief adds is the
// sunlit side brighter and the far side darker against the flat surface's, the hollows darker and the smooth stone
// with a glint. Bit 29: the texture taken a little along the view by its height (parallax); bit 30, the ground: large
// patches of shade over the world (r0-r11 the view -> the world) so that one tile's repeats do not show (mnoise).

float3 materialLight(float3 c, float4 m, float3 P, float3 N, float3 T, float3 B, float seen) {
    float2 xy = m.xy * 2.0 - 1.0;
    float3 n = normalize(T * xy.x + B * xy.y + N * sqrt(saturate(1.0 - dot(xy, xy))));
    float3 L = normalize(float3(twinX, twinY, twinW));
    float sunLight = uiPixels.x;
    float amb = lerp(0.85, 0.5, sunLight);
    float k = (amb + (1.0 - amb) * saturate(dot(n, L))) / (amb + (1.0 - amb) * saturate(dot(N, L)));
    float hollow = lerp(0.8, 1.0, saturate(m.z * 1.8));
    float3 V = normalize(-P);
    // 2.2.2: a face turned from the light (a step's side in a dark gap) was lit up to 1.6 times by its relief alone,
    // and one seen nearly edge-on took its relief from a few garbled texels - white glints flashing as the camera
    // turned: at most 1.3 times now, and the relief and the glint fade where the pixel sees the relief poorly
    // (shade()'s measure: edge-on, stretched, its texels along a line)
    float s = twinH * seen;
    c *= lerp(1.0, clamp(k, 0.55, 1.3) * hollow, s);
    float3 H = normalize(L + V);
    float spec = pow(saturate(dot(n, H)), 10.0 + 70.0 * m.w) * m.w * uiPixels.y * (0.3 + 0.7 * sunLight);
    // 2.1.7: a glint only as bright as the place - white over a dark gap, it flashed as the camera turned
    spec *= saturate(dot(c, float3(0.3, 0.59, 0.11)) * 2.5);
    return c + spec * 0.45 * s;
}

// 1.15, smooth hills (core/hills.hpp): the ground's normal between the corners of its cells, each corner's packed in
// t3 (a float: 4096 * (1 + x's 11 bits) + z's 12 bits, y upwards the rest; 0 none) - sourceRect.xy the world's place
// of corner (0, 0), .z one over the cells' side, .w the corners across + 1024 * down; sourceClamp.xyz the way to the
// sun in the world, footprint the strength, sharpen the light all around. The light of that normal against a flat
// ground's: the sunny slopes brighter, the far ones in a soft shade, no steps from cell to cell.
float hillLight(float3 P) {
    float3 w = P.x * float3(r0, r1, r2) + P.y * float3(r3, r4, r5) + P.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    int across = int(fmod(sourceRect.w, 1024.0) + 0.5), down = int(sourceRect.w / 1024.0);
    float2 g = (w.xz - sourceRect.xy) * sourceRect.z;
    float2 b = floor(g), f = g - b;
    float3 n = float3(0.0, 0.0, 0.0);
    float weight = 0.0;
    [unroll] for (int corner = 0; corner < 4; ++corner) {  // 1.25.1: not k - FXC keeps a loop's name to the end
        int2 o = int2(corner & 1, corner >> 1);
        int2 q = int2(b) + o;
        if (q.x < 0 || q.y < 0 || q.x >= across || q.y >= down) continue;
        float v = sceneDepth.Load(int3(q, 0));
        if (v < 4096.0) continue;
        float hi = floor(v / 4096.0), lo = v - hi * 4096.0;
        float nx = (hi - 1.0) / 1023.0 - 1.0, nz = lo / 2047.5 - 1.0;
        float bw = (o.x == 1 ? f.x : 1.0 - f.x) * (o.y == 1 ? f.y : 1.0 - f.y);
        n += float3(nx, -sqrt(saturate(1.0 - nx * nx - nz * nz)), nz) * bw;
        weight += bw;
    }
    if (weight < 0.01) return 1.0;
    n = normalize(n);
    float3 L = normalize(sourceClamp.xyz);
    float amb = sharpen;
    float k = (amb + (1.0 - amb) * saturate(dot(n, L))) / max(amb + (1.0 - amb) * saturate(-L.y), 1e-3);
    return lerp(1.0, clamp(k, 0.6, 1.4), footprint * saturate(weight * 2.0 - 0.5));
}
#endif

float4 shade(PsIn i) {
    float2 uv0 = ((stage0.y >> 16) & 7) == 1 ? i.uv1 : i.uv0;
    float2 du = ddx(uv0);  // outside any branch; the interface path only uses explicit gradients
    float2 dv = ddy(uv0);
#if !defined(UAGFX_SAFE) && !defined(UAGFX_LEAN)
    // 1.8: the point in the view and its surface's slopes, also outside any branch (garbage where unused)
    float pz = i.pos.w;
    float2 pg = (i.pos.xy - 0.5 - float2(jitterX, jitterY)) / max(scale, 1e-3);
    float3 P = float3((pg.x - cx) * pz / max(f0, 1e-3), (pg.y - cy) * pz / max(f1, 1e-3), pz);
    float3 dp1 = ddx(P), dp2 = ddy(P);
    // 1.9, a strip across a seam of the ground (seams.cpp): the other cell's texture where the noise over the world
    // says: one wandering line between the two textures. 1.9.5: diffuse alpha how much the place is the other's
    // (bilinear between the cells' middles: corners round off), the noise up on one side and down on the other - one
    // line both sides agree on. 3.0.5-beta.15 (R-R9XBKP: the strips 7.0 ms of a Radeon 610M's 42 on «Високі», the
    // dearest part): worked out first - a neighbour's strip's pixel the noise leaves the cell's own texture shows
    // nothing (its alpha), and is left before its textures, materials and hills (the most of a strip)
    float seamOther = 1.0;
    if (((stage1.y >> 31) & 1) != 0) {
        float3 w = P.x * float3(r0, r1, r2) + P.y * float3(r3, r4, r5) + P.z * float3(r6, r7, r8) +
                   float3(r9, r10, r11);
        float n = mnoise(w.xz * 0.22) * 0.65 + mnoise(w.xz * 0.7 + 5.1) * 0.35;
        float side = i.specular.g > 0.5 ? 1.0 : -1.0;
        seamOther = smoothstep(-0.05, 0.05, i.diffuse.a - 0.5 + side * (n - 0.5) * 0.9);
        if (i.specular.r <= 0.5 && seamOther < 0.001) discard;  // under half a step of 255 over the ground
    }
    bool material = ((stage1.y >> 28) & 1) != 0;
    float4 m = float4(0.5, 0.5, 0.5, 0.0);
    float pane = 0.0;
    float3 N = float3(0.0, 0.0, -1.0), T = float3(1.0, 0.0, 0.0), B = float3(0.0, 1.0, 0.0);
    float seen = 1.0;
    if (material || ((stage1.y >> 26) & 1) != 0) {  // the materials or the ground's relief (a draw's own, uniform)
        // the surface's normal towards the camera and its texture's way along it (T with u, B with v, the longer 1)
        N = normalize(cross(dp1, dp2));
        if (dot(N, P) > 0.0) N = -N;
        float3 dp2perp = cross(dp2, N), dp1perp = cross(N, dp1);
        T = dp2perp * du.x + dp1perp * dv.x;
        B = dp2perp * du.y + dp1perp * dv.y;
        float im = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-12));
        T *= im;
        B *= im;
        // 2.2.2: how clearly a pixel sees its texture's relief - one measure for the relief, the materials and their
        // glint, each now faded by all three causes the glints of 2.1.5-2.2.1 came from (each had one of its own and
        // passed the others): the texture stretched along the face (a step's side: stripes, 2.1.7), the face seen
        // nearly edge-on, the pixel's texels along one line (the two at once)
        float tw, th;
        tex0.GetDimensions(tw, th);
        float2 tx = du * float2(tw, th), ty = dv * float2(tw, th);  // the texels a screen pixel spans
        float spread = abs(tx.x * ty.y - tx.y * ty.x) / max(max(dot(tx, tx), dot(ty, ty)), 1e-6);
        seen = min(min(saturate(min(length(T), length(B)) * 4.0 - 0.4),
                       saturate(abs(dot(normalize(-P), N)) * 4.0 - 0.1)),
                   saturate(spread * 6.0 - 0.2));
    }
    if (material) {
        m = sceneColor.SampleGrad(samp0, uv0, du, dv);
        m = lerp(float4(0.5, 0.5, 0.5, 0.0), m, seen);  // garbled relief and gloss: none (c_tower2's gaps)
        if (seen > 0.99 && ((stage1.y >> 29) & 1) != 0) {  // parallax: one step along the view by the height
            float3 V = normalize(-P);
            float3 vt = float3(dot(V, T), dot(V, B), dot(V, N));
            // 2.1.5: at most a pixel and a half on the screen - farther, the step left a tile's part of its
            // texture for the next one's (smears and torn edges on c_tower2's floor)
            float2 shift = (m.z - 0.5) * 0.035 * vt.xy / (vt.z + 0.42);
            float reach = 1.5 * max(length(du), length(dv));
            uv0 += clamp(shift, -reach, reach);
            m = sceneColor.SampleGrad(samp0, uv0, du, dv);
        }
        pane = saturate((m.w - 0.74) * 4.0);  // 1.21: a window's pane (material_ps): glass's gloss
        if (m.w > 0.72) m.w = 0.35 * pane;
    }
    float4 t0 = tex0.SampleGrad(samp0, uv0, du, dv);
    if (((stage1.y >> 26) & 1) != 0) t0.rgb *= relief(uv0, du, dv, seen);  // 0.71: the ground
#else
    float4 t0 = tex0.Sample(samp0, uv0);
#endif
    if (uiMode == 3)
        t0 = twinTexel(i.pos.xy);
    else if (uiMode != 0)
        t0 = uiSample(uv0, du, dv, i.pos.xy);
    float2 uv1 = ((stage1.y >> 16) & 7) == 0 ? i.uv0 : i.uv1;
    float4 t1 = tex1.Sample(samp1, uv1);
    if (((stage1.y >> 25) & 1) != 0) t1 = smoothLightmap(uv1, t1);  // 0.22.6: the ground's lightmap (2.5.2: lean too)
    if (colorKey != 0) {  // colour-keyed textures carry the key as alpha 0 (TextureLevels, Task 10)
        if (((stage0.y >> 24) & 1) != 0 && t0.a == 0) discard;
        if (((stage1.y >> 24) & 1) != 0 && t1.a == 0) discard;
    }
    float4 c = i.diffuse;  // stage 0 off: diffuse
    if ((stage0.x & 255) != 1) {
        c = stage(stage0, i.diffuse, i.specular, i.diffuse, t0);
        if ((stage1.x & 255) != 1) c = stage(stage1, i.diffuse, i.specular, c, t1);
    }
#if !defined(UAGFX_SAFE) && !defined(UAGFX_LEAN)
    if (((stage1.y >> 27) & 1) != 0) c.rgb = leafLight(i.pos.xy, uv0, du, dv, c.rgb);  // 1.7: the trees
    if (((stage0.y >> 28) & 1) != 0) c.rgb = spriteLight(uv0, du, dv, c.rgb);  // 1.17: the characters
    if (material) {
        c.rgb = materialLight(c.rgb, m, P, N, T, B, seen);  // 1.8: the materials
        // 1.14, cliffs: a steep face whose texture the game stretches along it (one of its coordinates moves far
        // slower over the world than the other: T or B short) gets a rock's own detail over the world, in the
        // face's own two axes, breaking the streaks - its picture is not taken apart (an atlas's quarters stay)
        float stretch = 1.0 - saturate(min(length(T), length(B)) * 2.5);
        float3 nw = N.x * float3(r0, r1, r2) + N.y * float3(r3, r4, r5) + N.z * float3(r6, r7, r8);
        float k = stretch * (1.0 - smoothstep(0.5, 0.8, abs(nw.y))) * twinH;
        // 1.21, windows at night (uiPixels.z how much): a pane on a wall glows warm, one window in three left dark
        if (uiPixels.z > 0.0 && pane > 0.0) {
            float3 w = P.x * float3(r0, r1, r2) + P.y * float3(r3, r4, r5) + P.z * float3(r6, r7, r8) +
                       float3(r9, r10, r11);
            float wall = 1.0 - smoothstep(0.35, 0.6, abs(nw.y));
            float on = step(0.33, mhash(floor(float2(w.x + w.z, w.y) / 3.0)));
            float3 warm = float3(1.0, 0.84, 0.52) * (0.85 + 0.15 * mhash(floor(float2(w.x - w.z, w.y) / 3.0)));
            c.rgb = lerp(c.rgb, warm, saturate(pane * 1.3) * wall * on * uiPixels.z);
        }
        if (k > 0.01) {
            float3 w = P.x * float3(r0, r1, r2) + P.y * float3(r3, r4, r5) + P.z * float3(r6, r7, r8) +
                       float3(r9, r10, r11);
            float2 a = abs(nw.x) > abs(nw.z) ? float2(w.z, w.y) : float2(w.x, w.y);
            float detail = mnoise(a * 0.9) * 0.5 + mnoise(a * 2.3 + 7.1) * 0.3 + mnoise(a * 5.7 + 3.3) * 0.2;
            float crack = smoothstep(0.06, 0.0, abs(mnoise(a * 1.4 + 9.2) - 0.5));
            c.rgb *= lerp(1.0, (0.78 + 0.42 * detail) * (1.0 - 0.35 * crack), saturate(k));
        }
    }
    if (((stage0.y >> 26) & 1) != 0) c.rgb *= hillLight(P);  // 1.15: smooth hills
    if (((stage1.y >> 30) & 1) != 0) {  // 1.8: the ground's large patches
        float3 w = P.x * float3(r0, r1, r2) + P.y * float3(r3, r4, r5) + P.z * float3(r6, r7, r8) +
                   float3(r9, r10, r11);
        float warm = (mnoise(w.xz * 0.03 + 17.0) - 0.5) * 0.35;
        // 3.0.4-beta.31 (R-KZBBTN: «непонятный оттенок» - alberta's white paving in bluish and yellowish patches):
        // warmer and colder as much as the ground has a colour of its own - grass, earth and sand as before, white
        // and grey stone only lighter and darker
        float most = max(c.r, max(c.g, c.b));
        warm *= smoothstep(0.06, 0.25, (most - min(c.r, min(c.g, c.b))) / max(most, 1e-3));
        c.rgb *= (0.88 + 0.24 * mnoise(w.xz * 0.012)) * float3(1.0 + warm * 0.6, 1.0 + warm * 0.15, 1.0 - warm * 0.5);
    }
    // 1.9: a strip across a seam - its alpha (seamOther, worked out above)
    if (((stage1.y >> 31) & 1) != 0) {
        bool own = i.specular.r > 0.5;  // 1.9.2: the cell's own band at the edge, its rim covered
        // 1.25: the own band whole at the edge (drawn first, applySeams) - partly see-through, the texture's dark rim
        // under it showed through the neighbour's strip as a line along the cells (the desert's "cracks")
        // 2.0.2: whole over the rim only, then fading out - whole over all of it, its mirror's axis showed
        c.a = own ? 1.0 - smoothstep(0.3, 0.9, i.specular.b) : seamOther;
    }
#endif
    if (fogEnable == 2) c.rgb = lerp(c.rgb, fogColor.rgb, fogColor.a);  // 2.3: uaRO Studio's X-ray, the state's colour
    else if (fogEnable != 0) c.rgb = lerp(fogColor.rgb, c.rgb, i.specular.a);
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
    // 0.67, soft particles: an effect fades over the last few units before what is behind it (the frame's depth so
    // far, sceneDepth) - no hard line where it goes into the ground; its colour too when it adds light (bit 21).
#if !defined(UAGFX_SAFE) && !defined(UAGFX_LEAN)
    if (((alphaTest >> 20) & 1) != 0) {
        float behind = viewDepth(sceneDepth.Load(int3(int2(i.pos.xy), 0))) - viewDepth(i.pos.z);
        float fade = smoothstep(0.0, 1.0, saturate(behind / 7.0));
        c.a *= fade;
        if (((alphaTest >> 21) & 1) != 0) c.rgb *= fade;
    }
#endif
    // 2.7.38: a skill's light laid over softly (draw.cpp: the target's colour its factor, not its alpha) - as much
    // of it as its alpha lets through
    if (((alphaTest >> 23) & 1) != 0) c.rgb *= c.a;
    return c;
}

float4 draw_ps(PsIn i) : SV_Target { return shade(i); }

// 2.5: the same shader without the world's extras (UAGFX_LEAN, pipeline.cpp: the materials, the relief, the hills,
// the seams, the leaves, the sprites' light, the soft particles): a draw with none of their bits - the sprites, the
// effects, the interface, most of a frame - takes it: fewer registers and no explicit gradients, which the old cards
// pay for dearly. 2.5.2: the smooth lightmap stays in it - the ground of a weak card's set is lean with it.
float4 draw_lean_ps(PsIn i) : SV_Target { return shade(i); }

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
    // 2.7.36 (the tester in Comodo at night: «скилы очень яркие»): a skill's light 40 % into the glow (bit 22) - a
    // Magnus' columns, their light piled up, and its halo and light on what is around laid a white blot over them
    if (((alphaTest >> 22) & 1) != 0) o.g.rgb *= 0.4;
    return o;
}

GlowOut draw_glow_lean_ps(PsIn i) { return draw_glow_ps(i); }  // 2.5: with UAGFX_LEAN

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
// 0.58: the world drawn smaller than the screen (render_scale under 100 %) made larger again with sharpening on: a
// Catmull-Rom spline (nine bilinear taps, after Jimenez's and Pettineo's) - sharper than bilinear, then CAS on it, as
// AMD's FSR 1 sharpens its upscale (our own filter, not AMD's EASU).
float3 crTap(float2 p) { return tex0.SampleLevel(samp0, clamp(p, sourceClamp.xy, sourceClamp.zw), 0).rgb; }
float3 catmullRom(float2 uv) {
    float w, h;
    tex0.GetDimensions(w, h);
    float2 size = float2(w, h);
    float2 at = uv * size;
    float2 t1 = floor(at - 0.5) + 0.5;
    float2 f = at - t1;
    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);
    float2 w12 = w1 + w2;
    float2 p0 = (t1 - 1.0) / size, p3 = (t1 + 2.0) / size, p12 = (t1 + w2 / w12) / size;
    float3 r = crTap(float2(p0.x, p0.y)) * w0.x * w0.y + crTap(float2(p12.x, p0.y)) * w12.x * w0.y +
               crTap(float2(p3.x, p0.y)) * w3.x * w0.y + crTap(float2(p0.x, p12.y)) * w0.x * w12.y +
               crTap(float2(p12.x, p12.y)) * w12.x * w12.y + crTap(float2(p3.x, p12.y)) * w3.x * w12.y +
               crTap(float2(p0.x, p3.y)) * w0.x * w3.y + crTap(float2(p12.x, p3.y)) * w12.x * w3.y +
               crTap(float2(p3.x, p3.y)) * w3.x * w3.y;
    return max(r, 0.0);
}

float3 fetch(float2 uv, float2 dx, float2 dy) {
    if (footprint < 0.999 && sharpen >= 0.0) return catmullRom(uv);
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

// 0.65: [display] hdr - the picture (sRGB) to HDR10 (PQ, the BT.2020 colours): white at twinX nits, what is near
// white a little brighter (the fires, the sparks, the lamps) - up to twice it.
float3 toPq(float3 srgb, float paper) {
    float3 lin = lerp(pow((srgb + 0.055) / 1.055, 2.4), srgb / 12.92, step(srgb, 0.04045));
    float top = max(lin.r, max(lin.g, lin.b));
    lin *= 1.0 + smoothstep(0.75, 1.0, top);
    float3 c;
    c.r = dot(lin, float3(0.6274, 0.3293, 0.0433));
    c.g = dot(lin, float3(0.0691, 0.9195, 0.0114));
    c.b = dot(lin, float3(0.0164, 0.0880, 0.8956));
    c *= paper / 10000.0;
    float3 p = pow(max(c, 0.0), 0.1593017578125);
    return pow((0.8359375 + 18.8515625 * p) / (1.0 + 18.6875 * p), 78.84375);
}

float4 present_ps(PresentOut i) : SV_Target {
    float2 dx = ddx(i.uv);  // one destination pixel in the source; outside any branch
    float2 dy = ddy(i.uv);
    float3 c = fetch(i.uv, dx, dy);
    if (sharpen >= 0.0)
        c = cas(c, fetch(i.uv - dy, dx, dy), fetch(i.uv + dy, dx, dy), fetch(i.uv - dx, dx, dy), fetch(i.uv + dx, dx, dy));
    if (twinX > 0.0) c = toPq(saturate(c), twinX);  // 0.65: onto an HDR10 back buffer
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
        float3 c = (inner * w0 + b1 * w1 + b2 * w2 + b3 * w3 + b4 * w4) / (w0 + w1 + w2 + w3 + w4);
        // 0.26: only the bright glows - a soft knee from 0.2 to 0.8 (the level 99 aura's even light, a skill's broad
        // haze hardly), the cores of fire and the flashes fully.
        float top = max(c.r, max(c.g, c.b));
        float soft = clamp(top - 0.2, 0.0, 0.6);
        soft = soft * soft / 1.2;
        return float4(c * (max(soft, top - 0.5) / max(top, 1e-4)), 1.0);
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

// A pixel of a picture of this size for a point of the game's view space; false when it is off the picture or behind
// the camera. 0.86: the size given - the floor's reflections are drawn at half of the world's.
bool viewToPixelIn(float3 q, float2 size, out float2 tp) {
    tp = float2(0.0, 0.0);
    if (q.z <= nearZ) return false;
    tp = float2(q.x / q.z * f0 + cx, q.y / q.z * f1 + cy) * scale + 0.5 + float2(jitterX, jitterY);
    return all(tp >= 0.0) && all(tp < size);
}
bool viewToPixel(float3 q, out float2 tp) { return viewToPixelIn(q, targetSize, tp); }

// What the reflected ray meets first in the picture (size: its pixels) before the water (screen-space reflection),
// and how sure that is: 0 for the sky (the ray left the picture or met nothing).
float3 reflectionIn(float3 p, float3 r, float2 size, out float weight) {
    weight = 0.0;
    float dist = 2.0;
    float2 tp;
    [loop] for (int k = 0; k < 28; ++k) {
        float3 q = p + r * dist;
        if (!viewToPixelIn(q, size, tp)) break;
        float behind = q.z - viewDepth(sceneDepth.Load(int3(int2(tp), 0)));
        if (behind > 0.0 && behind < 3.0 + dist * 0.12) {
            float2 edge = min(tp, size - tp) / (0.08 * size);
            weight = saturate(min(edge.x, edge.y)) * saturate(1.5 - dist / 400.0);
            return sceneColor.Load(int3(int2(tp), 0)).rgb;
        }
        dist *= 1.22;
    }
    return float3(0.0, 0.0, 0.0);
}
float3 waterReflection(float3 p, float3 r, out float weight) { return reflectionIn(p, r, targetSize, weight); }

// 2.7.17, the rain's puddles (floor_reflect_ps): what stands close by, in even steps of a unit, fading along the way.
// reflectionIn's ever longer steps (made for the open water) met a standing sprite again and again, ever higher up it:
// a column of its copies twice its height below it (the tester's shot at prt_fild08, 2.7.16).
float3 puddleReflection(float3 p, float3 r, float2 size, out float weight) {
    weight = 0.0;
    float2 tp;
    [loop] for (int k = 1; k <= 24; ++k) {
        float dist = float(k);
        float3 q = p + r * dist;
        if (!viewToPixelIn(q, size, tp)) break;
        float behind = q.z - viewDepth(sceneDepth.Load(int3(int2(tp), 0)));
        if (behind > 0.0 && behind < 1.5) {
            float2 edge = min(tp, size - tp) / (0.08 * size);
            float fade = 1.0 - dist / 24.0;
            weight = saturate(min(edge.x, edge.y)) * fade * fade;
            return sceneColor.Load(int3(int2(tp), 0)).rgb;
        }
    }
    return float3(0.0, 0.0, 0.0);
}

// 1.7, the water's life (water(): twinH's bits): a swamp's bubbles swelling and bursting into rings, its water
// murkier (2.7.2: the fish circling under the surface gone - too faint to be seen).
float3 waterLife(float3 c, uint life, float2 xz, float t, float light) {
    if ((life & 2) != 0) {
        float2 bid = floor(xz / 3.0);
        float seed = hash12(bid + 13.1);
        float2 at = (bid + 0.25 + 0.5 * float2(hash12(bid + 2.2), hash12(bid + 5.5))) * 3.0;
        float age = frac(t * (0.18 + 0.12 * seed) + seed * 9.0);
        float d = length(xz - at);
        c *= float3(0.9, 0.95, 0.82);
        if (age < 0.7) {  // swelling
            float r = 0.08 + age * 0.35;
            float rim = exp(-pow((d - r * 0.7) * 18.0, 2.0));
            c = lerp(c, c * 0.75, smoothstep(r, r * 0.6, d) * 0.5) + rim * 0.12 * light;
        } else {  // burst: a ring
            float k = (age - 0.7) / 0.3;
            float x = d - (0.2 + k * 0.9);
            c += exp(-x * x * 60.0) * (1.0 - k) * 0.18 * light;
        }
    }
    return c;
}

float4 water(WaterIn i) {
    int2 px = int2(i.pos.xy);
    float t = sharpen;
    float waves = footprint;
    // 1.7: 2 a swamp's bubbles (water_life; 1 the fish and 4 the surf's swash gone in 2.7.2); 2.4: the water's own
    // parts, each a box of its own - 8 the foam along the shore, 16 the sun's glints, 32 the light on the bed
    uint life = uint(twinH + 0.5);
    // 0.22.2: the world's axes turned, for the noise: no rows along them.
    float2 fp = float2(dot(i.world.xz, float2(0.8, 0.6)), dot(i.world.xz, float2(-0.6, 0.8)));
    float2 slope = waveSlope(i.world.xz, t);
    // Ripples of noise on the swells, so that their glints do not line up.
    float2 rq = fp * 0.55 + t * float2(0.21, 0.13);
    float n0 = valueNoise(rq);
    float2 g = float2(valueNoise(rq + float2(0.15, 0.0)) - n0, valueNoise(rq + float2(0.0, 0.15)) - n0) / 0.15;
    slope += float2(0.8 * g.x - 0.6 * g.y, 0.6 * g.x + 0.8 * g.y) * 0.05;
    slope *= 0.15 + 1.6 * waves;
    // 0.94: rings where the last steps went (twinW of them: x, z, age in uiPixels, sourceRect, sourceClamp) - a
    // wave packet running out from each, fading as it spreads.
    [unroll] for (int s = 0; s < 3; ++s) {
        float4 at = s == 0 ? uiPixels : s == 1 ? sourceRect : sourceClamp;
        float on = step(float(s) + 0.5, twinW);
        float2 away = i.world.xz - at.xy;
        float d = length(away);
        float age = at.z;
        float radius = 0.4 + age * 5.0;
        float x = d - radius;
        float packet = exp(-x * x * 1.2) * saturate(1.0 - age / 1.6) * on;
        float wave = cos(x * 5.0) * packet * 0.35;
        slope += away / max(d, 0.05) * wave;
    }
    float3 n = normalize(float3(-slope.x, -1.0, -slope.y));  // up is -y
    float3 eye = float3(r9, r10, r11);
    float3 toEye = normalize(eye - i.world);
    float ndv = saturate(dot(n, toEye));
    float3 nv = float3(dot(n, float3(r0, r1, r2)), dot(n, float3(r3, r4, r5)), dot(n, float3(r6, r7, r8)));
    float waterZ = i.view.z;
    float2 dz = float2(ddx(waterZ), ddy(waterZ));  // the water's depth from pixel to pixel (outside any branch)
    float thick = max(bedDepth(sceneDepth.Load(int3(px, 0))) - waterZ, 0.0);
    // The bed through the waves - shifted by how far the waves tilt the water from flat (its up in the view:
    // -y of the world); where the shifted pixel is in front of the water, straight through.
    float2 tilt = nv.xy + float2(r1, r4);
    float2 shift = tilt * (36.0 * scale) * saturate(thick / 6.0) * (0.2 + waves);
    int2 rp = clamp(int2(i.pos.xy + shift), int2(0, 0), int2(targetSize) - 1);
    float bedZ = bedDepth(sceneDepth.Load(int3(rp, 0)));
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
    if ((life & 32) != 0) bed *= 1.0 + causticAt(bedXz, t) * shallow * waves;
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
#ifndef UAGFX_SAFE
    if (((stage0.y >> 27) & 1) != 0) {  // 1.16: the world mirrored (t5) where the picture has nothing to reflect
        float4 mirrored = mirrorTex.SampleLevel(samp0, saturate(i.pos.xy / targetSize + slope * 0.02), 0);
        seen = lerp(mirrored.rgb, seen, hit);
        hit = max(hit, mirrored.a);
    }
#endif
    float3 reflection = lerp(sky, seen, hit);
    float f0r = lerp(0.02, 0.3, twinX);
    float fresnel = f0r + (1.0 - f0r) * pow(1.0 - ndv, 5.0);
    float3 c = lerp(body, reflection, fresnel);
    // The sun's glints.
    float3 h = normalize(normalize(float3(-0.45, -0.8, 0.4)) + toEye);
    float ndh = saturate(dot(n, h));
    float sun = fogEnable != 0 ? 0.25 : 1.0;  // in a fog (dungeons, caves) hardly any sun
    // 2.7.34 (the tester in Alberta: «нравятся, но слишком шумно»): the sharp glints half as bright and in patches
    // that drift with the waves - all over the water they were a field of noise
    float patch = smoothstep(0.42, 0.62, valueNoise(fp * 0.06 + t * float2(0.03, -0.02)));
    if ((life & 16) != 0)
        c += (pow(ndh, 600.0) * 1.5 * patch + pow(ndh, 60.0) * 0.12) * light * (0.25 + 0.75 * waves) * sun;
    // 0.22.2: foam where the water meets the shore - near a pixel whose ground stands at or above the water there
    // (the water's own depth carried over by its slope on the screen), not over the whole of shallow water -
    // broken by noise that drifts with the waves.
    if ((life & 8) != 0) {
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
        // 1.25: lines of foam along the shore (the water's depth's lines) rolling in and broken by noise, a thin rim
        // at the edge - the blobs of noise before lay as a row of white dabs
        float broken =
            smoothstep(0.3, 0.7, valueNoise(fp * 0.9 + t * 0.2) * 0.7 + valueNoise(fp * 2.3 - t * 0.3) * 0.3);
        float rim = smoothstep(0.35, 0.05, thick);
        float roll = frac(thick * 1.3 - t * 0.22);
        float swell = exp(-pow((roll - 0.5) * 9.0, 2.0)) * saturate(1.0 - thick / 2.5);
        float froth = saturate(rim * (0.5 + 0.5 * broken) + swell * broken * 0.7);
        c = lerp(c, float3(0.92, 0.95, 0.97) * light, saturate(shore * 2.0) * froth * 0.55);
    }
    c = waterLife(c, life, i.world.xz, t, light);
    return float4(c, saturate(thick / 0.8));  // the shore line fades in, no hard edge
}

// 0.20, soft shadows where surfaces meet (screen-space ambient occlusion): on the map and its models once they are
// drawn, before the characters and the effects. f0-farZ are the game's projection, scale and jitter the world's;
// uiPixels the world's size and the shadows' (half of it); footprint the radius in world units, sharpen the
// strength (0-1); sourceRect.xy the blur's direction.
float3 viewFrom(int2 wp, float raw) {  // 2.6.8: viewAt with the pixel's depth read already
    float z = viewDepth(raw);
    float2 g = (float2(wp) - float2(jitterX, jitterY)) / scale;
    return float3((g.x - cx) * z / f0, (g.y - cy) * z / f1, z);
}

float3 viewAt(int2 wp) {  // the point of the view the world pixel wp shows (its centre: game (wp - jitter) / scale)
    return viewFrom(wp, sceneDepth.Load(int3(wp, 0)));
}

float3 viewFromZ(int2 wp, float z) {  // 3.0.5-beta.21: viewFrom with the view depth worked out already
    float2 g = (float2(wp) - float2(jitterX, jitterY)) / scale;
    return float3((g.x - cx) * z / f0, (g.y - cy) * z / f1, z);
}

// 3.0.5-beta.21 (the user: «продолжаем» - the soft shadows 3.3 ms of a Radeon 610M's 17 on «Середні»): the view
// depth of each pixel of the shadows' mask once - its pixels' middle's, as ao_ps and ao_blur_ps took it from the
// picture's depth, a load and a division each, about twenty times a pixel - for them to read (ao_ps t0, ao_blur_ps
// t1): the same depths
float ao_depth_ps(PresentOut i) : SV_Target {
    int ratio = int(uiPixels.x / uiPixels.z + 0.5);
    return viewDepth(sceneDepth.Load(int3(min(int2(i.pos.xy) * ratio + ratio / 2, int2(uiPixels.xy) - 1), 0)));
}

float ao_ps(PresentOut i) : SV_Target {
    const float3 kernel[12] = {
        float3(-0.0933, 0.0078, 0.0503), float3(-0.1224, 0.0021, 0.0252), float3(-0.0230, -0.1494, 0.0394),
        float3(-0.0421, 0.1821, 0.0711), float3(0.0986, -0.0906, 0.2185), float3(0.2057, -0.0826, 0.2377),
        float3(0.1547, -0.0847, 0.3660), float3(0.3701, 0.2501, 0.2247), float3(0.0994, 0.0337, 0.5971),
        float3(-0.7008, -0.0222, 0.1846), float3(0.3223, 0.5069, 0.6102), float3(0.3189, 0.2700, 0.9085)};
    int ratio = int(uiPixels.x / uiPixels.z + 0.5);  // 0.39.3: 2, or 4 for a big world
    int2 last = int2(uiPixels.xy) - 1;
    int2 wp = min(int2(i.pos.xy) * ratio + ratio / 2, last);  // the middle of its pixels
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) return 1.0;  // nothing drawn there
    // 3.0.5-beta.21: it and the mask's pixels beside it by their depths from ao_depth_ps (t0) - two pixels of the
    // picture apart at a half's size as before, four at a quarter's
    int2 m = int2(i.pos.xy), mLast = int2(uiPixels.zw) - 1;
    int2 ml = max(m - int2(1, 0), int2(0, 0)), mr = min(m + int2(1, 0), mLast);
    int2 mu = max(m - int2(0, 1), int2(0, 0)), md = min(m + int2(0, 1), mLast);
    float3 p = viewFromZ(wp, tex0.Load(int3(m, 0)).r);
    // The normal from the neighbours, each on the side nearer in depth, so that an edge does not bend it.
    float3 l = viewFromZ(min(ml * ratio + ratio / 2, last), tex0.Load(int3(ml, 0)).r);
    float3 r = viewFromZ(min(mr * ratio + ratio / 2, last), tex0.Load(int3(mr, 0)).r);
    float3 u = viewFromZ(min(mu * ratio + ratio / 2, last), tex0.Load(int3(mu, 0)).r);
    float3 d = viewFromZ(min(md * ratio + ratio / 2, last), tex0.Load(int3(md, 0)).r);
    // at the mask's edge the neighbour past it is the pixel itself - the one on the other side, mirrored (no slope of
    // nothing: the normal of none)
    if (m.x == 0) l = 2.0 * p - r;
    if (m.x == mLast.x) r = 2.0 * p - l;
    if (m.y == 0) u = 2.0 * p - d;
    if (m.y == mLast.y) d = 2.0 * p - u;
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
    // 3.0.5-beta.21: the mask's pixels' view depths from ao_depth_ps (t1) - their pixels' middles', as before
    float z0 = tex1.Load(int3(p, 0)).r;
    float sum = 0.0, weights = 0.0;
    [unroll] for (int k = -4; k <= 4; ++k) {
        int2 q = clamp(p + dir * k, int2(0, 0), last);
        float zq = tex1.Load(int3(q, 0)).r;
        float w = exp(-float(k * k) / 8.0) * saturate(1.0 - abs(zq - z0) / (0.04 * z0 + 1.0));
        sum += tex0.Load(int3(q, 0)).r * w;
        weights += w;
    }
    return sum / max(weights, 1e-4);
}

// Onto the picture: multiplied (blend ZERO, SRCCOLOR).
float4 ao_apply_ps(PresentOut i) : SV_Target {
    float ratio = floor(uiPixels.x / uiPixels.z + 0.5);  // 0.39.3: 2, or 4 for a big world
    float ao = tex0.SampleLevel(samp0, i.pos.xy / (ratio * uiPixels.zw), 0).r;
    float f = lerp(1.0, ao, sharpen);
    return float4(f, f, f, 1.0);
}

// 0.21, the picture's colours (grade): from its copy (t2) into the picture, before the interface. sharpen the
// saturation (0-2), footprint the contrast and twin.x the brightness (0.5-1.5), twin.y the warmth (-1 colder to 1
// warmer), twin.z the vignette (0-1); uiPixels.xy the world's size.
// 3.0.5-beta.4: the colours of a pixel at `pos` of a picture `size` big (grade_ps, world_grade_ps)
float3 gradeColour(float3 c, float2 pos, float2 size, float saturation, float contrast, float brightness,
                   float warmth, float vignette) {
    c = pow(max(c, 0.0), 1.0 / brightness);  // brighter middle tones, the white stays white
    float k = contrast;
    c = k >= 1.0 ? lerp(c, c * c * (3.0 - 2.0 * c), saturate((k - 1.0) * 2.0))  // an S curve: nothing clips
                 : lerp(float3(0.5, 0.5, 0.5), c, k);
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(float3(l, l, l), c, saturation);
    c *= float3(1.0 + 0.1 * warmth, 1.0 + 0.02 * warmth, 1.0 - 0.1 * warmth);
    float2 q = (pos / size - 0.5) * float2(size.x / size.y, 1.0);
    c *= 1.0 - vignette * smoothstep(0.35, 1.0, length(q) * 1.15) * 0.8;
    return saturate(c);
}

float4 grade_ps(PresentOut i) : SV_Target {
    float3 c = sceneColor.Load(int3(int2(i.pos.xy), 0)).rgb;
    return float4(gradeColour(c, i.pos.xy, uiPixels.xy, sharpen, footprint, twinX, twinY, twinW), 1.0);
}

// 3.0.5-beta.4 (R-DEYFZY, a Radeon 610M: the world at 75 % made larger 6-7 ms, its colours another 0.9): Direct3D 11's
// smaller world made larger by one tap and coloured on the way - the colours' copy and pass spared. r0-r4 the
// colours' saturation, contrast, brightness, warmth and vignette, r5 and r6 the picture's size.
float4 world_grade_ps(PresentOut i) : SV_Target {
    float2 dx = ddx(i.uv);
    float2 dy = ddy(i.uv);
    float3 c = tex0.SampleGrad(samp0, clamp(i.uv, sourceClamp.xy, sourceClamp.zw), dx, dy).rgb;
    return float4(gradeColour(c, i.pos.xy, float2(r5, r6), r0, r1, r2, r3, r4), 1.0);
}

// 2.7.20, the moon's beams (the user: «хочется чтобы было видно сам луч света луны на карте»): broad slanting bands
// of moonlight on the ground as through gaps in the clouds - kept to the map (g its world x and z, ten units a cell),
// drifting slowly (t seconds); 1 in a beam's middle, 0 between the beams
float moonBeam(float2 g, float t) {
    float u = dot(g, float2(0.7071, 0.7071)) / 150.0 + t * 0.012;  // across the beams: one every 15 cells
    float v = dot(g, float2(-0.7071, 0.7071)) / 420.0 - t * 0.004;  // along them
    float band = smoothstep(-0.1, 0.6, sin(6.2832 * (u + 0.15 * sin(6.2832 * v))));  // a little bent
    float gap = smoothstep(-0.8, 0.2, sin(6.2832 * (1.7 * v + 0.37 * u) + 1.3));  // clouds close one here and there
    return band * gap;
}

// 0.63, night: the world from its copy (t2) darker and colder - the bright stays (a fire, a lamp, a spark), and the
// torches' light (after this) shines out of it. sharpen how much (0-1). 0.74: first the time of day's light (twinX,
// twinY, twinW: its tint, 1 none); 0.79: and the region's look - its tint in that too, its saturation 1 + sourceRect.w.
float4 night_ps(PresentOut i) : SV_Target {
    float3 c = sceneColor.Load(int3(int2(i.pos.xy), 0)).rgb * float3(twinX, twinY, twinW);
    float grey = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = max(lerp(float3(grey, grey, grey), c, 1.0 + sourceRect.w), 0.0);
    c += (c * 1.3 + float3(0.10, 0.11, 0.15)) * sourceRect.x;  // 0.80: a flash of lightning
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    float hi = max(c.r, max(c.g, c.b)), lo = min(c.r, min(c.g, c.b));
    float sat = (hi - lo) / max(hi, 1e-3);
    // 2.7.57 (R-ZPEV4V, the guild emblems on the flags grey at night; the user: «днём всё хорошо ... надо как-то
    // продумать, чтобы не портились так сильно картинки»): a vivid colour - an emblem, a flag, a dress, a flower -
    // keeps its hue in the dark (85 % of its colour under a pale silver), darkened as all is; the dull ground, stone
    // and walls as before
    float vivid = smoothstep(0.35, 0.75, sat) * smoothstep(0.08, 0.2, hi);
    // 2.7.16: deeper past the whole night (footprint 0-1, «Глибока ніч»): 45 % of the light down to 25 % (2.7.17, the
    // user's choice: 16 % was black)
    float3 night = lerp(float3(l, l, l), c, lerp(0.55, 0.85, vivid)) *
                   lerp(float3(0.55, 0.68, 1.0), float3(0.80, 0.85, 1.0), vivid) *
                   lerp(0.45, 0.25, saturate(footprint));
    // 2.7.18, moonlight (twinH its strength, 0-2). 2.7.19 (the user: «луна должна светить и карта должна быть яркой
    // от лунного света», 2.7.18's about 30 % of the day's light «не играбельно»): the night's main light - the map
    // bright under it, silver-blue and muted, about 60 % of the day's light (the deep night's about 53 %); never
    // darker than the night without it, nor brighter than the day
    // 2.7.20: the moon's beams where the map is drawn (sourceRect.y their strength, 0-1; sourceRect.z the time; the
    // depth t3, the view's place in the world r0-r11): 1.56 times the moonlight in them, 0.86 between - the map as
    // bright as before on the whole (a fifth of it in the beams)
    float beam = 0.0, lift = 1.0;
    if (sourceRect.y > 0.0) {
        float raw = sceneDepth.Load(int3(int2(i.pos.xy), 0));
        if (raw < 0.99999) {
            float3 v = viewFrom(int2(i.pos.xy), raw);
            float3 w =
                v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) + float3(r9, r10, r11);
            beam = sourceRect.y * moonBeam(w.xz, sourceRect.z);
            lift = 1.0 + 0.7 * beam - 0.14 * sourceRect.y;
        }
    }
    // 2.7.47 (the user: «хотелось бы, чтобы ... в лунном луче цвета нормальными становились»): in a beam's middle the
    // colours come back - 60 % of them, not 35 %, and the silver less blue (as bright: its luminance kept)
    float3 tint = lerp(float3(0.62, 0.74, 1.0), float3(0.662, 0.740, 0.871), beam);
    float3 moon = lerp(float3(l, l, l), c, lerp(0.35 + 0.25 * beam, 0.85, vivid)) *
                  lerp(tint, float3(0.85, 0.90, 1.0), vivid) * lerp(0.82, 0.72, saturate(footprint)) * twinH * lift;
    night = min(max(night, moon), max(c, float3(l, l, l)));
    // 2.7.17: what stays bright is a light - a fire's, a lamp's, a spell's colour - not a white flower, pale hair or
    // stone: under «Глибока ніч» the meadow's flowers stood out of the dark as stars on the ground
    float keep = smoothstep(0.72, 0.97, l) * smoothstep(0.12, 0.35, sat);
    return float4(lerp(c, lerp(night, c, keep), sharpen), 1.0);
}

// 0.78, bloom (ao.cpp applyBloom): the picture's bright parts at a quarter of its size - over footprint, the brighter
// the more - blurred by dof_blur_ps, then added onto it (bloom_apply_ps: sharpen how much). 2.6.11: the filmic curve
// over all (the highlights rolled off, an S-curve) gone at the player's word.
float4 bloom_down_ps(PresentOut i) : SV_Target {
    float2 c = i.pos.xy * 4.0;
    float2 t = 1.0 / uiPixels.xy;
    float3 s = sceneColor.SampleLevel(samp0, (c + float2(-1.0, -1.0)) * t, 0).rgb +
               sceneColor.SampleLevel(samp0, (c + float2(1.0, -1.0)) * t, 0).rgb +
               sceneColor.SampleLevel(samp0, (c + float2(-1.0, 1.0)) * t, 0).rgb +
               sceneColor.SampleLevel(samp0, (c + float2(1.0, 1.0)) * t, 0).rgb;
    s *= 0.25;
    float l = max(s.r, max(s.g, s.b));
    float over = max(l - footprint, 0.0) / max(1.0 - footprint, 1e-3);
    return float4(s * (over * over / max(l, 1e-3)), 1.0);
}

float4 bloom_apply_ps(PresentOut i) : SV_Target {
    int2 p = int2(i.pos.xy);
    float3 c = sceneColor.Load(int3(p, 0)).rgb;
    float3 glow = tex0.SampleLevel(samp0, i.pos.xy * 0.25 / uiPixels.zw, 0).rgb;
    c += glow * sharpen;
    // 0.83: the world coming in from black after a new map (twinH, 0-1); 2.6.11: the film grain gone
    return float4(saturate(c) * twinH, 1.0);
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

// 0.75, TAA (device.cpp applyTaa) for any graphics card: the world (t0, drawn with a sub-pixel jitter at the
// picture's size) blended into the last frame's result (t2) fetched where each pixel was (t1, the motion), that
// history first held within the colours around the pixel (a box of their mean and spread in YCoCg) so that what moved
// or appeared leaves no trail. twinX 1: no history (a jump, a new size); twinY 1: t3 holds the characters' and the
// effects' mask - they move on their own, so less of their history is taken. uiPixels.xy the size.
float3 toYCoCg(float3 c) {
    return float3(dot(c, float3(0.25, 0.5, 0.25)), dot(c, float3(0.5, 0.0, -0.5)), dot(c, float3(-0.25, 0.5, -0.25)));
}
float3 fromYCoCg(float3 c) { return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }

float4 taa_ps(PresentOut i) : SV_Target {
    int2 p = int2(i.pos.xy);
    int2 last = int2(uiPixels.xy) - 1;
    float3 now = tex0.Load(int3(p, 0)).rgb;
    float3 mean = float3(0.0, 0.0, 0.0), square = float3(0.0, 0.0, 0.0);
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x) {
            float3 s = toYCoCg(tex0.Load(int3(clamp(p + int2(x, y), int2(0, 0), last), 0)).rgb);
            mean += s;
            square += s * s;
        }
    mean /= 9.0;
    float3 spread = sqrt(max(square / 9.0 - mean * mean, 0.0));
    float2 was = float2(p) + 0.5 + tex1.Load(int3(p, 0)).rg;
    bool outside = any(was < 0.0) || any(was > uiPixels.xy);
    float3 history = sceneColor.SampleLevel(samp0, was / uiPixels.xy, 0).rgb;
    history = fromYCoCg(clamp(toYCoCg(history), mean - 1.25 * spread, mean + 1.25 * spread));
    float w = twinX > 0.5 || outside ? 1.0 : 0.1;
    if (twinY > 0.5) w = max(w, lerp(0.1, 0.35, sceneDepth.Load(int3(p, 0))));
    return float4(lerp(history, now, w), 1.0);
}

// 0.22.3: the world's alpha - the coverage of its characters and effects - as DLSS's mask.
float mask_ps(PresentOut i) : SV_Target { return tex0.Load(int3(int2(i.pos.xy), 0)).a; }

float4 water_ps(WaterIn i) : SV_Target { return water(i); }

GlowOut water_glow_ps(WaterIn i) {
    GlowOut o;
    o.c = water(i);
    o.g = float4(0.0, 0.0, 0.0, o.c.a);  // the water covers the light under it
    return o;
}

// 2.7.31 (the tester in Prontera's sewers: «стены от огня очень гладкие»): a surface under a fire's light - its
// brightness lifted on a curve that keeps every step of it (the darks the most, four times at the bottom, never past
// white) and its own colour a third under the light's. 0.23.3's lift - the darks 3.4 times, the mid-tones up to 0.9 -
// pressed the stones' faces into one even orange (a lighter stone came out darker than a duller one: 0.5 -> 0.86, 0.7
// -> 0.7), and the light's colour took the surface's whole.
// 2.7.47 (R-PH6H7S, the Wizard by a Fire Wall at deep night: «скин персонажа просто поменял цвет»; the user: «хотелось
// бы, чтобы когда появляется свет, то цвета у персонажа становились нормальные»): in its own colours - the picture's
// before the night (day; fogColor.rgb the time of day's and the region's tint night_ps gave it, fogColor.a the
// region's saturation, sourceClamp.x the lightning) warmed by half the light's colour. 2.7.31 took a third of the hue
// from the night's grey-blue and the rest from the light: a dark robe came out pale pink, the grass by a fire brown.
// Under the open sky at night (twinH) no brighter than by day, a tenth over at most: the darks lifted fourfold out of
// the night flattened a figure.
float3 litSurface(float3 day, float lum, float3 colour) {
    const float3 luma = float3(0.2126, 0.7152, 0.0722);
    float3 base = day * fogColor.rgb;
    float g = dot(base, luma);
    base = max(lerp(float3(g, g, g), base, 1.0 + fogColor.a), 0.0);
    base += (base * 1.3 + float3(0.10, 0.11, 0.15)) * sourceClamp.x;
    float b = dot(base, luma);
    float target = lum * 4.0 / (1.0 + lum * 3.0);
    target = lerp(target, min(target, b * 1.1), twinH);
    float3 hue = base / max(b, 1e-3) * lerp(float3(1.0, 1.0, 1.0), colour / max(dot(colour, luma), 0.05), 0.5);
    return hue * (target / max(dot(hue, luma), 1e-3));
}

// 0.23, the torches (torches.cpp). Their light, from the picture's copy (t2) and its depth (t3) into the picture -
// the pixels it reaches only: t1 holds two texels a light (0.25: the torches' and the lamps'), its place in the view
// (xyz) and (0.39.2) reach (w, world units), then its colour and strength (w: with its flicker and the game's
// fade); footprint is how many, uiPixels.xy the world's size. 0.23.3: bounded ("everything white" around torches
// on bright ground, two side by side) - the light takes a surface over towards a coloured, brighter version of
// itself, never past white, the more the darker the surface; several lights saturate instead of adding up; what the
// sun lights already a light hardly changes. Under it the map's tint (the caves' purple) gives way to the light's
// colour.
float4 torch_light_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) discard;  // nothing drawn there
    float3 p = viewAt(wp);
    int2 last = int2(uiPixels.xy) - 1;
    float3 l = viewAt(max(wp - int2(1, 0), int2(0, 0))), r = viewAt(min(wp + int2(1, 0), last));
    float3 u = viewAt(max(wp - int2(0, 1), int2(0, 0))), d = viewAt(min(wp + int2(0, 1), last));
    float3 dx = abs(r.z - p.z) < abs(p.z - l.z) ? r - p : p - l;
    float3 dy = abs(d.z - p.z) < abs(p.z - u.z) ? d - p : p - u;
    float3 n = normalize(cross(dy, dx));
    if (dot(n, p) > 0.0) n = -n;  // towards the camera
    // 0.79, round characters (0.84: their own switch, sourceRect.x): something narrow in front of what is behind it - a
    // character, a monster, a post - is lit as if round across: its normal turns towards the nearer of its edges.
    if (sourceRect.x > 0.5) {
        float stepPx = max(1.0, scale * 1.5);
        float dl = 0.0, dr = 0.0;
        [loop] for (int k = 1; k <= 16 && (dl == 0.0 || dr == 0.0); ++k) {
            int2 o = int2(int(round(float(k) * stepPx)), 0);
            if (dl == 0.0 && viewDepth(sceneDepth.Load(int3(max(wp - o, int2(0, 0)), 0))) - p.z > 4.0) dl = float(k);
            if (dr == 0.0 && viewDepth(sceneDepth.Load(int3(min(wp + o, last), 0))) - p.z > 4.0) dr = float(k);
        }
        if (dl > 0.0 && dr > 0.0) {
            float across = (dl - dr) / (dl + dr);  // -1 at the left edge, 1 at the right
            n = normalize(n + float3(across * 1.3, 0.0, 0.0) * length(n));
        }
    }
    float light = 0.0;
    float3 colour = float3(0.0, 0.0, 0.0);
    int count = int(footprint);
    [loop] for (int k = 0; k < count; ++k) {
        float4 t = tex1.Load(int3(2 * k, 0, 0));  // 0.39.2: its place and reach; its colour and strength only if it
        float3 v = t.xyz - p;                      // reaches here
        float d2 = dot(v, v), reach2 = t.w * t.w;
        if (d2 >= reach2) continue;
        float4 c = tex1.Load(int3(2 * k + 1, 0, 0));
        float strength = abs(c.w), f = 1.0 - d2 / reach2;  // 2.7.24: negative a ground light's (below)
        float facing = saturate(dot(n, v * rsqrt(max(d2, 1e-4))) * 0.7 + 0.3);  // wrapped: soft on what turns away
        float a = strength * f * f / (1.0 + d2 / (0.2 * reach2)) * facing;  // brighter near the source
        // 0.68, rim light (sharpen: its strength): the flat characters and the models' outlines lit more on the side
        // that faces the light - where the depth a few pixels towards the light on the screen is far behind.
        if (sharpen > 0.0) {
            float2 sd = float2(f0 * (v.x * p.z - p.x * v.z), f1 * (v.y * p.z - p.y * v.z));
            float sl = length(sd);
            if (sl > 1e-4) {
                int2 q = clamp(wp + int2(round(sd / sl * 3.0 * scale)), int2(0, 0), last);
                float gap = viewDepth(sceneDepth.Load(int3(q, 0))) - p.z;
                a += strength * f * smoothstep(3.0, 12.0, gap) * sharpen;
            }
        }
        // 2.7.24: a ground light (an orb of light's, its strength negative) lights what lies near the ground alone -
        // the ground effect_light::kGroundLight (20) under it, the world's up -(r1, r4, r7): not the orb's own sprites
        // in the air, whose depth the game writes over their whole soft discs
        if (c.w < 0.0) a *= 1.0 - smoothstep(3.0, 7.0, dot(p - t.xyz, -float3(r1, r4, r7)) + 20.0);
        light += a;
        colour += a * c.rgb;
    }
    float3 sc = sceneColor.Load(int3(wp, 0)).rgb;
    const float3 luma = float3(0.2126, 0.7152, 0.0722);
    float lum = dot(sc, luma);
    // 2.7.47: at night under the open sky (twinH) a light surface too - the night's grey-blue gone from it
    float w = (1.0 - exp(-2.4 * light)) * (1.0 - 0.6 * smoothstep(0.3, 0.8, lum) * (1.0 - twinH));
    if (w < 0.004) discard;
    colour /= max(light, 1e-4);
    float3 warm = litSurface(tex0.Load(int3(wp, 0)).rgb, lum, colour);  // 2.7.47: t0 the picture before the night
    // 2.5.17: a triangular noise of a step either way against the rings an 8-bit picture shows in a wide soft light
    // (a skill's fire round a character, R-HNEYBN)
    float3 lit = lerp(sc, saturate(warm), w * 0.9);
    return float4(lit + (hash12(i.pos.xy) + hash12(i.pos.xy + 17.3) - 1.0) / 255.0, 1.0);
}

// 2.5: the same light in two passes instead of one over every pixel of the lights' union: each light's own quad on
// the screen (torch_light_vs, from its texels) adds what it gives each pixel into a buffer of light (torch_accum_ps:
// rgb its colour times the amount, a the amount), then one pass turns the sums into the picture (torch_apply_ps) -
// the arithmetic of torch_light_ps, but a pixel's work is its own lights', not all of them, and a pixel no light
// reaches costs nothing (a tower's sixty torches cost 3-7 ms at 1080p). uiPixels.xy the buffer's size, the world's.
struct TorchIn {
    float4 pos : SV_Position;
    nointerpolation uint light : TEXCOORD1;
};

TorchIn torch_light_vs(uint id : SV_VertexID, uint inst : SV_InstanceID) {
    float4 t = tex1.Load(int3(2 * int(inst), 0, 0));  // its place in the view and its reach
    float2 lo = float2(0.0, 0.0), hi = uiPixels.xy;    // the whole buffer: the camera within its reach
    if (t.z - t.w > nearZ) {  // the cube about it: its near face's corners projected bound the rest of it
        float z = t.z - t.w;
        float2 a = float2((t.x - t.w) / z * f0 + cx, (t.y - t.w) / z * f1 + cy) * scale + float2(jitterX, jitterY);
        float2 b = float2((t.x + t.w) / z * f0 + cx, (t.y + t.w) / z * f1 + cy) * scale + float2(jitterX, jitterY);
        lo = clamp(min(a, b) - 1.0, float2(0.0, 0.0), uiPixels.xy);
        hi = clamp(max(a, b) + 1.0, float2(0.0, 0.0), uiPixels.xy);
    }
    uint corner = id == 3 ? 2 : id == 4 ? 1 : id == 5 ? 3 : id;  // two triangles: 0 1 2, 2 1 3
    float2 p = float2((corner & 1) != 0 ? hi.x : lo.x, (corner & 2) != 0 ? hi.y : lo.y);
    TorchIn o;
    o.pos = float4(p.x / uiPixels.x * 2.0 - 1.0, 1.0 - p.y / uiPixels.y * 2.0, 0.0, 1.0);
    o.light = inst;
    return o;
}

float4 torch_accum_ps(TorchIn i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) discard;  // nothing drawn there
    float3 p = viewAt(wp);
    float4 t = tex1.Load(int3(2 * int(i.light), 0, 0));
    float3 v = t.xyz - p;
    float d2 = dot(v, v), reach2 = t.w * t.w;
    if (d2 >= reach2) discard;
    float4 c = tex1.Load(int3(2 * int(i.light) + 1, 0, 0));  // its colour and strength (2.7.24: < 0 a ground light's)
    float strength = abs(c.w);
    int2 last = int2(uiPixels.xy) - 1;
    float3 l = viewAt(max(wp - int2(1, 0), int2(0, 0))), r = viewAt(min(wp + int2(1, 0), last));
    float3 u = viewAt(max(wp - int2(0, 1), int2(0, 0))), d = viewAt(min(wp + int2(0, 1), last));
    float3 dx = abs(r.z - p.z) < abs(p.z - l.z) ? r - p : p - l;
    float3 dy = abs(d.z - p.z) < abs(p.z - u.z) ? d - p : p - u;
    float3 n = normalize(cross(dy, dx));
    if (dot(n, p) > 0.0) n = -n;  // towards the camera
    if (sourceRect.x > 0.5) {  // 0.79, round characters (torch_light_ps)
        float stepPx = max(1.0, scale * 1.5);
        float dl = 0.0, dr = 0.0;
        [loop] for (int k = 1; k <= 16 && (dl == 0.0 || dr == 0.0); ++k) {
            int2 o = int2(int(round(float(k) * stepPx)), 0);
            if (dl == 0.0 && viewDepth(sceneDepth.Load(int3(max(wp - o, int2(0, 0)), 0))) - p.z > 4.0) dl = float(k);
            if (dr == 0.0 && viewDepth(sceneDepth.Load(int3(min(wp + o, last), 0))) - p.z > 4.0) dr = float(k);
        }
        if (dl > 0.0 && dr > 0.0) {
            float across = (dl - dr) / (dl + dr);
            n = normalize(n + float3(across * 1.3, 0.0, 0.0) * length(n));
        }
    }
    float f = 1.0 - d2 / reach2;
    float facing = saturate(dot(n, v * rsqrt(max(d2, 1e-4))) * 0.7 + 0.3);
    float a = strength * f * f / (1.0 + d2 / (0.2 * reach2)) * facing;
    if (sharpen > 0.0) {  // 0.68, rim light (torch_light_ps)
        float2 sd = float2(f0 * (v.x * p.z - p.x * v.z), f1 * (v.y * p.z - p.y * v.z));
        float sl = length(sd);
        if (sl > 1e-4) {
            int2 q = clamp(wp + int2(round(sd / sl * 3.0 * scale)), int2(0, 0), last);
            float gap = viewDepth(sceneDepth.Load(int3(q, 0))) - p.z;
            a += strength * f * smoothstep(3.0, 12.0, gap) * sharpen;
        }
    }
    if (c.w < 0.0) a *= 1.0 - smoothstep(3.0, 7.0, dot(p - t.xyz, -float3(r1, r4, r7)) + 20.0);  // 2.7.24: the ground's
    return float4(c.rgb * a, a);
}

// The buffer's sums (tex0) into the picture's copy (t2), as torch_light_ps ended; 2.7.47: tex1 the picture before the
// night, the lit surfaces' own colours.
float4 torch_apply_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    float4 sum = tex0.Load(int3(wp, 0));
    float light = sum.a;
    if (light <= 0.0) discard;
    float3 sc = sceneColor.Load(int3(wp, 0)).rgb;
    const float3 luma = float3(0.2126, 0.7152, 0.0722);
    float lum = dot(sc, luma);
    float w = (1.0 - exp(-2.4 * light)) * (1.0 - 0.6 * smoothstep(0.3, 0.8, lum) * (1.0 - twinH));  // 2.7.47
    if (w < 0.004) discard;
    float3 colour = sum.rgb / max(light, 1e-4);
    float3 warm = litSurface(tex1.Load(int3(wp, 0)).rgb, lum, colour);  // 2.7.47: t1 the picture before the night
    // 2.5.17: a triangular noise of a step either way against the rings an 8-bit picture shows in a wide soft light
    // (a skill's fire round a character, R-HNEYBN)
    float3 lit = lerp(sc, saturate(warm), w * 0.9);
    return float4(lit + (hash12(i.pos.xy) + hash12(i.pos.xy + 17.3) - 1.0) / 255.0, 1.0);
}

// 2.7.54, the map warps (effect 321, draw.cpp; core/portal.hpp partOf): the game's own draws of a warp shaded anew
// where it draws them. 2.7.56 (the tester on 2.7.55, the game's pictures averaged: «новый и старый порталы не
// отличаются совсем»; on 2.7.54's lilac walls: «аккуратнее ... более похожим на оригинал»): the game's ring_blue.tga
// (2.7.55's record of it) drawn anew at full resolution - its white band at the ground and its crown of light spikes
// rising up the wall and turning blue as they fade, every edge smooth (the game's: 64 texels round a whole ring, 16
// steps of alpha), the spikes breathing slowly; the floor's ramp (alpha_down.tga: alpha 0.93 x v) without its 16
// steps, a little less (80 %); the sparks stay the game's. uv0.y 1 on the ground, uv0.x round the ring in 21sts.
// sourceRect.w the part (1 a ring, 2 the floor); twinH the time; footprint the strength (1 = 100 %).
float warpHash(float i, float s) { return frac(sin(i * 12.9898 + s * 78.233) * 43758.5453); }

// n spikes round the ring (phi: turns), w0 wide at the root (of a spike's share of the turn), each hmin-hmax of the
// wall high; 1 in a spike at its root, fading to its tip
float warpSpikes(float phi, float y, float n, float w0, float hmin, float hmax, float seed, float aa) {
    float x = phi * n;
    float cell = floor(x);
    float id = cell - n * floor(cell / n);  // round the ring: the last and the first are one (no seam where it closes)
    float local = x - cell - 0.5 - (warpHash(id, seed) - 0.5) * 0.5;
    float h = lerp(hmin, hmax, warpHash(id, seed + 1.7));
    h *= 0.88 + 0.12 * sin(twinH * (1.1 + warpHash(id, seed + 3.1)) + 6.28 * warpHash(id, seed + 5.3));
    float along = saturate(1.0 - y / max(h, 1e-3));
    float soft = aa * n * 1.2 + 0.035;  // soft as light, a pixel at least
    return smoothstep(-soft, soft, w0 * along - abs(local)) * along;
}

float4 warpShade(PsIn i) {
    float v = saturate(i.uv0.y);
    float y = 1.0 - v;                            // 0 at the ground, 1 at the wall's top
    float phi = i.uv0.x * (21.0 / 20.0);          // turns round the ring: whole ones, no seam
    float dy = max(fwidth(y), 1e-4), dphi = fwidth(phi);  // the slopes first, outside any choice
    float fog = fogEnable != 0 ? i.specular.a : 1.0;
    float strength = max(footprint, 0.0);
    // a ring, added: its band and its spikes, white turning blue up the wall, the game's tint and fade
    float band = 1.0 - smoothstep(0.24 - dy, 0.24 + dy, y);
    float spikes = max(max(warpSpikes(phi, y, 13.0, 0.26, 0.45, 0.85, 1.0, dphi),
                           warpSpikes(phi, y, 31.0, 0.28, 0.28, 0.55, 7.0, dphi) * 0.8),
                       warpSpikes(phi, y, 67.0, 0.24, 0.20, 0.42, 13.0, dphi) * 0.6);
    float3 light = lerp(float3(1.0, 1.0, 1.0), float3(0.15, 0.55, 1.0), smoothstep(0.1, 0.6, y));
    float3 ring = i.diffuse.rgb * light * (max(band, spikes) * i.diffuse.a) * strength * fog;
    // the floor, laid over: the game's ramp, smooth
    float3 floorColour = fogEnable != 0 ? lerp(fogColor.rgb, i.diffuse.rgb, i.specular.a) : i.diffuse.rgb;
    float floorAlpha = saturate(v * 0.93 * i.diffuse.a * 0.8 * min(strength, 1.25));
    return sourceRect.w < 1.5 ? float4(ring, 1.0) : float4(floorColour, floorAlpha);
}

float4 portal_draw_ps(PsIn i) : SV_Target { return warpShade(i); }

GlowOut portal_draw_glow_ps(PsIn i) {  // the rings' light into the glow; the floor hides what it covers
    GlowOut o;
    o.c = warpShade(i);
    o.g = sourceRect.w > 1.5 ? float4(0.0, 0.0, 0.0, o.c.a) : o.c;
    return o;
}

float fbm4(float2 p) {
    float s = 0.0, a = 0.5;
    [unroll] for (int k = 0; k < 4; ++k) {
        s += a * valueNoise(p);
        p = p * 2.03 + float2(1.7, -0.9);
        a *= 0.5;
    }
    return s;
}

// A flame at q (x across, its half width 1; y up from its base 0 to its tip 1), premultiplied, and a few sparks rising
// from it. hw and h are its half width and height in pixels (the sparks' size), seed its own pattern (0-1), flick its
// flicker (about 1). 2.6.16: "alive", the player's pick of four (R-N3ZZ3F: the fire "not very beautiful" - a smooth
// candle drop): a wide drop that keeps its width far up, its top torn by the noise rising through it into licking
// tongues and flamelets, coloured as heat glows - dark red, red, orange, yellow, near white at the heart (1.5c, 1.5c^3,
// c^6). Each torch its own ("several fires burn alike, repeating each other"): its seed sets its pace, height, lean
// and moment, and shifts the noise.
float3 torchFlame(float2 q, float t, float seed, float flick, float hw, float h) {
    float pace = 0.85 + 0.3 * frac(seed * 7.13);
    float tall = 0.92 + 0.16 * frac(seed * 3.71);
    float lean = (frac(seed * 5.37) - 0.5) * 0.12;
    float tt = t * pace + seed * 37.0;
    float y = q.y / ((0.95 + 0.45 * (flick - 1.0)) * tall);  // a brighter moment burns taller
    float sway = sin(tt * 2.1 + seed * 4.0) * 0.05 + sin(tt * 4.7 + seed * 1.3) * 0.025;
    float bend = (fbm4(float2(q.x * 0.8 + seed * 3.1, y * 1.1 - tt * 1.3)) - 0.47) * 0.6 * y;
    float x = q.x + (sway + lean) * y + bend;
    float wid = pow(saturate(1.0 - y * 0.72), 0.42) * smoothstep(-0.12, 0.2, y);
    float env = saturate(1.0 - abs(x) / max(wid, 1e-3));
    float n = fbm4(float2(x * 3.4 + seed * 5.0, y * 2.1 - tt * 3.1)) * 0.65 +
              fbm4(float2(x * 7.14 - seed, y * 4.4 - tt * 4.8)) * 0.35;
    float fire = env * 1.75 - n * (0.45 + 2.0 * y) + 0.1;
    float c1 = saturate(fire) * (1.0 - 0.32 * saturate(y)) * smoothstep(-0.06, 0.05, q.y);
    float3 o = float3(1.5 * c1, 1.5 * c1 * c1 * c1, pow(c1, 6.0)) * (1.0 + 0.5 * (flick - 1.0));
    t *= 0.75;  // the sparks' clock, as it was
    float size = max(h / 160.0, 0.35);
    [unroll] for (int k = 0; k < 4; ++k) {  // sparks: each rises, drifts and dies out, then starts again elsewhere
        float phase = t * (0.45 + 0.11 * float(k)) + seed * 3.7 + float(k) * 0.29;
        float life = frac(phase);
        float sx = (hash12(float2(seed * 13.0 + float(k), floor(phase))) - 0.5) * 0.9 +
                   sin(life * 9.0 + float(k)) * 0.12;
        float2 d = (q - float2(sx, 0.55 + life * 1.1)) * float2(hw, h);
        float e = exp(-dot(d, d) / (2.2 * size * size)) * (1.0 - life) * (1.0 - life);
        o += float3(1.0, 0.55, 0.15) * (e * 1.1);
    }
    return o;
}

// A torch's flame and the soft glow around it, over the picture (premultiplied: blend ONE, INVSRCALPHA - the glow
// adds light, the flame hides some of what is behind it; under DLSS its alpha marks it in the mask). sourceRect.xy its
// base and .zw base to tip in target pixels, sourceClamp.x its half width in target pixels, .y its view depth, .z how
// many of the game's effects it stands for (2.6.18: a map torch's two, core/torch.hpp), .w its seed (0-1);
// uiPixels.xy the world's size, .z its flicker, .w the time (seconds); twinX the glow's strength, twinY the
// game's fade, twinW the flame's brightness (0: the game draws its own), fogColor its colour. Hidden behind what stands
// in front of it - a character, the bowl's rim - by the picture's depth (t3).
float4 torch_fire_ps(PresentOut i) : SV_Target {
    float2 pos = i.pos.xy;
    float h = max(length(sourceRect.zw), 1.0);
    float2 up = sourceRect.zw / h;
    float2 rel = pos - sourceRect.xy;
    float2 q = float2(dot(rel, float2(-up.y, up.x)) / max(sourceClamp.x, 1.0), dot(rel, up) / h);
    float flick = uiPixels.z;
    float flameZ = sourceClamp.y;
    float z = viewDepth(sceneDepth.Load(int3(int2(pos), 0)));
    float seen = saturate((z - flameZ + 2.0) / 2.0);
    float2 mid = sourceRect.xy + sourceRect.zw * 0.38;
    // The glow: as much as the fire is seen, and not on what stands well in front of it.
    int2 m = int2(clamp(mid, float2(0.0, 0.0), uiPixels.xy - 1.0));
    float midSeen = saturate((viewDepth(sceneDepth.Load(int3(m, 0))) - flameZ + 3.0) / 3.0);
    float r2 = dot(pos - mid, pos - mid) / (h * h);
    float fade = saturate(1.0 - r2 / 4.84);  // none at 2.2 heights, the edge of its pass
    // 0.25.2: half of 0.23.3's - as the game's torch looks while it comes up from its fade, which the player liked.
    float glow = (0.105 * exp(-r2 / 0.16) + 0.045 / (1.0 + r2 / 0.5)) * fade * fade;  // 0.25.4: three quarters
    glow *= (1.0 + (flick - 1.0) * 0.5) * twinX * twinY * midSeen * saturate((z - flameZ + 15.0) / 15.0);
    // 0.23.3: on bright ground the glow only washes it out - less of it there.
    float under = dot(sceneColor.Load(int3(int2(pos), 0)).rgb, float3(0.2126, 0.7152, 0.0722));
    glow *= 1.0 - 0.6 * smoothstep(0.3, 0.9, under);
    float3 f = twinW > 0.0 ? torchFlame(q, uiPixels.w, sourceClamp.w, flick, sourceClamp.x, h) * (seen * twinY * twinW)
                           : float3(0.0, 0.0, 0.0);
    // 0.25: a torch the game does not show burns in its own colour (fogColor; white: the fire's own).
    float3 tint = fogColor.rgb;
    bool coloured = any(tint < 0.99);
    if (coloured) f = lerp(f, tint * dot(f, float3(0.3, 0.5, 0.2)) * 1.8, 0.85);
    float3 glowColour = coloured ? tint * 0.8 : float3(1.0, 0.5, 0.18);
    float a = saturate(max(f.r, max(f.g, f.b)) * 0.45);
    float3 c = glowColour * glow * (1.0 - a) + f;
    // 2.6.18: as two of it over each other (premultiplied: c + (1 - a) c, 1 - (1 - a)^2) - the look the torches had
    // while each of the game's two effects of a torch drew its own
    return sourceClamp.z > 1.5 ? float4(c * (2.0 - a), a * (2.0 - a)) : float4(c, a);
}

// 0.24, the chimneys' smoke (draw.cpp): a puff the game sends up, drawn as the game would (its colour, fade and fog:
// shade) with billowing inside - wisps, curls and gaps that rise and churn. The noise lies in the world, at each
// pixel's place on the puff (a billboard at its view depth, twinX): the puffs that overlap show the same wisps, so the
// column reads as one body of smoke rather than a stack of the same picture. r0-r11 the camera backwards (or the view
// itself when it is not known), f0-farZ the projection, twinH the time (seconds), footprint the density (1 = 100 %).
float4 smokeShade(PsIn i) {
    float4 c = shade(i);
    float2 g = (i.pos.xy - 0.5 - float2(jitterX, jitterY)) / scale;
    float z = twinX;
    float3 v = float3((g.x - cx) * z / f0, (g.y - cy) * z / f1, z);
    float3 w = v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    float t = twinH;
    float2 p = float2(w.x + w.z * 0.7, w.y) * 0.1 + float2(0.0, t * 0.3);  // up is -y: the wisps rise
    float2 warp = float2(fbm4(p + float2(0.0, t * 0.1)), fbm4(p + float2(5.2 + t * 0.05, 1.3)));
    float n = fbm4(p * 1.4 + warp * 1.5);
    c.a *= saturate(smoothstep(0.22, 0.66, n) * 1.6 * footprint);
    c.rgb *= 0.78 + 0.4 * n;  // the thick folds lighter, the thin ones darker: a little depth
    return c;
}

float4 smoke_ps(PsIn i) : SV_Target { return smokeShade(i); }

// 0.30, the forest's light (draw.cpp): a face of one of its beams - five-sided tubes from above to the ground - drawn
// as the game would (its picture, colour, fade and fog: shade) with soft ends, brighter along the face's middle, and
// light shimmering as through moving leaves. The noise lies in the world at the pixel's place on the beam (its depth:
// i.pos.w), so the tube's sides show one pattern as the camera turns. r0-r11 the camera backwards, f0-farZ the
// projection, twinH the time, footprint the brightness (1 = 100 %).
float4 rayShade(PsIn i) {
    float4 c = shade(i);
    float v = saturate(i.uv0.y), u = saturate(i.uv0.x);
    float ends = smoothstep(0.0, 0.22, v) * (1.0 - smoothstep(0.62, 1.0, v));
    float across = 0.55 + 0.45 * sin(u * 3.14159);
    float2 g = (i.pos.xy - 0.5 - float2(jitterX, jitterY)) / scale;
    float z = i.pos.w;
    float3 p = float3((g.x - cx) * z / f0, (g.y - cy) * z / f1, z);
    float3 w = p.x * float3(r0, r1, r2) + p.y * float3(r3, r4, r5) + p.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    float t = twinH;
    float leaves = fbm4(float2(w.x * 0.07 + t * 0.06, w.z * 0.07 - t * 0.045) + float2(w.y * 0.015, w.y * -0.01));
    float shimmer = 0.5 + 1.0 * smoothstep(0.3, 0.78, leaves);
    c.rgb *= ends * across * shimmer * footprint;
    return c;
}

float4 ray_ps(PsIn i) : SV_Target { return rayShade(i); }

// 0.31, lava (draw.cpp): a cell of it - the game's own picture in the map's light, drifting slowly as a crust over a
// glowing flow: the cracks between its plates glow and pulse, the crust is darker; the glow is its own (the map's
// light does not dim it) and goes into the glow target. The noise lies in the world at the pixel's place (its depth
// i.pos.w, r0-r11 the camera backwards, f0-farZ the projection), twinH the time, footprint the brightness (1 = 100 %).
float3 lavaShade(PsIn i, out float glow) {
    float2 g = (i.pos.xy - 0.5 - float2(jitterX, jitterY)) / scale;
    float z = i.pos.w;
    float3 p = float3((g.x - cx) * z / f0, (g.y - cy) * z / f1, z);
    float3 w = p.x * float3(r0, r1, r2) + p.y * float3(r3, r4, r5) + p.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    float t = twinH;
    float2 q = w.xz * 0.045;
    float2 flow = float2(t * 0.012, t * 0.008);
    float2 warp = float2(fbm4(q * 1.3 + flow + float2(3.1, 0.0)), fbm4(q * 1.3 - flow + float2(0.0, 7.7)));
    float plates = fbm4(q * 2.2 + warp * 1.6 + flow * 2.0);
    float crack = 1.0 - smoothstep(0.02, 0.12, abs(plates - 0.5));
    float pulse = 0.75 + 0.25 * sin(t * 1.3 + plates * 9.0);
    float4 tex = tex0.Sample(samp0, i.uv0 + (warp - 0.5) * 0.06);  // the game's lava, drifting
    float heat = saturate(dot(tex.rgb, float3(0.5, 0.35, 0.15)) * 1.1 + crack * pulse * 0.8 - 0.15);
    const float3 coal = float3(0.08, 0.02, 0.01), red = float3(0.6, 0.09, 0.02), orange = float3(1.0, 0.42, 0.06);
    const float3 yellow = float3(1.0, 0.85, 0.45);
    float3 ramp = heat < 0.4    ? lerp(coal, red, heat / 0.4)
                  : heat < 0.75 ? lerp(red, orange, (heat - 0.4) / 0.35)
                                : lerp(orange, yellow, (heat - 0.75) / 0.25);
    float emissive = smoothstep(0.45, 1.0, heat);
    float3 lit = tex.rgb * i.diffuse.rgb;
    float amount = saturate(footprint);
    float3 c = lerp(lit, ramp, 0.65 * amount) + ramp * emissive * 0.8 * footprint;
    if (fogEnable != 0) c = lerp(fogColor.rgb, c, i.specular.a);
    glow = emissive * footprint;
    return c;
}

float4 lava_ps(PsIn i) : SV_Target {
    float glow;
    return float4(lavaShade(i, glow), 1.0);
}

// 0.32, the characters' shadows (actor_shadows.cpp). A layer's shadow into the half-sized mask: its picture's alpha
// times its own, the darkest where several fall (blend MAX); none where the ground there is hidden by something nearer
// than the character's feet - the world's depth (t3, uiPixels.xy its size, twinX the mask's pixels to its).
struct ShadowIn {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float2 az : TEXCOORD1;    // the layer's alpha, the feet's view depth
    float2 feet : TEXCOORD2;  // 0.39.1: where they stand, world pixels
};

ShadowIn shadow_vs(float4 pos : POSITION, float2 uv : TEXCOORD0, float2 az : TEXCOORD1, float2 feet : TEXCOORD2) {
    ShadowIn o;
    o.pos = pos;
    o.uv = uv;
    o.az = az;
    o.feet = feet;
    return o;
}

float4 shadow_ps(ShadowIn i) : SV_Target {
    int2 last = int2(uiPixels.xy) - 1;
    int2 wp = min(int2(i.pos.xy * twinX), last);
    // 0.39.1: the ground's depth here, from the feet's along the ground's slope under them on the screen (nothing but
    // the ground is drawn yet there): the shadow lies nearer the camera than the feet, on the ground - only what
    // stands well in front of the ground hides it. (0.32 hid all of it nearer than the feet.)
    float step = 12.0 * scale;
    float z0 = viewDepth(sceneDepth.Load(int3(clamp(int2(i.feet), int2(0, 0), last), 0)));
    float z1 = viewDepth(sceneDepth.Load(int3(clamp(int2(i.feet + float2(0.0, step)), int2(0, 0), last), 0)));
    float slope = clamp((z1 - z0) / step, -0.6, 0.6);
    float expected = i.az.y + (float(wp.y) - i.feet.y) * slope;
    // 2.6.3 (actor_shadows::groundDepth): never deeper than the flat ground through the feet (r1, r4, r7 the world's
    // up in view space; none without the camera) - that depth below the feet fell on the wall below a ledge or on a
    // post in front, and the shadow falling there was cut: it blinked as a character walked along a ledge
    float3 up = float3(r1, r4, r7);
    if (dot(up, up) > 0.5) {
        float2 g = (i.feet - 0.5) / max(scale, 1e-3), q = float2(wp) / max(scale, 1e-3);  // game pixels
        float3 ray = float3((q.x - cx) / max(f0, 1e-3), (q.y - cy) / max(f1, 1e-3), 1.0);
        float3 at = float3((g.x - cx) * i.az.y / max(f0, 1e-3), (g.y - cy) * i.az.y / max(f1, 1e-3), i.az.y);
        float across = dot(up, ray);
        float level = abs(across) < 1e-4 ? -1.0 : dot(up, at) / across;
        if (level > 0.0) expected = min(expected, level);  // past the horizon: none
    }
    float z = viewDepth(sceneDepth.Load(int3(wp, 0)));
    if (z < expected - 6.0) discard;  // a wall, a tree in front of the ground there
    float a = saturate(tex0.Sample(samp0, i.uv).a * i.az.x);
    return float4(a, a, a, a);
}

// 0.34, haze and fog (haze.cpp) - 2.6.8: the far haze and the dungeons' dark and low fog gone at the player's word,
// the low mists left (the morning's over the fields and the towns, a swamp's). The picture's depth (t3) gives each
// pixel's place in the world (r0-r11 the camera backwards, f0-farZ the projection); over the low ground the mist
// drifts - sourceRect.rgb its colour, .w how much, the ground's height the character's (2.2.2: sourceClamp.x, .y 1
// when known; else that near the middle of the view); sharpen the time. Blended over the picture by its alpha.
float4 mist_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) discard;  // nothing drawn there
    float3 v = viewAt(wp);
    float gy = sourceClamp.x;  // 2.2.2: the ground's height at the character (.y 1 when known)
    if (sourceClamp.y < 0.5) {
        int2 last = int2(uiPixels.xy) - 1;
        int2 mid = int2(uiPixels.xy * float2(0.5, 0.58));
        gy = 0.0;
        [unroll] for (int k = 0; k < 5; ++k) {
            float3 m = viewAt(clamp(mid + int2((k - 2) * int(uiPixels.x) / 16, 0), int2(0, 0), last));
            gy += m.x * r1 + m.y * r4 + m.z * r7 + r10;
        }
        gy *= 0.2;
    }
    float3 w = v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    float low = saturate((w.y - gy + 4.0) / 8.0);  // up is -y: the lower, the thicker
    float t = sharpen;
    float wisps = fbm4(w.xz * 0.025 + float2(t * 0.02, -t * 0.013));
    float f = low * (0.45 + 0.55 * smoothstep(0.3, 0.7, wisps)) * sourceRect.w;
    if (f < 0.003) discard;
    return float4(sourceRect.rgb, f);
}

// 0.82, the skills' sparks (torches.cpp applyEmbers), added on: about each place a skill burns or shines (t1, two
// texels: its middle (target pixels) .xy, its size .z, strength .w; then its colour .rgb and kind .w - 1 fire, 0
// another) - footprint how many - a dozen sparks: a fire's embers rise, swaying, and die out; another skill's
// glints spread out around it and fade. sharpen the time, scale k.
float4 embers_ps(PresentOut i) : SV_Target {
    float2 p = i.pos.xy;
    float t = sharpen;
    float3 sum = float3(0.0, 0.0, 0.0);
    int count = int(footprint);
    [loop] for (int k = 0; k < count; ++k) {
        float4 s = tex1.Load(int3(2 * k, 0, 0));
        float2 d0 = p - s.xy;
        if (dot(d0, d0) > 9.0 * s.z * s.z) continue;
        float4 c = tex1.Load(int3(2 * k + 1, 0, 0));
        bool fire = c.w > 0.5;
        [unroll] for (int j = 0; j < 12; ++j) {
            float seed = hash12(float2(float(k) * 13.1, float(j) * 7.7));
            float phase = frac(t * (fire ? 0.55 : 0.8) * (0.7 + 0.6 * seed) + seed * 3.1);
            float2 jitter = float2(hash12(float2(seed, 1.3)), hash12(float2(seed, 7.9))) - 0.5;
            float2 pos;
            if (fire) {
                pos = s.xy + jitter * s.z * 1.4 + float2(sin(t * 2.3 + seed * 6.28) * s.z * 0.12, -phase * s.z * 2.2);
            } else {
                float a = seed * 6.28 + t * 0.6;
                pos = s.xy + jitter * s.z * 0.5 + float2(cos(a), sin(a) * 0.6) * phase * s.z * 1.3;
            }
            float size = (0.9 + 1.4 * hash12(float2(seed, 4.2))) * scale;
            float2 d = p - pos;
            float glow = exp(-dot(d, d) / (size * size)) * sin(phase * 3.14159);
            float3 colour = fire ? lerp(float3(1.0, 0.85, 0.45), c.rgb, phase)
                                 : lerp(float3(1.0, 1.0, 1.0), c.rgb, 0.6);
            sum += colour * glow * s.w;
        }
    }
    if (dot(sum, sum) < 1e-6) discard;
    return float4(sum, 1.0);
}

// 0.81, grass (ao.cpp applyGrass), before the characters are drawn: each pixel looks down the screen for a blade
// rooted below it that reaches up to it - a root on green ground (the picture's copy, t2), at a spot of the world
// that has a blade (its place on the ground, so the blades stay put as the camera moves), the nearest root (lowest on
// the screen) in front. The blades sway in the wind and lie down where a step was left lately (t1, as steps_ps reads
// it; footprint how many). twinX the blades' longest (target pixels), twinY how many (0-1), sharpen the time.
// 1.7: the trodden blades lean away from the step; twinW the share of flowers (flowers), sourceRect.y their colours
// (living::Place::flowers); twinH the dew (dew) glinting on the tips. 2.7.10: the gusts gone (too faint to see).
// 2.6.8 (R-9K2F6E: 22 ms on an HD 6570): the steps' hold read from their grid (footprint 1: t1, core/trample.hpp;
// fogColor its corner, one over a texel's side, texels a side) instead of up to 64 steps a pixel; the root's depth
// read once; the map's ground looked up only for a root that has a blade; uiPixels.z 2 (Direct3D 11) looks for roots
// on every second row only - a blade still found whole, its root a row higher or lower.
float4 trampleAt(float2 xz) {  // 2.6.8: the grid between its four texel centres around xz, past its edge the edge's
    float2 g = (xz - fogColor.xy) * fogColor.z - 0.5;
    int last = int(fogColor.w + 0.5) - 1;
    int2 i = int2(floor(g));
    float2 f = g - float2(i);
    int2 a = clamp(i, int2(0, 0), int2(last, last)), b = clamp(i + int2(1, 1), int2(0, 0), int2(last, last));
    float4 t00 = tex1.Load(int3(a.x, a.y, 0)), t10 = tex1.Load(int3(b.x, a.y, 0));
    float4 t01 = tex1.Load(int3(a.x, b.y, 0)), t11 = tex1.Load(int3(b.x, b.y, 0));
    return lerp(lerp(t00, t10, f.x), lerp(t01, t11, f.x), f.y);
}

float3 flowerColour(float pick, int palette) {
    float3 a = float3(0.98, 0.86, 0.25), b = float3(0.97, 0.96, 0.92), c = float3(0.72, 0.58, 0.92);  // a meadow
    if (palette == 1) { a = float3(0.92, 0.28, 0.18); b = float3(0.98, 0.62, 0.20); c = float3(0.98, 0.88, 0.40); }
    else if (palette == 2) { a = float3(0.98, 0.72, 0.82); b = float3(0.99, 0.93, 0.95); c = float3(0.90, 0.50, 0.70); }
    else if (palette == 3) { a = float3(0.95, 0.25, 0.55); b = float3(0.99, 0.60, 0.15); c = float3(0.98, 0.92, 0.30); }
    return pick < 0.4 ? a : pick < 0.75 ? b : c;
}

// 2.7.57: the picture there is green ground (the grass's test of a root, at its neighbours)
bool greenGround(int2 p, int2 last) {
    float3 c = sceneColor.Load(int3(clamp(p, int2(0, 0), last), 0)).rgb;
    return c.g >= c.r * 1.04 && c.g >= c.b * 1.15 && c.g >= 0.12;
}

// 3.0.5-beta.12 (the user: «1 и 2» - the grass 9-11 ms of a Radeon 610M's frame, nine roots of ten a pixel looked at
// thrown out by what their own place decides): grass_ps's tests of a root that its own place decides - the picture
// green ground there, not under the water, a blade at that spot of the world (its hash), the root near the blade's
// middle (a flower's head reaches further: grass_ps), not too small to see, a green patch round it, the map's own
// ground - and what grass_ps goes on with: its view place, its colour, the blade's cell, its nearness, its height
// before the steps tread it, whether a flower. grass_mask_ps puts it into a mask once a pixel; grass_ps reads the
// mask first (tex0) - the same blades, most of its work gone.
struct GrassRoot {
    float3 v;
    float3 rc;
    float2 id;
    float near;
    float height;
    bool flower;
};

// 3.0.5-beta.16: the parts of a root that grassRoot and grassRootAt both work out, one expression each - the root's
// place in the world, how near it is to its cell's blade's spot, whether a flower, the blade's height
float3 grassWorld(float3 v) {
    return v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) + float3(r9, r10, r11);
}
float grassNear(float2 cell, float2 id) {
    float2 at = frac(cell) - (0.3 + 0.4 * float2(hash12(id + 11.3), hash12(id + 5.7)));
    return dot(at, at);
}
// 1.8.3: flowers in patches, a few in each (scattered over the whole field they were a speckle); 2.7.29: the
// patches' noise for a flower's root alone (an && of HLSL works both its sides out: for every root)
bool grassFlower(float2 id, float3 w) {
    bool flower = false;
    if (hash12(id + 31.7) < twinW) flower = valueNoise(w.xz * 0.07 + 3.3) > 0.72;
    return flower;
}
float grassHeight(float2 id, bool flower) { return (0.45 + 0.55 * hash12(id + 2.1)) * (flower ? 1.15 : 1.0); }

bool grassRoot(int2 root, int2 last, out GrassRoot g) {
    g = (GrassRoot)0;
    float rd = sceneDepth.Load(int3(root, 0));
    if (rd >= 0.99999) return false;
    g.rc = sceneColor.Load(int3(root, 0)).rgb;
    if (g.rc.g < g.rc.r * 1.04 || g.rc.g < g.rc.b * 1.15 || g.rc.g < 0.12) return false;  // not green ground
    g.v = viewFrom(root, rd);
    float3 w = grassWorld(g.v);
    // 2.7.2: none on a pond's or a river's bed - under the water (uiPixels.w its level, up is -y; none known: far
    // below): its green bed showed through the shallow water and grew blades over it (R-32CYUV, cmd_fild01)
    if (w.y > uiPixels.w) return false;
    float2 cell = w.xz * 2.0;  // a blade or none every half unit
    g.id = floor(cell);
    if (hash12(g.id) > twinY) return false;  // no blade here
    g.near = grassNear(cell, g.id);
    if (g.near > 0.2) return false;  // 2.7.29: off even a flower's head - before the patches' noise
    // 2.7.4: thinned out, then none, as the camera goes far back - a blade stands on a disc about a ninth of a unit
    // across, under a pixel there: its root came and went with every frame's jitter and the grass blinked
    float disc = 0.11 * f0 * scale / max(g.v.z, 1.0);  // the disc's radius in the picture's pixels
    if (hash12(g.id + 7.7) > smoothstep(0.8, 1.6, disc)) return false;
    g.flower = grassFlower(g.id, w);
    g.height = grassHeight(g.id, g.flower);
    if (!g.flower && g.near > 0.05) return false;  // a blade's own spot (a flower's head: up to 0.2, grass_ps)
    // 2.7.57 (the tester: «трава на брусчатке мигает когда идешь»): a root on a green patch, not a speck - the grass
    // painted in the paving's cracks is a few pixels across: as the camera moved by parts of a pixel they came and
    // went, and their blades with them. Green three of the game's pixels round it, each way.
    int reach = max(int(3.0 * scale + 0.5), 3);
    if (!greenGround(root + int2(reach, 0), last) || !greenGround(root - int2(reach, 0), last) ||
        !greenGround(root + int2(0, reach), last) || !greenGround(root - int2(0, reach), last))
        return false;
    // 2.2.1: on the map's own ground only - sourceClamp the cells' grid (its corner, one over a side, the cells across
    // + 1024 * down), t5 each cell's lowest and highest y: umbala's green decks and bridges stand above it (2.6.8:
    // looked up only for a root that has a blade - most have none)
    if (sourceClamp.z > 0.0) {
        int cellsAcross = int(fmod(sourceClamp.w, 1024.0) + 0.5), cellsDown = int(sourceClamp.w / 1024.0);
        int2 cellAt = int2(floor((w.xz - sourceClamp.xy) * sourceClamp.z));
        if (all(cellAt >= int2(0, 0)) && cellAt.x < cellsAcross && cellAt.y < cellsDown) {
            float2 cellSpan = mirrorTex.Load(int3(cellAt, 0)).rg;
            if (cellSpan.x > cellSpan.y || w.y < cellSpan.x - 0.75 || w.y > cellSpan.y + 0.75) return false;
        }
    }
    return true;
}

// 3.0.5-beta.16 (R-APBSRU: the grass 5.4 ms of a Radeon 610M's 24 on «Середні»): what grassRoot makes of a root,
// without its tests - for one the mask says passed them (the same picture and root: the same answers); grass_ps
// worked each marked root out whole again for every pixel above it
GrassRoot grassRootAt(int2 root) {
    GrassRoot g = (GrassRoot)0;
    g.rc = sceneColor.Load(int3(root, 0)).rgb;
    g.v = viewFrom(root, sceneDepth.Load(int3(root, 0)));
    float3 w = grassWorld(g.v);
    float2 cell = w.xz * 2.0;
    g.id = floor(cell);
    g.near = grassNear(cell, g.id);
    g.flower = grassFlower(g.id, w);
    g.height = grassHeight(g.id, g.flower);
    return g;
}

// 3.0.5-beta.12: each pixel as a blade's root, once: 1 where grassRoot finds one could stand (grass_ps then looks
// at it), 0 where none can. 3.0.5-beta.16: with the roots on every second row only (uiPixels.z 2: Direct3D 11's, a
// thinned grass's) half as tall - its row j the picture's row 2j (the odd rows' half of the work was never read)
float grass_mask_ps(PresentOut i) : SV_Target {
    int every = uiPixels.z > 1.5 ? 2 : 1;
    GrassRoot g;
    return grassRoot(int2(int(i.pos.x), int(i.pos.y) * every), int2(uiPixels.xy) - 1, g) ? 1.0 : 0.0;
}

// 3.0.5-beta.20 (the user: «делай 1 и 2» - the grass pass worked every pixel of a town's paving, a road, the sand,
// where no root can give a blade): the picture in tiles of 8 x 8 - grass_tiles_ps whether any pixel of a tile is a
// root the mask marked (r3 the mask's rows); grass_near_ps whether a tile's pixels can reach one - the tiles below
// within the longest blade and aside within the sway at its most (r0, r1 the tiles to the left and the right, r2
// below; t0 the tiles), written under the mask's rows of its texture; grass_ps leaves a pixel of a tile that cannot
// before anything else
float grass_tiles_ps(PresentOut i) : SV_Target {
    int every = uiPixels.z > 1.5 ? 2 : 1;
    int2 tile = int2(i.pos.xy);
    int rows = 8 / every, top = tile.y * rows, maskRows = int(r3), width = int(uiPixels.x);
    [loop] for (int y = top; y < top + rows && y < maskRows; ++y)
        [loop] for (int x = tile.x * 8; x < tile.x * 8 + 8 && x < width; ++x)
            if (tex0.Load(int3(x, y, 0)).r > 0.5) return 1.0;
    return 0.0;
}

float grass_near_ps(PresentOut i) : SV_Target {
    int maskRows = int(r3);
    int2 tile = int2(int(i.pos.x), int(i.pos.y) - maskRows);
    int2 tiles = int2((int(uiPixels.x) + 7) / 8, (int(uiPixels.y) + 7) / 8);
    int x0 = max(tile.x - int(r0), 0), x1 = min(tile.x + int(r1), tiles.x - 1);
    int y1 = min(tile.y + int(r2), tiles.y - 1);
    [loop] for (int y = tile.y; y <= y1; ++y)
        [loop] for (int x = x0; x <= x1; ++x)
            if (tex0.Load(int3(x, y, 0)).r > 0.5) return 1.0;
    return 0.0;
}

float4 grass_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    int2 last = int2(uiPixels.xy) - 1;
    int every = uiPixels.z > 1.5 ? 2 : 1;  // 2.6.8: every second row's roots only (the roots on the even rows)
    // 3.0.5-beta.20: a tile none of whose pixels can reach a marked root (grass_near_ps, under the mask's rows)
    int maskRows = (int(uiPixels.y) + every - 1) >> (every - 1);
    if (tex0.Load(int3(wp.x >> 3, maskRows + (wp.y >> 3), 0)).r < 0.5) discard;
    float here = sceneDepth.Load(int3(wp, 0));
    float hereZ = here >= 0.99999 ? 1e6 : viewDepth(here);
    int longest = int(twinX);
    float t = sharpen;
    // 1.7: the way the steps near here push the blades, along the screen (the view's x axis in the world: r0-r2).
    // 1.8.3: the wind's sway by the place in the world under the pixel, not by the pixel - by the pixel the waves
    // moved over the grass with the camera as one walked.
    // 1.9.9: how far the steps trod the grass down here, once a pixel (until 1.9.8 for every root it looked at: up
    // to 28 roots times 64 steps a pixel - the costliest effect, 1.3-3.2 ms at 4K)
    float lean = 0.0, swayPhase = 0.0, trod = 1.0;
    float windStrength = length(sourceRect.zw);
    float2 windWay = windStrength > 1e-4 ? sourceRect.zw / windStrength : float2(0.943, 0.33);
    // 1.22: the blades lean the wind's way on the screen (the view's x axis in the world: r0-r2)
    float windLean = windStrength * dot(float3(windWay.x, 0.0, windWay.y), float3(r0, r1, r2)) * 0.35;
    if (here < 0.99999) {
        float3 hv = viewAt(wp);
        float3 hw = hv.x * float3(r0, r1, r2) + hv.y * float3(r3, r4, r5) + hv.z * float3(r6, r7, r8) +
                    float3(r9, r10, r11);
        swayPhase = hw.x * 0.35 + hw.z * 0.22;
        if (footprint > 0.5) {  // 2.6.8: the steps' hold, from their grid
            float4 hold = trampleAt(hw.xz);
            lean = clamp(dot(float3(hold.x, 0.0, hold.y) * 2.0, float3(r0, r1, r2)), -1.0, 1.0);
            trod = hold.z;
        }
    }
    int first = longest - (every == 2 ? ((wp.y + longest) & 1) : 0);
    // 2.7.29: the sway's wave once a pixel, not at every root (the user: «можешь оптимизировать?» - the grass 3-5 ms
    // of a frame at 4K)
    float swayAll = (sin(t * 1.7 + swayPhase) * 0.25 + lean * 0.5 + windLean) * twinX;
    // 3.0.5-beta.12: grass_mask_ps's mask in tex0. 3.0.5-beta.17 (R-RAFYSB: beta.16's grass 0.9 ms dearer - with a
    // way without the mask beside the masked one in its loop, the old cards' compiler worked a marked root out both
    // ways): the masked way alone; without its mask the grass is not drawn (the renderers)
    [loop] for (int k = first; k >= 1; k -= every) {
        float up = float(k) / twinX;
        float sway = swayAll * up * up;
        int2 root = clamp(wp + int2(int(round(-sway)), k), int2(0, 0), last);
        // the mask's row of it (3.0.5-beta.16: every second row's - an odd row's root, the picture's last row clamped,
        // has none), then only what grassRoot makes of it
        if ((root.y & (every - 1)) != 0 || tex0.Load(int3(root.x, root.y >> (every - 1), 0)).r < 0.5) continue;
        GrassRoot g = grassRootAt(root);
        if (hereZ < g.v.z - 1.0) continue;  // something in front of the blade
        bool head = g.flower && up > g.height * 0.78;
        if (g.near > (head ? 0.2 : 0.05)) continue;
        float height = g.height * trod;  // trodden down
        if (up > height) continue;
        float3 colour = g.rc * lerp(0.65, 1.25, up / max(height, 1e-3));
        if (head) colour = flowerColour(hash12(g.id + 17.9), int(sourceRect.y + 0.5)) * (0.75 + 0.4 * g.rc.g);
        if (twinH > 0.0 && up > height * 0.85) {  // a dew drop on the tip, now and then catching the sun
            float seed = hash12(g.id + 43.1);
            float glint = pow(saturate(sin(t * (0.9 + seed * 1.7) + seed * 40.0)), 24.0) * step(0.8, seed);
            colour = lerp(colour, float3(1.0, 1.0, 0.97), glint * twinH);
        }
        return float4(colour, 1.0);
    }
    discard;
    return float4(0.0, 0.0, 0.0, 0.0);
}

// 1.7, the ground of the living world (ao.cpp applyGroundLife), before the characters, blended over by its alpha:
// twinX sand drifting over desert ground in the wind (sand_drift), twinW the walls of a damp cave with water running
// down them where light falls (wet_walls); 2.7.45: the snow's glints (twinY) gone. sharpen the time;
// r0-r11 the view -> the world; the picture's copy t2.
float4 ground_life_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) discard;
    int2 last = int2(uiPixels.xy) - 1;
    float3 v = viewAt(wp);
    float3 w = v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    float3 col = sceneColor.Load(int3(wp, 0)).rgb;
    float luma = dot(col, float3(0.299, 0.587, 0.114));
    float t = sharpen;
    float3 o = col;
    float a = 0.0;
    if (twinX > 0.0 && col.r > col.g * 1.02 && col.g > col.b * 1.05 && col.r > 0.22) {  // sand
        float2 wind = any(uiPixels.zw != 0.0) ? normalize(uiPixels.zw) : float2(0.94, 0.34);  // 1.22: shared
        float2 q = float2(dot(w.xz, wind), dot(w.xz, float2(-wind.y, wind.x)));
        float n = valueNoise(float2(q.x * 0.07 - t * 1.4, q.y * 0.8)) * 0.6 +
                  valueNoise(float2(q.x * 0.16 - t * 2.3 + 3.1, q.y * 1.9)) * 0.4;
        // 1.25: fewer and fainter, in drifting runs (the whole ground hatched looked drawn on)
        float run = smoothstep(0.45, 0.7, valueNoise(float2(q.x * 0.012 - t * 0.3, q.y * 0.05)));
        float streak = smoothstep(0.68, 0.86, n) * run * twinX;
        o = lerp(o, min(col * 1.15 + 0.05, 1.0), streak);
        a = max(a, streak * 0.32);
    }
    if (twinW > 0.0) {  // a damp cave
        float3 l = viewAt(max(wp - int2(1, 0), int2(0, 0))), r = viewAt(min(wp + int2(1, 0), last));
        float3 u = viewAt(max(wp - int2(0, 1), int2(0, 0))), d = viewAt(min(wp + int2(0, 1), last));
        float3 n = normalize(cross(d - u, r - l));
        float3 nw = n.x * float3(r0, r1, r2) + n.y * float3(r3, r4, r5) + n.z * float3(r6, r7, r8);
        float wall = 1.0 - smoothstep(0.55, 0.85, abs(nw.y));
        float2 s = float2(w.x + w.z, w.y);
        float lit = smoothstep(0.12, 0.45, luma);
        // 2.6.21: the glints gone - blinking dots in cells half a unit wide, "looks like dots in the air"; the
        // trickles run down: the world's y grows downwards, and + t had them crawl up the walls
        float run = smoothstep(0.93, 1.0, valueNoise(float2(s.x * 2.5, s.y * 0.12 - t * 0.35))) * 0.5 * lit;
        float wet = run * wall * twinW;
        o = lerp(o, min(col * 1.6 + 0.12, 1.0), saturate(wet));
        a = max(a, saturate(wet));
    }
#ifndef UAGFX_SAFE
    // 1.20, the year and the rain on the whole map (core/seasons.hpp): sourceRect.x winter's snow on what faces up,
    // .z the green ground drier (autumn, winter), .w greener (spring); sourceClamp.z the rain's wet on the walls, the
    // roofs and the models (rain_wet) - darker, a sheen where one looks along them. 2.4: .y (autumn's leaves on the
    // ground) and 1.9.8's pebbles (sourceClamp.y) are gone - specks a few pixels across over every paved square.
    if (any(sourceRect > 0.0) || sourceClamp.z > 0.0) {
        float3 l = viewAt(max(wp - int2(1, 0), int2(0, 0))), r = viewAt(min(wp + int2(1, 0), last));
        float3 u = viewAt(max(wp - int2(0, 1), int2(0, 0))), d = viewAt(min(wp + int2(0, 1), last));
        float3 n = normalize(cross(d - u, r - l));
        float3 nw = n.x * float3(r0, r1, r2) + n.y * float3(r3, r4, r5) + n.z * float3(r6, r7, r8);
        float up = smoothstep(0.55, 0.85, abs(nw.y));
        float green = saturate((col.g - max(col.r, col.b)) * 6.0);
        if (sourceRect.z > 0.0 || sourceRect.w > 0.0) {  // the grass's colour by the season
            float3 straw = float3(luma * 1.2, luma * 1.05, luma * 0.55);
            float3 c2 = lerp(col, straw, sourceRect.z * 0.7);
            c2 = lerp(c2, col * float3(0.9, 1.12, 0.88), sourceRect.w * 0.6);
            float k = green * up;
            o = lerp(o, c2, k / max(a + k, 1e-3));
            a = saturate(max(a, k));
        }
        if (sourceRect.x > 0.0) {  // snow on what faces up
            float drift = valueNoise(w.xz * 0.35) * 0.6 + valueNoise(w.xz * 1.3 + 4.1) * 0.4;
            float cover = saturate(up * smoothstep(0.3, 0.6, drift + 0.3 * (up - 0.5))) * sourceRect.x;
            cover *= 1.0 - smoothstep(0.7, 0.88, luma);  // already white
            float3 snow = float3(0.9, 0.93, 0.98) * clamp(luma * 1.6 + 0.25, 0.4, 1.0);
            o = lerp(o, snow, cover / max(a + cover, 1e-3));
            a = saturate(max(a, cover));
        }
        if (sourceClamp.z > 0.0) {  // wet in the rain
            float along = pow(1.0 - saturate(abs(dot(normalize(v), n))), 3.0);
            float wall = 1.0 - up;
            float run = smoothstep(0.9, 1.0, valueNoise(float2((w.x + w.z) * 2.0, w.y * 0.1 + t * 0.6))) * wall;
            float3 wet = col * lerp(0.78, 0.9, up) + float3(0.55, 0.6, 0.65) * (along * 0.25 + run * 0.15);
            float k = sourceClamp.z * 0.8;
            o = lerp(o, wet, k / max(a + k, 1e-3));
            a = saturate(max(a, k));
        }
    }
#endif
    if (a < 0.004) discard;
    return float4(o, a);
}

// 1.7, the living world's creatures and particles (critters.cpp; core/critters.hpp): a quad each (critters_vs, six
// vertices a sprite), premultiplied over the world. t1 four texels a sprite - its middle (target pixels), size
// (pixels) and view depth; its kind, two numbers of its kind and its alpha; its heading on the screen (radians) and
// how flat a circle on the ground looks; for a dragonfly the water's surface under it (pixels, depth, 1). Kind 13:
// the light from above under the sea, over the whole picture. sharpen the time, twinW the creatures' light. 2.6.11:
// kind 14 a firefly (q.y how lit).
float segment(float2 p, float2 a, float2 b) {
    float2 pa = p - a, ba = b - a;
    return length(pa - ba * saturate(dot(pa, ba) / max(dot(ba, ba), 1e-6)));
}

float2 turned(float2 d, float angle) {
    float c = cos(angle), s = sin(angle);
    return float2(d.x * c + d.y * s, -d.x * s + d.y * c);
}

struct CritterIn {
    float4 pos : SV_Position;
    float2 d : TEXCOORD0;
    nointerpolation uint k : TEXCOORD1;
};

CritterIn critters_vs(uint id : SV_VertexID) {
    CritterIn o;
    uint k = id / 6, v = id % 6;
    float2 corner = float2(v == 1 || v == 4 || v == 5 ? 1.0 : -1.0, v == 2 || v == 3 || v == 5 ? 1.0 : -1.0);
    float4 s = tex1.Load(int3(4 * k, 0, 0));
    float kind = tex1.Load(int3(4 * k + 1, 0, 0)).x;
    float2 px = abs(kind - 13.0) < 0.5 ? (corner * 0.5 + 0.5) * targetSize : s.xy + corner * s.z * 3.5;
    o.pos = float4(px.x * 2.0 / targetSize.x - 1.0, 1.0 - px.y * 2.0 / targetSize.y, 0.0, 1.0);
    o.d = (px - s.xy) / max(s.z, 1.0);
    o.k = k;
    return o;
}

float4 critters_ps(CritterIn i) : SV_Target {
    uint k = i.k;
    float2 d = i.d;
    float2 p = i.pos.xy;
    float4 s = tex1.Load(int3(4 * k, 0, 0));
    float4 q = tex1.Load(int3(4 * k + 1, 0, 0));
    float4 r = tex1.Load(int3(4 * k + 2, 0, 0));
    int kind = int(q.x + 0.5);
    float t = sharpen;
    if (kind == 13) {  // under the sea: light from above in slanting bands, swaying
        float2 u = p / targetSize;
        float band = 0.0;
        [unroll] for (int b = 0; b < 4; ++b) {
            float x = u.x + u.y * 0.35 + sin(t * 0.3 + float(b) * 1.7) * 0.04 - float(b) * 0.27 - 0.1;
            band += smoothstep(0.05, 0.0, abs(frac(x) - 0.5) - 0.02 * (1.0 + float(b % 2)));
        }
        return float4(float3(0.55, 0.8, 1.0) * band * 0.07 * (1.0 - u.y * 0.7) * q.w, 0.0);
    }
    float raw = sceneDepth.Load(int3(int2(p), 0));
    if ((raw >= 0.99999 ? 1e6 : viewDepth(raw)) < s.w - 0.8) discard;  // behind something
    float a = 0.0;
    float3 c = float3(0.0, 0.0, 0.0);
    float3 glow = float3(0.0, 0.0, 0.0);
    if (kind == 1) {  // a butterfly: q.y its wings' beat, q.z its colour
        float2 l = turned(d, r.x);
        float f = 0.2 + 0.8 * abs(sin(q.y));
        float2 wg = float2(abs(l.y) / f, l.x);
        float wing = max(smoothstep(1.0, 0.75, length((wg - float2(0.5, 0.15)) / float2(0.5, 0.45))),
                         smoothstep(1.0, 0.75, length((wg - float2(0.4, -0.3)) / float2(0.38, 0.32))));
        int ci = int(q.z + 0.5);
        c = ci == 0 ? float3(0.97, 0.96, 0.9) : ci == 1 ? float3(0.98, 0.8, 0.22)
          : ci == 2 ? float3(0.45, 0.66, 0.98) : float3(0.96, 0.55, 0.2);
        float body = smoothstep(0.1, 0.05, abs(l.y)) * step(abs(l.x), 0.42);
        c = lerp(c, float3(0.15, 0.12, 0.1), body);
        a = max(wing, body);
    } else if (kind == 2) {  // a dragonfly, over the water only
        float4 u = tex1.Load(int3(4 * k + 3, 0, 0));
        if (u.w > 0.5) {
            float under = sceneDepth.Load(int3(clamp(int2(u.xy), int2(0, 0), int2(targetSize) - 1), 0));
            if ((under >= 0.99999 ? 1e6 : viewDepth(under)) < u.z - 0.3) discard;  // ground there, not water
        }
        float2 l = turned(d, r.x);
        float body = smoothstep(0.09, 0.04, abs(l.y)) * step(-0.75, l.x) * step(l.x, 0.45);
        float flick = 0.6 + 0.4 * sin(q.y);
        float wings = 0.0;
        [unroll] for (int w = 0; w < 4; ++w) {
            float2 c0 = float2(w < 2 ? 0.18 : 0.0, (w % 2 == 0 ? 1.0 : -1.0) * 0.32);
            wings = max(wings, smoothstep(1.0, 0.7, length((l - c0) / float2(0.11, 0.36 * flick))));
        }
        c = lerp(float3(0.7, 0.8, 0.85), float3(0.12, 0.36, 0.42), body);
        a = max(body, wings * 0.22);  // 1.25: the wings a faint shimmer, not a pale outline
    } else if (kind == 3) {  // a bird on the ground, pecking: q.y its time, q.z its facing
        float peck = pow(saturate(sin(q.y * 5.0)), 6.0) * 0.25;
        float2 l = float2(d.x * q.z, d.y);
        float body = smoothstep(1.0, 0.8, length((l - float2(-0.05, 0.05)) / float2(0.55, 0.32)));
        float head = smoothstep(0.2, 0.15, length(l - float2(0.45, -0.22 + peck)));
        float beak = step(segment(l, float2(0.6, -0.22 + peck), float2(0.75, -0.18 + peck * 1.4)), 0.04);
        c = body > head ? float3(0.42, 0.33, 0.25) : float3(0.3, 0.24, 0.18);
        c = lerp(c, float3(0.85, 0.65, 0.2), beak);
        a = max(max(body, head), beak);
    } else if (kind == 4) {  // a bird flying: wings beating
        float flap = sin(q.y) * 0.45;
        float v = min(segment(d, float2(0.0, 0.0), float2(-0.7, -flap)),
                      segment(d, float2(0.0, 0.0), float2(0.7, -flap)));
        c = float3(0.22, 0.18, 0.15);
        a = smoothstep(0.09, 0.05, v) + smoothstep(0.18, 0.12, length(d / float2(1.0, 0.7)));
    } else if (kind == 5) {  // a leaf turning: q.y its angle, q.z its colour (2.7.30: 4 a summer's yellow-green)
        float2 l = turned(d, q.y);
        float e = length(l / float2(1.0, 0.45 + 0.35 * abs(sin(q.y * 0.7))));
        int ci = int(q.z + 0.5);
        c = ci == 0 ? float3(0.42, 0.62, 0.2) : ci == 1 ? float3(0.86, 0.66, 0.16)
          : ci == 2 ? float3(0.82, 0.38, 0.12) : ci == 4 ? float3(0.74, 0.84, 0.32) : float3(0.97, 0.76, 0.84);
        c *= 0.85 + 0.25 * step(0.0, l.y);
        a = smoothstep(1.0, 0.8, e);
    } else if (kind == 6) {  // an ember
        glow = float3(1.0, 0.62, 0.22) * exp(-dot(d, d) * 3.0) * 1.4;
    } else if (kind == 8) {  // a drop
        c = float3(0.85, 0.93, 1.0);
        a = smoothstep(1.0, 0.6, length(d));
        glow = c * 0.15 * a;
    } else if (kind == 9) {  // a bubble: a bright rim, a glint
        a = smoothstep(0.25, 0.05, abs(length(d) - 0.8)) * 0.8;
        c = float3(0.8, 0.95, 1.0);
        glow = float3(1.0, 1.0, 1.0) * smoothstep(0.3, 0.1, length(d - float2(-0.35, -0.35))) * 0.6;
    } else if (kind == 10) {  // a falling drip: a streak q.y sizes long
        c = float3(0.8, 0.9, 1.0);
        a = smoothstep(0.9, 0.3, segment(d, float2(0.0, -q.y), float2(0.0, 0.0))) * 0.8;
    } else if (kind == 14) {  // 2.6.11: a firefly - a yellow-green light and its soft halo, lit a while (q.y)
        float rr = dot(d, d);
        glow = float3(0.78, 1.0, 0.38) * (exp(-rr * 3.0) * 1.5 + exp(-rr * 0.35) * 0.3) * q.y;
    } else if (kind == 11) {  // a ring on the ground
        a = smoothstep(0.12, 0.0, abs(length(d / float2(1.0, r.y)) - 0.9)) * 0.7;
        c = float3(0.85, 0.92, 1.0);
    } else {  // a shadow on the ground
        a = smoothstep(1.0, 0.4, length(d / float2(1.0, r.y))) * 0.9;
    }
    a = saturate(a) * q.w;
    if (kind != 12) c *= twinW;
    float3 o = c * a + glow * q.w;
    if (a < 0.002 && dot(o, o) < 1e-6) discard;
    return float4(o, a);
}

// 1.8, a level of a texture's material (materials.cpp): from the picture (t0, its levels read whole, wrapped - the
// textures tile) at its level uiPixels.z (uiPixels.w levels; sourceRect.xy its size): its relief from the slopes of
// its brightness at four scales, its height against its far average, its gloss where it is grey and light (stone,
// tiles, metal) - rgba: the normal's xy, the height, the gloss.
float matLuma(int2 p, int lod) {
    int have = max(int(uiPixels.w + 0.5) - 1, 0);  // 1.8.1: past the source's levels, every 2^n-th texel of its last
    int at = min(lod, have);
    int2 size = max(int2(sourceRect.xy) >> at, int2(1, 1));
    int2 q = p << (lod - at);
    q = ((q % size) + size) % size;
    return dot(tex0.Load(int3(q, at)).rgb, float3(0.299, 0.587, 0.114));
}

float4 material_ps(PresentOut i) : SV_Target {
    int2 p = int2(i.pos.xy);
    int base = int(uiPixels.z + 0.5), most = base + 5;
    int top = most;
    float far = matLuma(p >> (top - base), top);
    float2 slope = float2(0.0, 0.0);
    float h = 0.0, weights = 0.0;
    [unroll] for (int s = 0; s < 4; ++s) {
        int lod = min(base + s, most);
        int2 q = p >> (lod - base);
        float w = s == 0 ? 1.0 : s == 1 ? 0.75 : s == 2 ? 0.55 : 0.4;
        float gx = matLuma(q + int2(1, 0), lod) - matLuma(q - int2(1, 0), lod);
        float gy = matLuma(q + int2(0, 1), lod) - matLuma(q - int2(0, 1), lod);
        slope += float2(gx, gy) * 0.5 * w / float(1 << (lod - base));
        h += (matLuma(q, lod) - far) * w;
        weights += w;
    }
    // 2.3, uaRO Studio's material of one texture: its gloss, relief and height scaled (sourceClamp.xyz, 1 as built)
    float3 n = normalize(float3(-slope * 5.0 * sourceClamp.y, 1.0));
    int have = max(int(uiPixels.w + 0.5) - 1, 0), at = min(base, have);
    int2 size = max(int2(sourceRect.xy) >> at, int2(1, 1));
    int2 q0 = p << (base - at);
    float3 rgb = tex0.Load(int3(((q0 % size) + size) % size, at)).rgb;
    float sat = max(rgb.r, max(rgb.g, rgb.b)) - min(rgb.r, min(rgb.g, rgb.b));
    float lum = dot(rgb, float3(0.299, 0.587, 0.114));
    float gloss = min(saturate((0.22 - sat) * 4.0) * smoothstep(0.2, 0.6, lum) * 0.7 * sourceClamp.x, 0.7);
    // 1.21: a window's pane - an even patch clearly darker than the wall about it: alpha 0.75-1 (gloss stays under
    // 0.7), lit at night by shade(). 2.7.58 (R-JSBTFQ: a guild's emblem on its flag lit in warm blocks at night - its
    // black rider an even dark patch on a light field): none in a picture under 64 texels a side (sourceRect.zw the
    // game's own size, the AI's copy four times it) - an emblem's 24 in 32, a house's walls 128 and more
    float local = matLuma(p >> 2, base + 2), wide = matLuma(p >> 4, base + 4);
    float pane = smoothstep(0.1, 0.2, wide - local) * smoothstep(0.35, 0.22, local) *
                 smoothstep(0.08, 0.03, abs(lum - local)) * step(64.0, min(sourceRect.z, sourceRect.w));
    if (pane > 0.3 && sourceClamp.x > 0.0) gloss = 0.75 + 0.25 * pane;
    return float4(n.xy * 0.5 + 0.5, saturate(0.5 + h / weights * 1.6 * sourceClamp.z), gloss);
}

#ifndef UAGFX_SAFE
// 1.16, the world mirrored under the water (mirror.cpp): the last frame's opaque world (the sun map's casters, x y z
// u v and the game's colour) turned over the water's level twinX (up is -y) and drawn by the game's camera -
// r0-r11 the world into the view, f0-farZ its projection, uiPixels.xy the game's screen - into a picture of its own;
// what lay under the water is left out. footprint 1: the texture's holes (alpha under twinY) let through.
struct MirrorIn {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 colour : COLOR0;
    float under : TEXCOORD1;
};

MirrorIn mirror_vs(float3 p : POSITION, float2 uv : TEXCOORD0, float4 colour : COLOR0) {
    MirrorIn o;
    o.under = p.y - twinX;
    float4 w = float4(p.x, 2.0 * twinX - p.y, p.z, 1.0);
    float3 v = float3(dot(w, float4(r0, r1, r2, r3)), dot(w, float4(r4, r5, r6, r7)), dot(w, float4(r8, r9, r10, r11)));
    float z = max(v.z, nearZ + 0.01);
    float sx = v.x / z * f0 + cx, sy = v.y / z * f1 + cy;
    float nz = farZ * (1.0 - nearZ / z) / (farZ - nearZ);
    o.pos = float4((sx / uiPixels.x * 2.0 - 1.0) * z, (1.0 - sy / uiPixels.y * 2.0) * z, nz * z, z);
    o.uv = uv;
    o.colour = colour;
    return o;
}

float4 mirror_ps(MirrorIn i) : SV_Target {
    if (i.under > 0.05) discard;
    float4 t = tex0.Sample(samp0, i.uv);
    if (footprint > 0.5 && t.a < twinY) discard;
    return float4(t.rgb * i.colour.rgb, 1.0);
}
#endif

// 0.80, footprints (ao.cpp applySteps): on the ground (the picture's depth, the camera backwards) about each step of
// t1 - two texels: its place (xyz) and way (w), then its age 0-1 (x) - footprint how many; twinX the kind: 1 snow,
// 2 sand (a foot's print, fading), 3 dust (a puff spreading and thinning). Blended over by its alpha.
float stepPrint(int k, float3 w, int kind) {  // step k's print (its alpha) at the world's w
    float4 s = tex1.Load(int3(2 * k, 0, 0));
    if (abs(w.y - s.y) > 3.0) return 0.0;  // core/step_tiles.hpp's kRise
    float2 d = w.xz - s.xz;
    if (dot(d, d) > 40.0) return 0.0;  // its kReach
    float age = tex1.Load(int3(2 * k + 1, 0, 0)).x;
    if (kind == 3) {
        float r = 1.0 + 4.0 * age;
        float puff = exp(-dot(d, d) / (r * r)) * (1.0 - age) * (1.0 - age);
        return puff * (0.7 + 0.3 * valueNoise(w.xz * 0.8 + s.xz));
    }
    float ca = cos(s.w), sa = sin(s.w);
    float2 l = float2(d.x * ca + d.y * sa, -d.x * sa + d.y * ca);
    float e = length(l / float2(1.6, 0.75));  // 2.7.35: half again as big (were 2.2 by 1 units)
    return smoothstep(1.0, 0.65, e) * (1.0 - age * age);
}

// 3.0.5-beta.9 (R-VMPQKX: 4.1 ms of a Radeon 610M's frame - every pixel went through all 64 steps): t1's rows from the
// second hold the screen's tiles of 64 pixels (core/step_tiles.hpp, twinY by twinW of them), a texel a tile - the steps
// that may touch it as four 16-bit masks, whole numbers in its channels; a pixel goes through its tile's alone
float4 steps_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    int2 tile = min(wp / 64, int2(int(twinY + 0.5), int(twinW + 0.5)) - 1);
    uint4 parts = uint4(tex1.Load(int3(tile.x, tile.y + 1, 0)) + 0.5);
    if (all(parts == 0)) discard;
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) discard;
    float3 v = viewAt(wp);
    float3 w = v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    int kind = int(twinX + 0.5);
    float a = 0.0;
    [loop] for (int part = 0; part < 4; ++part) {
        uint bits = part == 0 ? parts.x : part == 1 ? parts.y : part == 2 ? parts.z : parts.w;
        [loop] while (bits != 0) {
            uint low = bits & (0u - bits);  // the lowest step left (no firstbitlow: shader model 4 too)
            bits ^= low;
            a = max(a, stepPrint(part * 16 + int(round(log2(float(low)))), w, kind));
        }
    }
    if (a < 0.01) discard;
    float3 colour = kind == 1   ? float3(0.42, 0.48, 0.62)
                    : kind == 2 ? float3(0.50, 0.38, 0.24)
                                : float3(0.82, 0.74, 0.60);
    // 2.7.35 (the tester in Comodo: «Сліди на снігу й піску - еле заметно»): the prints darker
    float most = kind == 1 ? 0.62 : kind == 2 ? 0.52 : 0.35;
    return float4(colour, a * most);
}

// 0.73, light in the air (haze.cpp), added onto the picture: about each light of the torches' pass (t1, footprint of
// them, as torch_light_ps reads them) the air glows - the light scattered along the view up to what the pixel shows
// (its depth), so a wall hides the glow behind it - twinX its strength; and under the open sky (twinY its strength)
// rays from the sky where the map's edge shows it, streaming away from the sun (sourceRect.xy, target pixels). 2.5.7:
// drawn at a share of the world's size (sourceRect.w: 2 at half - a quarter of the pixels, of the rays' 24 depth reads
// each), each pixel the world's it stands for, then laid over smoothly (half_over_ps, added).
float4 shafts_ps(PresentOut i) : SV_Target {
    int2 last = int2(uiPixels.xy) - 1;
    int2 wp = min(int2(i.pos.xy * max(sourceRect.w, 1.0)), last);
    float raw = sceneDepth.Load(int3(wp, 0));
    float2 g = (float2(wp) - float2(jitterX, jitterY)) / scale;
    float3 dir = normalize(float3((g.x - cx) / f0, (g.y - cy) / f1, 1.0));
    float far = raw >= 0.99999 ? 1e4 : length(viewAt(wp));
    float3 sum = float3(0.0, 0.0, 0.0);
    int count = int(footprint);
    [loop] for (int k = 0; k < count; ++k) {
        float4 t = tex1.Load(int3(2 * k, 0, 0));
        float t0 = dot(t.xyz, dir);
        float h = sqrt(max(dot(t.xyz, t.xyz) - t0 * t0, 0.0));
        if (h >= t.w) continue;
        h = max(h, 2.0);
        float along = (atan((far - t0) / h) + atan(t0 / h)) / h;  // the integral of 1 / distance^2 along the view
        float4 c = tex1.Load(int3(2 * k + 1, 0, 0));
        float fall = 1.0 - h / t.w;
        // 2.7.36: the skills' lights (twinW to twinH in the list) a third - sixteen of a Magnus whitened the air
        float share = float(k) >= twinW && float(k) < twinH ? 0.33 : 1.0;
        sum += c.rgb * (abs(c.w) * along * fall * fall * twinX * share);  // 2.7.24: a ground light's negative
    }
    if (twinY > 0.0) {
        float2 toSun = sourceRect.xy - float2(wp);
        float2 stride = toSun / max(length(toSun), 1.0) * min(length(toSun), 0.45 * length(uiPixels.xy)) / 24.0;
        float2 q = float2(wp) + stride * hash12(float2(wp));  // dithered: no bands
        float rays = 0.0, decay = 1.0;
        [loop] for (int s = 0; s < 24; ++s) {
            q += stride;
            rays += (sceneDepth.Load(int3(clamp(int2(q), int2(0, 0), last), 0)) >= 0.99999 ? 1.0 : 0.0) * decay;
            decay *= 0.94;
        }
        sum += sourceRect.z * float3(1.0, 0.93, 0.78) * (rays / 24.0 * twinY * (raw >= 0.99999 ? 0.3 : 1.0));
    }
    if (dot(sum, sum) < 1e-6) discard;
    return float4(sum, 1.0);
}

// 0.72, reflections on the floor (haze.cpp): what faces up - the floors of the dungeons and houses, the ground in the
// rain - reflects the picture (t2, before it) along the mirrored view (waterReflection), the more the flatter one looks
// along it. twinX its strength. 0.86: drawn at half the world's size (a pixel for four: a quarter of the rays),
// premultiplied - none where nothing reflects - then laid over the world by half_over_ps. 2.7.36: twinW the night.
float4 floor_reflect_ps(PresentOut i) : SV_Target {
    int2 last = int2(uiPixels.xy) - 1;
    int2 wp = min(int2(i.pos.xy) * 2, last);
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) return float4(0.0, 0.0, 0.0, 0.0);
    float3 p = viewAt(wp);
    float3 l = viewAt(max(wp - int2(1, 0), int2(0, 0))), r = viewAt(min(wp + int2(1, 0), last));
    float3 u = viewAt(max(wp - int2(0, 1), int2(0, 0))), d = viewAt(min(wp + int2(0, 1), last));
    float3 dx = abs(r.z - p.z) < abs(p.z - l.z) ? r - p : p - l;
    float3 dy = abs(d.z - p.z) < abs(p.z - u.z) ? d - p : p - u;
    float3 n = normalize(cross(dy, dx));
    if (dot(n, p) > 0.0) n = -n;  // towards the camera
    float3 v = normalize(p);
    // 2.2.2: where both neighbours of a side lie off this surface (a thin edge, a gap's or a step's rim) its normal is
    // a guess that changes as the camera turns, nearly across the view - a reflection flashed there. It fades out from
    // a jump of 0.8 % of the depth (a floor's own is under 0.1 % a pixel), and where the normal turns from the camera
    // (a floor faces it by half or more).
    float breakX = min(abs(r.z - p.z), abs(p.z - l.z)), breakY = min(abs(d.z - p.z), abs(p.z - u.z));
    float rim = smoothstep(0.008, 0.02, max(breakX, breakY) / p.z);
    float3 nw = n.x * float3(r0, r1, r2) + n.y * float3(r3, r4, r5) + n.z * float3(r6, r7, r8);
    // 2.7.16 (the tester at prt_fild08 in the rain: «квадратность на дороге», «отражения побитые»): the rain's water
    // lies level - it mirrors along the world's up, not each triangle's own slope - and a gentle hill's faces hold it
    // alike; by a face's own flatness (0.88-0.97) one face of a slope was wet and the next dry, a reflection cut at
    // their edges. The floors of dungeons and houses (no rain) as before.
    bool rain = twinY > 0.0;
    float3 up = normalize(float3(-r1, -r4, -r7));  // the world's up (-y) in the view
    if (dot(up, p) > 0.0) up = -up;
    float3 nr = rain ? up : n;
    float flat = rain ? smoothstep(0.6, 0.85, -nw.y) : smoothstep(0.88, 0.97, -nw.y);  // up is -y
    float shine = flat * (1.0 - rim) * smoothstep(0.15, 0.3, dot(-v, nr));  // what reflects
    // the rain's wet darkens a rim as the floor about it (taken for floor): no light specks along the edges
    float wet = rain ? lerp(flat, 1.0, rim) : 0.0;
    if (shine <= 0.0 && wet <= 0.0) return float4(0.0, 0.0, 0.0, 0.0);
    float weight = 0.0;
    float3 c = float3(0.0, 0.0, 0.0);
    if (shine > 0.0)  // 2.7.17: the rain's water mirrors what stands close by
        c = rain ? puddleReflection(p + nr * 0.6, reflect(v, nr), uiPixels.xy, weight)
                 : reflectionIn(p + nr * 0.6, reflect(v, nr), uiPixels.xy, weight);
    float fresnel = 0.04 + 0.96 * pow(1.0 - saturate(dot(-v, nr)), 5.0);
    // 2.7.43 (the user: «отражение должно быть в калюжах, а не на песке под дождём»): in the rain only the puddles
    // mirror - the wet ground round them darker, no mirror
    float a = rain ? 0.0 : shine * weight * twinX * (0.35 + 0.65 * saturate(fresnel * 4.0));
    // 0.97, wet ground (twinY): the ground a little darker all over, and puddles - patches of noise over the world,
    // the same wherever the camera goes - mirroring nearly as water does.
    float dark = 0.0;
    if (twinY > 0.0) {
        float3 world = p.x * float3(r0, r1, r2) + p.y * float3(r3, r4, r5) + p.z * float3(r6, r7, r8) +
                       float3(r9, r10, r11);
        // 2.7.43: puddles only where the ground is hard - paving, stone, trodden earth: grey or dark, not much
        // colour; sand, grass and snow take the water in. Smaller (about 12 units, not 22) and fewer.
        float3 ground = sceneColor.Load(int3(wp, 0)).rgb;
        float top = max(ground.r, max(ground.g, ground.b)), low = min(ground.r, min(ground.g, ground.b));
        float tint = (top - low) / max(top, 1e-3);
        float light = dot(ground, float3(0.3, 0.59, 0.11));
        float hard = smoothstep(0.32, 0.18, tint) * (1.0 - smoothstep(0.62, 0.78, light));
        float n0 = valueNoise(world.xz * 0.08) * 0.7 + valueNoise(world.xz * 0.3 + 3.1) * 0.3;
        float puddle = smoothstep(0.6, 0.68, n0) * shine * hard;
        // 2.7.36 (the tester in Comodo: «меняется только отражение, а так визуально ничего не меняется»): the wet
        // ground darker - 16 % was hardly seen - and a puddle shows the sky where nothing close stands in it; 2.7.43:
        // a darker sky, and much of it only looking along the puddle - big grey stains lay over the sand
        dark = (0.3 + 0.15 * puddle) * wet * twinY;
        float3 sky = float3(0.42, 0.46, 0.52) * lerp(1.0, 0.15, twinW);
        float pa = puddle * (0.2 + 0.6 * saturate(fresnel * 3.0) + 0.3 * weight) * twinY;
        if (pa > a) {
            c = lerp(sky, c, weight);
            a = pa;
        }
        // 1.7: rings where the rain's drops fall into the puddles (sharpen the time); 2.7.36: thicker - a twentieth
        // of a unit was lost at the pass's half size
        float2 rc = world.xz * 0.6;
        float2 cid = floor(rc);
        float2 cf = frac(rc) - (0.3 + 0.4 * float2(hash12(cid + 1.3), hash12(cid + 4.1)));
        float age = frac(sharpen * 0.9 + hash12(cid));
        float ring = exp(-pow((length(cf) - age * 0.45) * 12.0, 2.0)) * (1.0 - age) * puddle * twinY;
        c = lerp(c, float3(0.92, 0.95, 1.0) * lerp(1.0, 0.35, twinW), saturate(ring * 0.7));
        a = max(a, ring * 0.45);
    }
    // Premultiplied over: the ground darkened by `dark`, then the reflection over it by `a`.
    return float4(c * a, 1.0 - (1.0 - dark) * (1.0 - a));
}

// 0.86: a half-size premultiplied picture (t0) laid over the world, smoothly (uiPixels.xy the world's size).
float4 half_over_ps(PresentOut i) : SV_Target {
    float4 c = tex0.SampleLevel(samp0, i.pos.xy / uiPixels.xy, 0);
    if (c.a < 0.003) discard;
    return c;
}

// 0.35, heat haze (heat.cpp): where the hot air rises - over the torches' flames, burning skills, lava, and far off
// in the desert - the picture is read from its copy (t2) a little aside, the aside drifting up. t1 holds a texel a
// spot - its middle (target pixels) .xy, radius .z, strength .w - footprint how many; twinX the desert's (0 none, by
// the depth, t3), sharpen the time, scale k, uiPixels.xy the world's size.
float4 heat_ps(PresentOut i) : SV_Target {
    float2 p = i.pos.xy;
    float heat = 0.0;
    int count = int(footprint);
    float2 swirl = float2(0.0, 0.0);
    float t = sharpen;
    [loop] for (int k = 0; k < count; ++k) {
        float4 s = tex1.Load(int3(k, 0, 0));
        float2 d = p - s.xy;
        if (s.w < 0.0) {  // 0.38: a portal - the air turns about it, faster near its middle
            float r = length(d) / max(s.z, 1.0);
            float f = saturate(1.0 - r);
            float turn = -s.w * f * f * (0.8 + 0.2 * sin(t * 1.5 + r * 6.0));  // 0.39.1: steadier
            swirl += float2(-d.y, d.x) / max(length(d), 1.0) * turn * 6.0 * scale;
            continue;
        }
        d.y *= d.y > 0.0 ? 2.0 : 0.6;  // further above than below
        float f = saturate(1.0 - dot(d, d) / max(s.z * s.z, 1.0));
        heat += s.w * f * f;
    }
    if (twinX > 0.0) heat += twinX * smoothstep(250.0, 900.0, viewDepth(sceneDepth.Load(int3(int2(p), 0))));
    if (heat < 0.002 && dot(swirl, swirl) < 0.01) discard;
    float2 q = p / scale * 0.06;
    float2 n = float2(fbm4(q + float2(0.0, t * 1.4)), fbm4(q * 1.3 + float2(4.1, t * 1.7))) - 0.5;
    float2 aside = n * saturate(heat) * 4.0 * scale + swirl;
    return float4(sceneColor.SampleLevel(samp0, (p + aside) / uiPixels.xy, 0).rgb, 1.0);
}

// 0.39, the driver's own weather (weather.cpp): three layers of particles, far to near, falling (or blown) through
// cells of the screen - a particle a cell, some cells empty - each layer moved by the camera's moves (sourceRect.xy,
// game pixels) the more the nearer. twinX the kind (1 snow, 2 rain, 3 leaves, 4 sand; 0.70: 5 pollen, 6 dust - motes
// drifting in the air), footprint how much (1 = 100 %),
// sharpen the time, scale k. Blended over the picture by its alpha.
// 3.0.5-beta.19 (the user: «делай» - R-CRSXXA: the pollen still 2.37 ms of a Radeon 610M's 22 on «Середні» with its
// far particles left out: the full-screen pass's own work for each of two million pixels - its three layers' cells,
// the kind's numbers, the presence's hash): the particles as small quads (weather_vs - six vertices a cell, the
// layers far to near - each its particle's reach within its own cell), shaded by weather_quad_ps as weather_ps does
// (one function): the pixels near a particle alone. Sand keeps weather_ps: its air lies over the whole picture.

// A layer of the kind: how near it is (depth), its cells' side, its fall, how far the wind blows it, how many of its
// cells hold a particle
struct WeatherLayer {
    float depth;
    float cell;
    float2 fall;
    float blown;
    float presence;
};

WeatherLayer weatherLayer(int kind, int layer, float t) {
    WeatherLayer w;
    w.depth = 0.6 + 0.5 * float(layer);
    w.cell = (kind == 1 ? 36.0 : kind == 2 ? 22.0 : kind == 3 ? 110.0 : kind == 4 ? 60.0 : 80.0) * w.depth;
    w.fall = kind == 1 ? float2(8.0 * sin(t * 0.21), 34.0)
             : kind == 2 ? float2(-55.0, 520.0)
             : kind == 3 ? float2(18.0, 26.0)
             : kind == 4 ? float2(-300.0, 14.0)
                         : float2(5.0, kind == 5 ? -3.0 : 2.0);
    // 1.22: the shared wind blows it along the screen (sourceRect.z how far so far, .w how fast now)
    w.blown = kind == 1 ? 1.0 : kind == 2 ? 0.6 : kind == 3 ? 1.6 : kind == 4 ? 2.5 : 0.4;
    // 2.7.35: a few more leaves (the tester in Comodo: «Листопад плохо заметно»)
    w.presence = kind == 3 ? 0.24 : kind == 4 ? 0.35 : kind >= 5 ? 0.3 : 0.55;
    return w;
}

// a picture's place (game pixels) plus this: its place in the layer's cells
float2 weatherShift(WeatherLayer w, float t) {
    return sourceRect.xy * w.depth - w.fall * w.depth * t - float2(sourceRect.z * w.blown, 0.0) * w.depth;
}

// 3.0.5-beta.18 (R-H3QJBG): how far from its middle a particle reaches - its shape and its own motion at their most;
// past it a pixel gets under 1e-4 of it (a leaf's: none). x and y apart (weather_vs's quad), the kind's own test in
// weatherShare. fall: the layer's with the wind (rain's way).
float2 weatherReach(int kind, float depth, float2 fall) {
    if (kind == 1) return (3.01 * (0.9 + 0.8 * depth) + 4.0 * depth).xx;  // the flake and its sway
    if (kind == 2) {  // the streak, 10 depth along its way, its across under 1e-4 within sqrt(2.83 depth)
        float2 way = abs(normalize(fall));
        float along = 10.0 * depth, across = sqrt(2.83 * depth);
        return float2(along * way.x + across * way.y, along * way.y + across * way.x);
    }
    if (kind == 3) return float2(max(7.0 * depth, 1.2), 4.4 * depth);  // the leaf at its widest
    if (kind == 4) return float2(70.02 * depth, 2.43 * depth);         // the ellipse under 1e-4
    return (3.0 * (0.45 + 0.35 * depth) + 8.49 * depth).xx;            // the mote and its wander
}

// The particle of the cell of the layer that the place q (the picture's place plus weatherShift) lies in: its share
// of the pixel (alpha; 0 none) and its colour. lit, litColour: the dust's light (weather_ps)
float weatherShare(int kind, int layer, WeatherLayer w, float2 q, float t, float lit, float3 litColour,
                   out float3 colour) {
    colour = float3(1.0, 1.0, 1.0);
    float2 id = floor(q / w.cell);
    float2 f = frac(q / w.cell);
    if (hash12(id + float2(91.0, 17.0) * float(layer + 1)) > w.presence * footprint) return 0.0;
    float2 h = float2(hash12(id + float(layer) * 17.0), hash12(id * 1.7 + float(layer) * 3.1));
    float2 d = (f - (0.2 + 0.6 * h)) * w.cell;  // from the particle, game pixels
    float2 fall = w.fall + float2(sourceRect.w * w.blown, 0.0);
    float depth = w.depth;
    // 3.0.5-beta.18: a pixel the particle does not reach (weatherReach) goes on before the sines and the exp
    bool close;
    if (kind == 1) {
        float reach = 3.01 * (0.9 + 0.8 * depth) + 4.0 * depth;
        close = dot(d, d) <= reach * reach;
    } else if (kind == 2) {
        float2 way = normalize(fall);
        float along = dot(d, way), across = dot(d, float2(-way.y, way.x));
        close = abs(along) < 10.0 * depth && across * across <= 2.83 * depth;
    } else if (kind == 3) {
        close = abs(d.x) < max(7.0 * depth, 1.2) && abs(d.y) < 4.4 * depth;
    } else if (kind == 4) {
        float2 e = d / float2(26.0 * depth, 0.9 * depth);
        close = dot(e, e) <= 7.25;
    } else {
        float reach = 3.0 * (0.45 + 0.35 * depth) + 8.49 * depth;
        close = dot(d, d) <= reach * reach;
    }
    float share = 0.0;
    [branch] if (close) {
        float shape = 0.0;
        float alpha = 0.0;
        if (kind == 1) {  // snow: a soft flake, swaying
            d.x += sin(t * 1.3 + h.x * 6.28) * 4.0 * depth;
            float r = 0.9 + 0.8 * depth;
            shape = exp(-dot(d, d) / (r * r));
            alpha = 0.85;
        } else if (kind == 2) {  // rain: a thin streak along its fall
            float2 dir = normalize(fall);
            float along = dot(d, dir), across = dot(d, float2(-dir.y, dir.x));
            shape = exp(-across * across / (0.35 * depth)) * saturate(1.0 - abs(along) / (10.0 * depth));
            colour = float3(0.78, 0.83, 0.92);
            alpha = 0.32;
        } else if (kind == 3) {  // a leaf: tumbling as it falls, its width turning
            // 2.7.35: twice as big - 7 by 4 game pixels in the middle layer read as orange specks at 1080p
            float turn = abs(sin(t * 2.1 + h.y * 6.28));
            float2 e = d / float2(max(7.0 * depth * turn, 1.2), 4.4 * depth);
            shape = saturate(1.0 - dot(e, e)) * 1.5;
            colour = h.x < 0.4    ? float3(0.86, 0.45, 0.13)
                     : h.x < 0.75 ? float3(0.58, 0.36, 0.16)
                                  : float3(0.62, 0.66, 0.2);
            alpha = 0.92;
        } else if (kind == 4) {  // sand: a streak blown across
            float2 e = d / float2(26.0 * depth, 0.9 * depth);
            shape = exp(-dot(e, e));
            colour = float3(0.86, 0.76, 0.56);
            alpha = 0.14;
        } else {  // a mote: wandering slowly, now catching the light, now not
            d += float2(sin(t * 0.37 + h.x * 6.28), cos(t * 0.29 + h.y * 6.28)) * 6.0 * depth;
            float r = 0.45 + 0.35 * depth;
            shape = exp(-dot(d, d) / (r * r)) * (0.55 + 0.45 * sin(t * (0.8 + h.y) + h.x * 20.0));
            colour = kind == 5 ? float3(1.0, 0.95, 0.75) : litColour;
            alpha = kind == 5 ? 0.55 : min(0.35 * lit, 0.8);
        }
        share = saturate(shape) * alpha;
    }
    return share;
}

// 1.23, dust in the light (twinY): indoors the motes show where light falls on what is behind them (the picture's
// copy, t0) - in the beams, by the torches - and hardly in the dark; tinted by that light. 2.6.20: lit, a mote is
// lighter than what is behind it, and a third of it stays in the dark (by a torch's yellow wall the motes took the
// wall's own colour and the dark put them out - on, the dust was gone)
void weatherLight(int kind, float2 at, out float lit, out float3 litColour) {
    lit = 1.0;
    litColour = float3(0.85, 0.80, 0.70);
    if (twinY > 0.0 && kind == 6) {
        float3 under = tex0.Load(int3(int2(at), 0)).rgb;
        float l = dot(under, float3(0.299, 0.587, 0.114));
        lit = lerp(1.0, 0.3 + 2.1 * smoothstep(0.12, 0.55, l), twinY);
        litColour = lerp(litColour, saturate(under * 1.6 + 0.35), 0.6 * twinY);
    }
}

float4 weather_ps(PresentOut i) : SV_Target {
    float2 p = i.pos.xy / scale;  // game pixels
    float t = sharpen;
    int kind = int(twinX + 0.5);
    float3 sum = float3(0.0, 0.0, 0.0);
    float a = 0.0;
    float lit;
    float3 litColour;
    weatherLight(kind, i.pos.xy, lit, litColour);
    [unroll] for (int layer = 0; layer < 3; ++layer) {
        WeatherLayer w = weatherLayer(kind, layer, t);
        float3 colour;
        float s = weatherShare(kind, layer, w, p + weatherShift(w, t), t, lit, litColour, colour);
        sum += colour * s;
        a += s;
    }
    if (kind == 4) {  // and the air itself a little sandy
        sum += float3(0.86, 0.76, 0.56) * 0.05 * footprint;
        a += 0.05 * footprint;
    }
    if (a < 0.003) discard;
    return float4(sum / a, saturate(a));
}

// 3.0.5-beta.19: a cell of a layer each six vertices - the layers one after another, far to near (r0, r1: the first
// cells of layers 1 and 2; r2-r4: each layer's cells across, from its cell under the picture's corner); a quad over its
// particle's reach within its own cell, none for an empty cell
struct WeatherQuad {
    float4 pos : SV_Position;
    nointerpolation float4 cell : TEXCOORD0;  // the cell (xy) and its layer (z)
};

WeatherQuad weather_vs(uint id : SV_VertexID) {
    WeatherQuad o;
    o.pos = float4(2.0, 2.0, 0.5, 1.0);  // past the target's right: nothing drawn
    o.cell = float4(0.0, 0.0, 0.0, 0.0);
    uint n = id / 6, v = id % 6;
    uint layer = n < uint(r0) ? 0 : n < uint(r1) ? 1 : 2;
    uint local = n - (layer == 0 ? 0 : layer == 1 ? uint(r0) : uint(r1));
    uint across = uint(layer == 0 ? r2 : layer == 1 ? r3 : r4);
    int kind = int(twinX + 0.5);
    float t = sharpen;
    WeatherLayer w = weatherLayer(kind, int(layer), t);
    float2 shift = weatherShift(w, t);
    float2 cell = floor(shift / w.cell) + float2(float(local % across), float(local / across));
    if (hash12(cell + float2(91.0, 17.0) * float(layer + 1)) > w.presence * footprint) return o;  // an empty cell
    float2 h = float2(hash12(cell + float(layer) * 17.0), hash12(cell * 1.7 + float(layer) * 3.1));
    float2 middle = (cell + 0.2 + 0.6 * h) * w.cell - shift;  // the picture's game pixels
    float2 reach = weatherReach(kind, w.depth, w.fall + float2(sourceRect.w * w.blown, 0.0));
    float2 lo = max(middle - reach, cell * w.cell - shift), hi = min(middle + reach, (cell + 1.0) * w.cell - shift);
    if (lo.x >= hi.x || lo.y >= hi.y) return o;
    float2 corner = float2(v == 1 || v == 4 || v == 5 ? hi.x : lo.x, v == 2 || v == 3 || v == 5 ? hi.y : lo.y);
    float2 px = corner * scale;  // target pixels
    o.pos = float4(px.x * 2.0 / uiPixels.x - 1.0, 1.0 - px.y * 2.0 / uiPixels.y, 0.0, 1.0);
    o.cell = float4(cell, float(layer), 0.0);
    return o;
}

// 3.0.5-beta.19: a pixel of a particle's quad - weather_ps's share of its layer, for a pixel of its own cell only
// (the cell beside it gives its pixels its own particle); blended over by its alpha
float4 weather_quad_ps(WeatherQuad i) : SV_Target {
    float2 p = i.pos.xy / scale;  // game pixels
    float t = sharpen;
    int kind = int(twinX + 0.5);
    int layer = int(i.cell.z + 0.5);
    WeatherLayer w = weatherLayer(kind, layer, t);
    float2 q = p + weatherShift(w, t);
    if (any(floor(q / w.cell) != i.cell.xy)) discard;
    float lit;
    float3 litColour;
    weatherLight(kind, i.pos.xy, lit, litColour);
    float3 colour;
    float s = weatherShare(kind, layer, w, q, t, lit, litColour, colour);
    if (s < 0.003) discard;
    return float4(colour, s);
}

// 0.64, the rain's splashes: rings spreading where drops land, fixed on the ground - each pixel's place in the world
// from the picture's depth (t3) and the camera backwards (r0-r11, the projection f0-farZ) - on what faces up only,
// each patch of the ground its own drop now and then. sharpen the time, footprint how much rain.
float4 splash_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) discard;  // nothing drawn there
    int2 last = int2(uiPixels.xy) - 1;
    float3 p = viewAt(wp);
    float3 r = viewAt(min(wp + int2(1, 0), last)), d = viewAt(min(wp + int2(0, 1), last));
    float3 n = normalize(cross(d - p, r - p));
    float3 nw = n.x * float3(r0, r1, r2) + n.y * float3(r3, r4, r5) + n.z * float3(r6, r7, r8);
    if (abs(nw.y) < 0.8) discard;  // a wall, a tree trunk: no splash
    float3 w = p.x * float3(r0, r1, r2) + p.y * float3(r3, r4, r5) + p.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    float2 g = w.xz / 3.0;  // patches of three world units
    float2 id = floor(g);
    if (hash12(id + float2(7.3, 1.9)) > 0.3 * footprint) discard;
    float phase = frac(sharpen / 0.8 + hash12(id));
    float2 o = (float2(hash12(id + 1.1), hash12(id + 2.2)) - 0.5) * 0.5;
    float dist = length(frac(g) - 0.5 - o);
    float radius = phase * 0.42;
    float ring = exp(-(dist - radius) * (dist - radius) / 0.0012) * (1.0 - phase);
    if (ring < 0.02) discard;
    return float4(0.86, 0.9, 1.0, saturate(ring * 0.4));
}

// 0.65, the waterfalls the maps' makers placed (821: bra_fild01's), which uaRO.exe never shows (waterfalls.cpp): a
// sheet of falling water - streaks running down it, foam and mist at its foot - hidden behind what stands nearer (the
// picture's depth, t3). uv: across 0-1, down 0-1; az.x how much, az.y the sheet's view depth; sharpen the time.
float4 waterfall_ps(ShadowIn i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    float z = viewDepth(sceneDepth.Load(int3(wp, 0)));
    if (z < i.az.y - 4.0) discard;  // a rock, a tree in front of it
    float u = i.uv.x, v = i.uv.y, t = sharpen;
    float lane = floor(u * 36.0);
    float speed = 1.1 + 0.9 * hash12(float2(lane, 3.0));
    float fall = frac(v * 2.5 - t * speed + hash12(float2(lane, 9.0)));
    float streak = smoothstep(0.0, 0.2, fall) * (1.0 - smoothstep(0.5, 1.0, fall));
    float edge = smoothstep(0.0, 0.15, u) * smoothstep(0.0, 0.15, 1.0 - u) * smoothstep(0.0, 0.05, v);
    float foam = smoothstep(0.7, 1.0, v) * (0.75 + 0.25 * sin(t * 3.0 + u * 23.0));
    float a = ((0.3 + 0.35 * streak) * i.az.x + foam * 0.45) * edge;
    if (a < 0.01) discard;
    float3 c = lerp(float3(0.58, 0.74, 0.86), float3(0.94, 0.97, 1.0), saturate(streak * 0.6 + foam));
    return float4(c, saturate(a));
}

// The mask softened: nine taps along sourceRect.xy (across, then down), uiPixels.zw the mask's size.
float4 shadow_blur_ps(PresentOut i) : SV_Target {
    float2 step = sourceRect.xy / uiPixels.zw;
    float2 uv = i.pos.xy / uiPixels.zw;
    const float w[5] = {0.227, 0.194, 0.122, 0.054, 0.016};
    float m = tex0.SampleLevel(samp0, uv, 0).r * w[0];
    [unroll] for (int k = 1; k < 5; ++k)
        m += (tex0.SampleLevel(samp0, uv + step * float(k), 0).r + tex0.SampleLevel(samp0, uv - step * float(k), 0).r) *
             w[k];
    return float4(m, m, m, m);
}

// Multiplied into the picture (blend ZERO, SRC_COLOR): what light the shadows leave, sharpen their darkness.
float4 shadow_apply_ps(PresentOut i) : SV_Target {
    float m = tex0.SampleLevel(samp0, i.pos.xy / uiPixels.xy, 0).r;
    float f = 1.0 - saturate(m) * sharpen;
    return float4(f, f, f * 1.02, 1.0);
}

GlowOut lava_glow_ps(PsIn i) {
    GlowOut o;
    float glow;
    float3 c = lavaShade(i, glow);
    o.c = float4(c, 1.0);
    o.g = float4(c * glow, 1.0);  // its glow into the glow target: the lava shines
    return o;
}

GlowOut ray_glow_ps(PsIn i) {  // 0.39.2: a beam is light - its glow its colour (the choice made FXC fail inside)
    GlowOut o;
    o.c = rayShade(i);
    o.g = o.c;
    return o;
}

// 0.28, the fireflies (torches.cpp): a glowing dot and its soft halo, added to the picture - sourceRect.xy its middle
// and .z its core's radius (target pixels), sourceClamp.y its view depth, fogColor its colour, twinX its brightness
// (its flash, the game's fade, the player's setting). Hidden behind what stands in front of it by the picture's depth.
// 0.29: also the maps' hidden glows, softer - sourceRect.w how sharp the core is (a firefly's 1.2), twinY how much
// halo (a firefly's 1).
float4 firefly_ps(PresentOut i) : SV_Target {
    float2 d = i.pos.xy - sourceRect.xy;
    float r = max(sourceRect.z, 0.5);
    float d2 = dot(d, d) / (r * r);
    float z = viewDepth(sceneDepth.Load(int3(int2(i.pos.xy), 0)));
    float seen = saturate((z - sourceClamp.y + 1.5) / 1.5);
    float core = exp(-d2 * sourceRect.w);
    float halo = (0.22 * exp(-d2 / 10.0) + 0.05 / (1.0 + d2 / 4.0)) * twinY;
    float fade = saturate(1.0 - d2 / 64.0);  // none at eight radii, the edge of its pass
    float3 c = fogColor.rgb * halo + lerp(fogColor.rgb, float3(1.0, 1.0, 1.0), 0.55) * core;
    return float4(c * (twinX * seen * fade), 0.0);
}

// 0.29, the towers' dust (torches.cpp): motes drifting about a place, each a tiny soft dot that twinkles -
// sourceRect.xy the cloud's middle (target pixels), .z pixels a world unit there, .w its radius (world units);
// sourceClamp.y its view depth, .w its seed; uiPixels.w the time; fogColor the motes' colour, twinX their brightness.
float4 dust_ps(PresentOut i) : SV_Target {
    float2 pos = i.pos.xy;
    float z = viewDepth(sceneDepth.Load(int3(int2(pos), 0)));
    float seen = saturate((z - sourceClamp.y + 6.0) / 6.0);
    float t = uiPixels.w;
    float r = max(0.12 * sourceRect.z, 0.7);  // a mote: a tenth of a unit, at least most of a pixel
    float sum = 0.0;
    [loop] for (int k = 0; k < 24; ++k) {
        float fk = float(k);
        float2 h = float2(hash12(float2(sourceClamp.w * 31.0 + fk, 1.7)),
                          hash12(float2(fk * 1.3, sourceClamp.w * 17.0)));
        float2 dir = float2(cos(h.x * 6.2832), sin(h.x * 6.2832)) * sqrt(h.y) * 0.8;
        float2 drift = float2(sin(t * (0.11 + 0.05 * h.y) + fk), cos(t * (0.09 + 0.04 * h.x) + fk * 1.7)) * 0.25;
        float2 d = pos - (sourceRect.xy + (dir + drift) * sourceRect.w * sourceRect.z);
        float twinkle = 0.35 + 0.65 * saturate(sin(t * (0.8 + h.x) + fk * 2.1) * 0.5 + 0.5);
        sum += exp(-dot(d, d) / (r * r)) * twinkle;
    }
    return float4(fogColor.rgb * (sum * twinX * seen), 0.0);
}

GlowOut smoke_glow_ps(PsIn i) {
    GlowOut o;
    o.c = smokeShade(i);
    o.g = ((alphaTest >> 19) & 1) != 0 ? o.c : float4(0.0, 0.0, 0.0, o.c.a);
    return o;
}

)HLSL";
}

}  // namespace uagfx::gpu
