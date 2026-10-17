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
// 1.16: the world mirrored under the water (mirror.cpp), for water_ps - the table's fourth
Texture2D mirrorTex : register(t5);
SamplerState samp0 : register(s0);
SamplerState samp1 : register(s1);
// The distance from the camera of a depth the game's projection gave (water, the effects).
float viewDepth(float d) { return farZ * nearZ / (farZ - d * (farZ - nearZ)); }

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

// 1.7: a gust's strength at game x and time t (0-1): a band about 250 pixels wide crossing the screen from the left
// every 9 s or so, stronger and weaker in turn.
float gustAt(float x, float t) {
    float band = pow(saturate(sin(x * 0.0042 - t * 0.7)), 8.0);
    return band * (0.55 + 0.45 * sin(t * 0.23));
}

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
        // 1.7, gusts (uiPixels.w): a band of wind crossing the screen from the left now and then (grass_ps alike).
        // 1.22: the shared wind's - its sign the side it comes from, its size how strong
        float across = uiPixels.w < 0.0 ? 4096.0 - i.pos.x : i.pos.x;
        wave += uiPixels.w * gustAt(across, twinH) * 2.6;
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
float4 smoothLightmap(float2 uv, float4 sharp) {
    float2 t = uv * 256.0;
    float2 lo = floor(t / 8.0) * 8.0 + 0.5;
    float2 hi = lo + 7.0;
    float4 sum = float4(0.0, 0.0, 0.0, 0.0);
    float weights = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x) {
            float w = (x == 0 ? 2.0 : 1.0) * (y == 0 ? 2.0 : 1.0);
            sum += tex1.SampleLevel(samp1, clamp(t + float2(x, y) * 1.5, lo, hi) / 256.0, 0) * w;
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
    float2 o = k / size;
    const float3 luma = float3(0.2126, 0.7152, 0.0722);
    float hl = dot(tex0.SampleGrad(samp0, uv - float2(o.x, 0.0), du, dv).rgb, luma);
    float hr = dot(tex0.SampleGrad(samp0, uv + float2(o.x, 0.0), du, dv).rgb, luma);
    float hu = dot(tex0.SampleGrad(samp0, uv - float2(0.0, o.y), du, dv).rgb, luma);
    float hd = dot(tex0.SampleGrad(samp0, uv + float2(0.0, o.y), du, dv).rgb, luma);
    // The height across a game texel against the picture's texels a screen pixel (tx, ty, k times as many) and their
    // area (k squared): k goes out, the relief the game's texture gives.
    float2 g = float2(hr - hl, hd - hu) * 0.5;
    float2 tx = du * size, ty = dv * size;  // texels a screen pixel
    float area = abs(tx.x * ty.y - tx.y * ty.x);
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
// 1.17, living characters (draw.cpp spriteConstants): a sprite of a character or a monster lit by the sun from the
// side its picture's own shape gives - its silhouette's slope a few texels around as the way its edge faces: the side
// towards the sun (twinX, twinY on the screen, twinW its height) brighter with a warm rim, the far side a little
// darker. uiPixels.x the strength, .y the sun's light here.
float3 spriteLight(float2 uv, float2 du, float2 dv, float3 c) {
    float w, h;
    tex0.GetDimensions(w, h);
    float2 size = float2(w, h);
    float2 sx = du / max(length(du * size), 1e-4) * 2.0;  // two texels along the screen's x, then its y
    float2 sy = dv / max(length(dv * size), 1e-4) * 2.0;
    float l = tex0.SampleGrad(samp0, uv - sx, du, dv).a, r = tex0.SampleGrad(samp0, uv + sx, du, dv).a;
    float u = tex0.SampleGrad(samp0, uv - sy, du, dv).a, d = tex0.SampleGrad(samp0, uv + sy, du, dv).a;
    float2 out2 = -float2(r - l, d - u);  // from the inside out, screen axes (y down)
    float edge = saturate(length(out2) * 1.5);
    float3 n = normalize(float3(out2 * 1.2, 0.8));
    float2 ls = float2(twinX, twinY);
    float3 L = normalize(float3(ls * sqrt(saturate(1.0 - twinW * twinW)), 0.35 + twinW));
    float sun = uiPixels.y;
    float amb = lerp(0.85, 0.55, sun);
    float k = (amb + (1.0 - amb) * saturate(dot(n, L))) / (amb + (1.0 - amb) * L.z);
    float rim = pow(saturate(dot(normalize(out2 + 1e-5), normalize(ls + 1e-5))), 2.0) * edge * sun;
    float s = uiPixels.x;
    c *= lerp(1.0, clamp(k, 0.75, 1.3), s);
    c += float3(1.0, 0.9, 0.7) * rim * 0.22 * s;
    return c;
}

// 1.7, the trees (draw_constants.hpp leafShading): a draw of leaves, its box sourceRect (game pixels). uiPixels.xy
// the way to the sun on the screen, .z the crown's roundness (tree_volume) - lit as a ball from the sun's side, darker
// in its middle; r4 the light through the leaves (leaf_light): a leaf with a hole a few pixels towards the sun is on
// the crown's edge and glows warm green; sourceClamp.rgb the tree's own shade, r0-r2 the autumn's colour and r3 how
// much (tree_tints), r5 rime on the crown's top.
// 1.24: a smooth noise over the crown's own box (q: -1 to 1 across it) - the same patches however near the camera
float leafNoise(float2 p) {
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float b = frac(sin(dot(i + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
    float c = frac(sin(dot(i + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float d = frac(sin(dot(i + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float3 leafLight(float2 pos, float2 uv, float2 du, float2 dv, float3 c) {
    float2 lo = sourceRect.xy * scale + 0.5 + float2(jitterX, jitterY);
    float2 hi = sourceRect.zw * scale + 0.5 + float2(jitterX, jitterY);
    float2 q = clamp((pos - (lo + hi) * 0.5) / max((hi - lo) * 0.5, 1.0), -1.0, 1.0);
    float2 sun = uiPixels.xy;
    float luma = dot(c, float3(0.299, 0.587, 0.114));
    c *= sourceClamp.rgb;
    // 1.24: clumps of the crown each a little their own - lighter and yellower, deeper and bluer - over the box, the
    // tree's own patches (its tint the seed): the same picture repeated on every layer of leaves shows less
    float2 seed = sourceClamp.rg * 37.0;
    float clump = leafNoise(q * 3.2 + seed) * 0.65 + leafNoise(q * 7.5 + seed * 1.7) * 0.35;
    c *= lerp(float3(0.86, 0.9, 0.98), float3(1.1, 1.08, 0.94), clump);
    // 1.8.2: golden, not brown; 1.24: in patches - some of the crown still green - and by the leaf's own light
    // (luma times the colour: the shade between the leaves stays, the crown is no flat orange)
    if (r3 > 0.0) {
        float turned = r3 * smoothstep(0.25, 0.65, leafNoise(q * 2.4 + seed * 0.7 + 5.3));
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
// patches of shade over the world (r0-r11 the view -> the world) so that one tile's repeats do not show.
float mhash(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float mnoise(float2 p) {
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(mhash(i), mhash(i + float2(1, 0)), f.x), lerp(mhash(i + float2(0, 1)), mhash(i + 1.0), f.x), f.y);
}

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
#ifndef UAGFX_SAFE
    // 1.8: the point in the view and its surface's slopes, also outside any branch (garbage where unused)
    float pz = i.pos.w;
    float2 pg = (i.pos.xy - 0.5 - float2(jitterX, jitterY)) / max(scale, 1e-3);
    float3 P = float3((pg.x - cx) * pz / max(f0, 1e-3), (pg.y - cy) * pz / max(f1, 1e-3), pz);
    float3 dp1 = ddx(P), dp2 = ddy(P);
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
    if (((stage1.y >> 25) & 1) != 0) t1 = smoothLightmap(uv1, t1);  // 0.22.6: the ground's lightmap
    if (colorKey != 0) {  // colour-keyed textures carry the key as alpha 0 (TextureLevels, Task 10)
        if (((stage0.y >> 24) & 1) != 0 && t0.a == 0) discard;
        if (((stage1.y >> 24) & 1) != 0 && t1.a == 0) discard;
    }
    float4 c = i.diffuse;  // stage 0 off: diffuse
    if ((stage0.x & 255) != 1) {
        c = stage(stage0, i.diffuse, i.specular, i.diffuse, t0);
        if ((stage1.x & 255) != 1) c = stage(stage1, i.diffuse, i.specular, c, t1);
    }
#ifndef UAGFX_SAFE
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
        c.rgb *= (0.88 + 0.24 * mnoise(w.xz * 0.012)) * float3(1.0 + warm * 0.6, 1.0 + warm * 0.15, 1.0 - warm * 0.5);
    }
    // 1.9, a strip across a seam of the ground (seams.cpp): the other cell's texture where the noise over the world
    // says: one wandering line between the two textures.
    if (((stage1.y >> 31) & 1) != 0) {
        float3 w = P.x * float3(r0, r1, r2) + P.y * float3(r3, r4, r5) + P.z * float3(r6, r7, r8) +
                   float3(r9, r10, r11);
        float n = mnoise(w.xz * 0.22) * 0.65 + mnoise(w.xz * 0.7 + 5.1) * 0.35;
        // 1.9.5: diffuse alpha how much the place is the other's (bilinear between the cells' middles: corners
        // round off), the noise up on one side and down on the other - one line both sides agree on
        float side = i.specular.g > 0.5 ? 1.0 : -1.0;
        bool own = i.specular.r > 0.5;  // 1.9.2: the cell's own band at the edge, its rim covered
        float other = smoothstep(-0.05, 0.05, i.diffuse.a - 0.5 + side * (n - 0.5) * 0.9);
        // 1.25: the own band whole at the edge (drawn first, applySeams) - partly see-through, the texture's dark rim
        // under it showed through the neighbour's strip as a line along the cells (the desert's "cracks")
        // 2.0.2: whole over the rim only, then fading out - whole over all of it, its mirror's axis showed
        c.a = own ? 1.0 - smoothstep(0.3, 0.9, i.specular.b) : other;
    }
#endif
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
    // 0.67, soft particles: an effect fades over the last few units before what is behind it (the frame's depth so
    // far, sceneDepth) - no hard line where it goes into the ground; its colour too when it adds light (bit 21).
#ifndef UAGFX_SAFE
    if (((alphaTest >> 20) & 1) != 0) {
        float behind = viewDepth(sceneDepth.Load(int3(int2(i.pos.xy), 0))) - viewDepth(i.pos.z);
        float fade = smoothstep(0.0, 1.0, saturate(behind / 7.0));
        c.a *= fade;
        if (((alphaTest >> 21) & 1) != 0) c.rgb *= fade;
    }
#endif
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

// 1.7, the water's life (water(): twinH's bits): fish - dark shapes circling under the surface, now and then a ring
// where one jumped; a swamp's bubbles swelling and bursting into rings, its water murkier.
float3 waterLife(float3 c, uint life, float2 xz, float t, float thick, float light) {
    // 1.25: fewer, larger and soft - shadows deep under the surface; small and sharp they read as scratches
    if ((life & 1) != 0 && thick > 1.2) {
        float2 cid = floor(xz / 11.0);
        float seed = hash12(cid + 91.7);
        if (seed < 0.16) {
            float sp = (0.2 + 0.25 * hash12(cid + 3.3)) * (seed < 0.08 ? 1.0 : -1.0);
            float ang = t * sp + seed * 40.0;
            float2 pos = (cid + 0.5) * 11.0 + float2(cos(ang), sin(ang)) * 3.0;
            float2 dir = float2(-sin(ang), cos(ang)) * sign(sp);
            float2 d = xz - pos;
            float2 l = float2(dot(d, dir), dot(d, float2(-dir.y, dir.x)));
            l.y -= sin(t * 6.0 + seed * 20.0) * 0.1 * l.x * step(l.x, 0.0);
            float body = smoothstep(1.0, 0.25, length(l / float2(1.1, 0.38)));
            float tail = smoothstep(0.0, 0.3, -0.8 - l.x) * smoothstep(0.0, 0.3, l.x + 1.5) *
                         smoothstep(0.1, 0.0, abs(l.y) - (-0.8 - l.x) * 0.7);
            c *= 1.0 - 0.22 * saturate(body + tail * 0.6) * saturate((thick - 1.2) / 2.0);
            float jump = frac(t * 0.05 + seed * 7.0);
            float x = length(d) - jump / 0.06 * 1.6;
            c += exp(-x * x * 30.0) * saturate(1.0 - jump / 0.06) * 0.35 * light;
        }
    }
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
    c += (pow(ndh, 600.0) * 3.0 + pow(ndh, 60.0) * 0.12) * light * (0.25 + 0.75 * waves) * sun;
    // 0.22.2: foam where the water meets the shore - near a pixel whose ground stands at or above the water there
    // (the water's own depth carried over by its slope on the screen), not over the whole of shallow water -
    // broken by noise that drifts with the waves.
    uint life = uint(twinH + 0.5);  // 1.7: 1 fish, 2 a swamp's bubbles, 4 the surf's swash (water_life)
    float shore = 0.0;
    float swash = (life & 4) != 0 ? 0.5 + 0.5 * sin(t * 0.55 + fp.x * 0.04) : 0.0;
    float reach = (5.0 + 4.0 * swash) * scale;
    [unroll] for (int k = 0; k < 4; ++k) {
        float2 o = k == 0 ? float2(reach, 0.0) : k == 1 ? float2(-reach, 0.0) : k == 2 ? float2(0.0, reach)
                                                                                    : float2(0.0, -reach);
        int2 q = clamp(int2(i.pos.xy + o), int2(0, 0), int2(targetSize) - 1);
        float above = viewDepth(sceneDepth.Load(int3(q, 0))) - (waterZ + dot(o, dz));
        shore = max(shore, saturate(1.0 - above / 0.6));
    }
    shore *= saturate(1.0 - thick / 3.0);
    // 1.25: lines of foam along the shore (the water's depth's lines) rolling in and broken by noise, a thin rim at
    // the edge - the blobs of noise before lay as a row of white dabs
    float broken = smoothstep(0.3, 0.7, valueNoise(fp * 0.9 + t * 0.2) * 0.7 + valueNoise(fp * 2.3 - t * 0.3) * 0.3);
    float rim = smoothstep(0.35, 0.05, thick);
    float roll = frac(thick * 1.3 - t * 0.22);
    float swell = exp(-pow((roll - 0.5) * 9.0, 2.0)) * saturate(1.0 - thick / 2.5);
    float froth = saturate(rim * (0.5 + 0.5 * broken) + swell * broken * 0.7);
    c = lerp(c, float3(0.92, 0.95, 0.97) * light, saturate(shore * 2.0) * froth * (0.55 + 0.3 * swash));
    c = waterLife(c, life, i.world.xz, t, thick, light);
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
    int ratio = int(uiPixels.x / uiPixels.z + 0.5);  // 0.39.3: 2, or 4 for a big world
    int2 last = int2(uiPixels.xy) - 1;
    int2 wp = min(int2(i.pos.xy) * ratio + ratio / 2, last);  // the middle of its pixels
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
    int ratio = int(uiPixels.x / uiPixels.z + 0.5);  // 0.39.3
    float z0 = viewDepth(sceneDepth.Load(int3(min(p * ratio + ratio / 2, int2(uiPixels.xy) - 1), 0)));
    float sum = 0.0, weights = 0.0;
    [unroll] for (int k = -4; k <= 4; ++k) {
        int2 q = clamp(p + dir * k, int2(0, 0), last);
        float zq = viewDepth(sceneDepth.Load(int3(min(q * ratio + ratio / 2, int2(uiPixels.xy) - 1), 0)));
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

// 0.63, night: the world from its copy (t2) darker and colder - the bright stays (a fire, a lamp, a spark), and the
// torches' light (after this) shines out of it. sharpen how much (0-1). 0.74: first the time of day's light (twinX,
// twinY, twinW: its tint, 1 none); 0.79: and the region's look - its tint in that too, its saturation 1 + sourceRect.w.
float4 night_ps(PresentOut i) : SV_Target {
    float3 c = sceneColor.Load(int3(int2(i.pos.xy), 0)).rgb * float3(twinX, twinY, twinW);
    float grey = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = max(lerp(float3(grey, grey, grey), c, 1.0 + sourceRect.w), 0.0);
    c += (c * 1.3 + float3(0.10, 0.11, 0.15)) * sourceRect.x;  // 0.80: a flash of lightning
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    float3 night = lerp(float3(l, l, l), c, 0.55) * float3(0.55, 0.68, 1.0) * 0.45;
    float keep = smoothstep(0.72, 0.97, l);
    return float4(lerp(c, lerp(night, c, keep), sharpen), 1.0);
}

// 0.22, depth of field (tilt-shift): sharp around what the camera looks at - the character in the middle of the
// screen - softer with the distance in depth from it, near and far, as if the map were a model. uiPixels the
// world's size and its blurred copy's (a quarter); footprint where the blur starts (0.22.5: a share of the focus's
// distance), sharpen its strength (0-1); sourceRect.xy the blur's direction.
// 1.3.5: eight by eight of the picture under a tent (sixteen bilinear taps, the middle ones the most) - a box of four
// by four made the far blur swim as the camera followed a walk: each step of the picture across the quarter's grid
// changed which pixels went into which.
float4 dof_down_ps(PresentOut i) : SV_Target {
    float2 c = i.pos.xy * 4.0;
    float2 t = 1.0 / uiPixels.xy;
    float3 s = float3(0.0, 0.0, 0.0);
    float weights = 0.0;
    [unroll] for (int y = 0; y < 4; ++y)
        [unroll] for (int x = 0; x < 4; ++x) {
            float2 o = float2(float(x) * 2.0 - 3.0, float(y) * 2.0 - 3.0);  // -3, -1, 1, 3
            float w = (4.0 - abs(o.x)) * (4.0 - abs(o.y));
            s += sceneColor.SampleLevel(samp0, (c + o) * t, 0).rgb * w;
            weights += w;
        }
    return float4(s / weights, 1.0);
}

// 0.78, bloom (ao.cpp applyBloom): the picture's bright parts at a quarter of its size - over footprint, the brighter
// the more - blurred by dof_blur_ps, then added onto it (bloom_apply_ps: sharpen how much), and a filmic curve over
// all (twinY how much of it): the highlights roll off softly instead of clipping, the shadows keep their depth.
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

// The picture is the game's (not HDR): what passes 0.8 - the bloom added on - rolls off towards white instead of
// clipping, and a light S-curve gives contrast while the middle grey stays where it is.
float3 filmic(float3 c) {
    float3 over = max(c - 0.8, 0.0);
    float3 x = saturate(min(c, 0.8) + 0.2 * (1.0 - exp(-over / 0.2)));
    return lerp(x, x * x * (3.0 - 2.0 * x), 0.18);
}

float4 bloom_apply_ps(PresentOut i) : SV_Target {
    int2 p = int2(i.pos.xy);
    float3 c = sceneColor.Load(int3(p, 0)).rgb;
    float3 glow = tex0.SampleLevel(samp0, i.pos.xy * 0.25 / uiPixels.zw, 0).rgb;
    c += glow * sharpen;
    c = lerp(saturate(c), filmic(c), twinY);
    // 0.83: film grain (twinW how much: a fine noise, new each frame, less in the bright parts) and the world coming
    // in from black after a new map (twinH, 0-1)
    float n = hash12(i.pos.xy + float2(sourceRect.z * 1.618, sourceRect.z * 0.618)) - 0.5;
    c += n * twinW * 0.09 * (1.0 - 0.6 * dot(c, float3(0.2126, 0.7152, 0.0722)));
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

// 0.22.3, the focus (one texel): the median of the depths around the middle of the screen - the character there
// covers the middle itself in some frames of its walk and not in others, the ground around it holds still - and
// carried over from the last frame's (t0) by twinH, this frame's share of the way (0.22.4: by the time since it,
// not by frames). 0.22.4: nothing drawn (the void past a cliff, the sky) counts for nothing - with too few samples
// left the focus stays - and only a jump of a warp or another map is taken at once.
float dof_focus_ps(PresentOut i) : SV_Target {
    int2 mid = int2(uiPixels.xy * 0.5);
    float rad = 26.0 * scale;
    float z[13];
    float valid = 0.0;
    [unroll] for (int k = 0; k < 13; ++k) {
        float a = 6.2831853 * float(k) / 12.0;
        int2 q = k == 0 ? mid : clamp(mid + int2(float2(cos(a), sin(a)) * rad), int2(0, 0), int2(uiPixels.xy) - 1);
        float d = sceneDepth.Load(int3(q, 0));
        bool drawn = d < 0.99999;
        z[k] = drawn ? viewDepth(d) : 1e6;  // sorted past every drawn one
        valid += drawn ? 1.0 : 0.0;
    }
    [unroll] for (int n = 0; n < 12; ++n)  // sorted (bubbles; every index known when compiled)
        [unroll] for (int m = 0; m < 12 - n; ++m) {
            float lo = min(z[m], z[m + 1]);
            z[m + 1] = max(z[m], z[m + 1]);
            z[m] = lo;
        }
    float before = tex0.Load(int3(0, 0, 0)).r;
    // 1.4.2: the depth of the point the camera looks at (the character), when the game's camera could be read (r0).
    if (r0 > 0.0) return before <= 0.0 || abs(r0 - before) > 300.0 ? r0 : lerp(before, r0, twinH);
    if (valid < 4.0) return before;  // too little of the ground in view: as it was (0 until there is a focus)
    float now = z[int(valid) / 2];
    return before <= 0.0 || abs(now - before) > 300.0 ? now : lerp(before, now, twinH);
}

float4 dof_apply_ps(PresentOut i) : SV_Target {
    int2 p = int2(i.pos.xy);
    float3 sharp = sceneColor.Load(int3(p, 0)).rgb;
    float3 soft = tex0.SampleLevel(samp0, i.pos.xy * 0.25 / uiPixels.zw, 0).rgb;
    float focus = tex1.Load(int3(0, 0, 0)).r;  // dof_focus_ps
    float z = viewDepth(sceneDepth.Load(int3(p, 0)));
    // 0.22.5: the sharp zone a share of the focus's distance (footprint), so that zooming the camera out or in
    // keeps the same part of the screen sharp.
    float start = focus * footprint;
    float blur = focus > 0.0 ? smoothstep(start, start * 3.0, abs(z - focus)) * sharpen : 0.0;  // none yet
    return float4(lerp(sharp, soft, blur), 1.0);
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
        float f = 1.0 - d2 / reach2;
        float facing = saturate(dot(n, v * rsqrt(max(d2, 1e-4))) * 0.7 + 0.3);  // wrapped: soft on what turns away
        float a = c.w * f * f / (1.0 + d2 / (0.2 * reach2)) * facing;  // brighter near the source
        // 0.68, rim light (sharpen: its strength): the flat characters and the models' outlines lit more on the side
        // that faces the light - where the depth a few pixels towards the light on the screen is far behind.
        if (sharpen > 0.0) {
            float2 sd = float2(f0 * (v.x * p.z - p.x * v.z), f1 * (v.y * p.z - p.y * v.z));
            float sl = length(sd);
            if (sl > 1e-4) {
                int2 q = clamp(wp + int2(round(sd / sl * 3.0 * scale)), int2(0, 0), last);
                float gap = viewDepth(sceneDepth.Load(int3(q, 0))) - p.z;
                a += c.w * f * smoothstep(3.0, 12.0, gap) * sharpen;
            }
        }
        light += a;
        colour += a * c.rgb;
    }
    float3 sc = sceneColor.Load(int3(wp, 0)).rgb;
    const float3 luma = float3(0.2126, 0.7152, 0.0722);
    float lum = dot(sc, luma);
    float w = (1.0 - exp(-2.4 * light)) * (1.0 - 0.6 * smoothstep(0.3, 0.8, lum));
    if (w < 0.004) discard;
    colour /= max(light, 1e-4);
    float lift = 1.0 + 2.4 * (1.0 - smoothstep(0.08, 0.7, lum));  // dark surfaces lit more, bright ones tinted
    float3 warm = colour * (min(lum * lift, 0.9) / max(dot(colour, luma), 0.05));
    return float4(lerp(sc, saturate(warm), w * 0.9), 1.0);
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

// A flame at q (x across, its half width 1; y up from its base 0 to its tip 1), premultiplied: it rises, sways and
// tears into tongues as noise scrolls up through it, a few sparks rise from it. hw and h are its half width and height
// in pixels (the sparks' size), seed its own pattern (0-1), flick its flicker (about 1). 0.23.2: calmer and softer
// than 0.23's ("burns too harshly") - slower, softer edges, orange more than white; its brightness the caller's.
float3 torchFlame(float2 q, float t, float seed, float flick, float hw, float h) {
    t *= 0.75;
    float y = q.y / (0.97 + 0.4 * (flick - 1.0));  // a brighter moment burns taller
    float n1 = fbm4(float2(q.x * 1.3 + seed * 7.1, q.y * 1.9 - t * 2.4 + seed * 3.3));
    float n2 = fbm4(float2(q.x * 2.6 - seed * 2.3, q.y * 3.6 - t * 3.9));
    float n3 = valueNoise(float2(q.x * 5.5 + seed, q.y * 7.0 - t * 6.5));
    float sway = sin(t * 1.9 + seed * 5.0) * 0.10 + sin(t * 3.7 + seed) * 0.05;
    float x = q.x + (n1 - 0.47) * 1.5 * pow(saturate(y), 1.4) + sway * y;
    float width = 1.15 * pow(saturate(1.0 - y), 0.5) * smoothstep(-0.2, 0.25, y);
    float body = 1.0 - smoothstep(0.0, 1.0, abs(x) / max(width, 1e-3));
    float torn = body - (n2 - 0.32) * smoothstep(0.1, 0.85, y) - (n3 - 0.5) * 0.25 * y;
    float heat = saturate(torn) * saturate(1.05 - y * 0.6) * smoothstep(-0.06, 0.06, q.y);
    const float3 red = float3(0.6, 0.08, 0.02), orange = float3(1.0, 0.40, 0.06);
    const float3 yellow = float3(1.0, 0.74, 0.28), white = float3(1.0, 0.86, 0.6);
    float3 c = heat < 0.3    ? red * (heat / 0.3)
               : heat < 0.55 ? lerp(red, orange, (heat - 0.3) / 0.25)
               : heat < 0.88 ? lerp(orange, yellow, (heat - 0.55) / 0.33)
                             : lerp(yellow, white, saturate((heat - 0.88) / 0.12));
    float3 o = c * (smoothstep(0.02, 0.45, heat) * (1.0 + 0.5 * (flick - 1.0)));
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
// base and .zw base to tip in target pixels, sourceClamp.x its half width in target pixels, .y its view depth, .w its
// seed (0-1); uiPixels.xy the world's size, .z its flicker, .w the time (seconds); twinX the glow's strength, twinY the
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
    return float4(glowColour * glow * (1.0 - a) + f, a);
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
    float z = viewDepth(sceneDepth.Load(int3(wp, 0)));
    if (z < expected - 6.0) discard;  // a wall, a tree in front of the ground there
    float a = saturate(tex0.Sample(samp0, i.uv).a * i.az.x);
    return float4(a, a, a, a);
}

// 0.33, the clouds' shadows (clouds.cpp): each pixel's place in the world from the picture's depth (t3; r0-r11 the
// camera backwards, f0-farZ the projection), darker where the clouds' noise is high there, the clouds drifting with the
// wind. sharpen the time, footprint the darkness (1 = 100 %). 0.39.3: into a mask a quarter of a big world's size,
// half of a smaller one's (uiPixels.xy the world's size, .zw the mask's) - the light they leave.
float clouds_ps(PresentOut i) : SV_Target {
    int ratio = int(uiPixels.x / uiPixels.z + 0.5);
    int2 wp = min(int2(i.pos.xy) * ratio + ratio / 2, int2(uiPixels.xy) - 1);  // the middle of its pixels
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) return 1.0;  // nothing drawn there
    float3 v = viewAt(wp);
    float3 w = v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    float t = sharpen;
    float2 q = w.xz * 0.0035 + (any(sourceRect.xy != 0.0) ? sourceRect.xy : float2(t * 0.012, t * 0.005));  // 1.22
    float n = fbm4(q) * 0.7 + fbm4(q * 2.3 + float2(5.1, 1.7)) * 0.3;
    return 1.0 - smoothstep(0.46, 0.68, n) * footprint * 0.55;
}

// 0.39.3: the clouds' mask (t0) onto the picture, multiplied (blend ZERO, SRC_COLOR).
float4 clouds_apply_ps(PresentOut i) : SV_Target {
    float ratio = floor(uiPixels.x / uiPixels.z + 0.5);
    float f = tex0.SampleLevel(samp0, i.pos.xy / (ratio * uiPixels.zw), 0).r;
    return float4(f * 0.985, f, min(f * 1.015, 1.0), 1.0);  // a touch cooler under the clouds
}

// 0.34, haze and fog (haze.cpp): the picture's depth (t3) gives each pixel's view depth and place in the world (r0-r11
// the camera backwards, f0-farZ the projection). Far off it goes over to the haze's colour (fogColor) - twinX where
// it starts, twinY where it is whole, twinW how much; over the low ground a fog drifts - sourceRect.rgb its colour,
// .w how much, the ground's height that near the middle of the view (the player's); sharpen the time. Blended over
// the picture by its alpha.
float4 haze_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    bool nothing = sceneDepth.Load(int3(wp, 0)) >= 0.99999;
    float3 v = viewAt(wp);
    float a = (nothing ? 1.0 : smoothstep(twinX, twinY, v.z)) * twinW;
    float3 c = fogColor.rgb;
    if (sourceRect.w > 0.0 && !nothing) {
        int2 last = int2(uiPixels.xy) - 1;
        int2 mid = int2(uiPixels.xy * float2(0.5, 0.58));
        float gy = 0.0;
        [unroll] for (int k = 0; k < 5; ++k) {
            float3 m = viewAt(clamp(mid + int2((k - 2) * int(uiPixels.x) / 16, 0), int2(0, 0), last));
            gy += m.x * r1 + m.y * r4 + m.z * r7 + r10;
        }
        gy *= 0.2;
        float3 w =
            v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) + float3(r9, r10, r11);
        float low = saturate((w.y - gy + 4.0) / 8.0);  // up is -y: the lower, the thicker
        float t = sharpen;
        float wisps = fbm4(w.xz * 0.025 + float2(t * 0.02, -t * 0.013));
        float f = low * (0.45 + 0.55 * smoothstep(0.3, 0.7, wisps)) * sourceRect.w;
        c = lerp(c, sourceRect.rgb, f / max(a + f, 1e-3));
        a = saturate(a + f);
    }
    if (a < 0.003) discard;
    return float4(c, a);
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
// (living::Place::flowers); twinH the dew (dew) glinting on the tips; sourceRect.x the gusts (gusts): a band of
// wind crossing the screen bends the blades and lightens them.
float3 flowerColour(float pick, int palette) {
    float3 a = float3(0.98, 0.86, 0.25), b = float3(0.97, 0.96, 0.92), c = float3(0.72, 0.58, 0.92);  // a meadow
    if (palette == 1) { a = float3(0.92, 0.28, 0.18); b = float3(0.98, 0.62, 0.20); c = float3(0.98, 0.88, 0.40); }
    else if (palette == 2) { a = float3(0.98, 0.72, 0.82); b = float3(0.99, 0.93, 0.95); c = float3(0.90, 0.50, 0.70); }
    else if (palette == 3) { a = float3(0.95, 0.25, 0.55); b = float3(0.99, 0.60, 0.15); c = float3(0.98, 0.92, 0.30); }
    return pick < 0.4 ? a : pick < 0.75 ? b : c;
}

float4 grass_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    int2 last = int2(uiPixels.xy) - 1;
    float here = sceneDepth.Load(int3(wp, 0));
    float hereZ = here >= 0.99999 ? 1e6 : viewDepth(here);
    int longest = int(twinX);
    float t = sharpen;
    int count = int(footprint);
    // 1.7: the way the steps near here push the blades, along the screen (the view's x axis in the world: r0-r2).
    // 1.8.3: the wind's sway and the gusts by the place in the world under the pixel, not by the pixel - by the
    // pixel the waves moved over the grass with the camera as one walked.
    // 1.9.9: how far the steps trod the grass down here, once a pixel (until 1.9.8 for every root it looked at: up
    // to 28 roots times 64 steps a pixel - the costliest effect, 1.3-3.2 ms at 4K)
    float lean = 0.0, swayPhase = 0.0, gust = 0.0, trod = 1.0;
    float windStrength = length(sourceRect.zw);
    float2 windWay = windStrength > 1e-4 ? sourceRect.zw / windStrength : float2(0.943, 0.33);
    // 1.22: the blades lean the wind's way on the screen (the view's x axis in the world: r0-r2)
    float windLean = windStrength * dot(float3(windWay.x, 0.0, windWay.y), float3(r0, r1, r2)) * 0.35;
    if (here < 0.99999) {
        float3 hv = viewAt(wp);
        float3 hw = hv.x * float3(r0, r1, r2) + hv.y * float3(r3, r4, r5) + hv.z * float3(r6, r7, r8) +
                    float3(r9, r10, r11);
        swayPhase = hw.x * 0.35 + hw.z * 0.22;
        if (sourceRect.x > 0.0)  // 1.22: along the shared wind (sourceRect.zw its way times its strength)
            gust = pow(saturate(sin(dot(hw.xz, windWay * 0.042) - t * 0.7)), 8.0) * (0.55 + 0.45 * sin(t * 0.23)) *
                   sourceRect.x;
        float2 push = float2(0.0, 0.0);
        [loop] for (int s = 0; s < count; ++s) {
            float4 st = tex1.Load(int3(2 * s, 0, 0));
            float age = tex1.Load(int3(2 * s + 1, 0, 0)).x;
            float2 d = hw.xz - st.xz;
            float dd = dot(d, d);
            if (dd < 12.0 && dd > 1e-4) push += d * rsqrt(dd) * (1.0 - age) * (1.0 - dd / 12.0);
            trod *= lerp(1.0, saturate(dd / 9.0), 1.0 - age);
        }
        lean = clamp(dot(float3(push.x, 0.0, push.y), float3(r0, r1, r2)), -1.0, 1.0);
    }
    [loop] for (int k = longest; k >= 1; --k) {
        float up = float(k) / twinX;
        float sway = sin(t * 1.7 + swayPhase) * 0.25 * twinX * up * up;
        sway += (gust * 0.55 + lean * 0.5 + windLean) * twinX * up * up;
        int2 root = clamp(wp + int2(int(round(-sway)), k), int2(0, 0), last);
        float rd = sceneDepth.Load(int3(root, 0));
        if (rd >= 0.99999) continue;
        float3 rc = sceneColor.Load(int3(root, 0)).rgb;
        if (rc.g < rc.r * 1.04 || rc.g < rc.b * 1.15 || rc.g < 0.12) continue;  // not green ground
        float3 v = viewAt(root);
        if (hereZ < v.z - 1.0) continue;  // something in front of the blade
        float3 w = v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) +
                   float3(r9, r10, r11);
        // 2.2.1: on the map's own ground only - sourceClamp the cells' grid (its corner, one over a side, the cells
        // across + 1024 * down), t5 each cell's lowest and highest y: umbala's green decks and bridges stand above it
        if (sourceClamp.z > 0.0) {
            int cellsAcross = int(fmod(sourceClamp.w, 1024.0) + 0.5), cellsDown = int(sourceClamp.w / 1024.0);
            int2 cellAt = int2(floor((w.xz - sourceClamp.xy) * sourceClamp.z));
            if (all(cellAt >= int2(0, 0)) && cellAt.x < cellsAcross && cellAt.y < cellsDown) {
                float2 cellSpan = mirrorTex.Load(int3(cellAt, 0)).rg;
                if (cellSpan.x > cellSpan.y || w.y < cellSpan.x - 0.75 || w.y > cellSpan.y + 0.75) continue;
            }
        }
        float2 cell = w.xz * 2.0;  // a blade or none every half unit
        float2 id = floor(cell);
        float2 at = frac(cell) - (0.3 + 0.4 * float2(hash12(id + 11.3), hash12(id + 5.7)));
        if (hash12(id) > twinY) continue;  // no blade here
        // 1.8.3: flowers in patches, a few in each (scattered over the whole field they were a speckle)
        bool flower = hash12(id + 31.7) < twinW && valueNoise(w.xz * 0.07 + 3.3) > 0.72;
        float height = (0.45 + 0.55 * hash12(id + 2.1)) * (flower ? 1.15 : 1.0);
        bool head = flower && up > height * 0.78;
        if (dot(at, at) > (head ? 0.2 : 0.05)) continue;
        height *= trod;  // trodden down
        if (up > height) continue;
        float3 colour = rc * lerp(0.65, 1.25, up / max(height, 1e-3)) * (1.0 + 0.2 * gust * up);
        if (head) colour = flowerColour(hash12(id + 17.9), int(sourceRect.y + 0.5)) * (0.75 + 0.4 * rc.g);
        if (twinH > 0.0 && up > height * 0.85) {  // a dew drop on the tip, now and then catching the sun
            float seed = hash12(id + 43.1);
            float glint = pow(saturate(sin(t * (0.9 + seed * 1.7) + seed * 40.0)), 24.0) * step(0.8, seed);
            colour = lerp(colour, float3(1.0, 1.0, 0.97), glint * twinH);
        }
        return float4(colour, 1.0);
    }
    discard;
    return float4(0.0, 0.0, 0.0, 0.0);
}

// 1.7, the ground of the living world (ao.cpp applyGroundLife), before the characters, blended over by its alpha:
// twinX sand drifting over desert ground in the wind (sand_drift), twinY snow glittering (snow_sparkle), twinW the
// walls of a damp cave glistening where light falls on them and water running down (wet_walls). sharpen the time;
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
    if (twinY > 0.0 && luma > 0.5 && max(col.r, max(col.g, col.b)) - min(col.r, min(col.g, col.b)) < 0.18) {  // snow
        float2 cell = floor(w.xz * 3.0);
        float seed = hash12(cell);
        if (seed > 0.86) {
            float2 f = frac(w.xz * 3.0) - (0.25 + 0.5 * float2(hash12(cell + 3.7), hash12(cell + 8.1)));
            float tw = pow(saturate(sin(t * (0.8 + seed * 2.0) + seed * 60.0 + (v.x + v.y) * 0.02)), 16.0);
            float g = exp(-dot(f, f) * 90.0) * tw * twinY;
            o = lerp(o, float3(1.0, 1.0, 1.0), g);
            a = max(a, g);
        }
    }
    if (twinW > 0.0) {  // a damp cave
        float3 l = viewAt(max(wp - int2(1, 0), int2(0, 0))), r = viewAt(min(wp + int2(1, 0), last));
        float3 u = viewAt(max(wp - int2(0, 1), int2(0, 0))), d = viewAt(min(wp + int2(0, 1), last));
        float3 n = normalize(cross(d - u, r - l));
        float3 nw = n.x * float3(r0, r1, r2) + n.y * float3(r3, r4, r5) + n.z * float3(r6, r7, r8);
        float wall = 1.0 - smoothstep(0.55, 0.85, abs(nw.y));
        float2 s = float2(w.x + w.z, w.y);
        float2 cell = floor(s * 2.2);
        float seed = hash12(cell);
        float lit = smoothstep(0.12, 0.45, luma);
        float g = 0.0;
        if (seed > 0.8) {
            float2 f = frac(s * 2.2) - 0.5;
            g = exp(-dot(f, f) * 30.0) * pow(saturate(sin(t * 0.7 + seed * 50.0)), 6.0) * lit;
        }
        float run = smoothstep(0.93, 1.0, valueNoise(float2(s.x * 2.5, s.y * 0.12 + t * 0.35))) * 0.5 * lit;
        float wet = (g + run) * wall * twinW;
        o = lerp(o, min(col * 1.6 + 0.12, 1.0), saturate(wet));
        a = max(a, saturate(wet));
    }
    if (sourceClamp.y > 0.0 && col.r > col.b * 1.12 && col.g > col.b * 1.05 && col.g < col.r * 1.08 && luma < 0.75) {
        // 1.9.8, ground clutter: pebbles on bare earth, roads and sand - a few in a cell of the world, each a little
        // stone lit from up left with its shadow down right
        float2 cell = floor(w.xz * 1.6);
        float seed = hash12(cell + 71.3);
        if (seed > 0.78) {
            float2 at = (cell + 0.2 + 0.6 * float2(hash12(cell + 2.9), hash12(cell + 8.3))) / 1.6;
            float r = 0.06 + 0.1 * hash12(cell + 4.4);
            float2 d = (w.xz - at) / r;
            float e = length(d * float2(1.0, 1.3));
            float shadow = smoothstep(1.3, 0.7, length((d - float2(0.35, 0.35)) * float2(1.0, 1.3)));
            float stone = smoothstep(1.0, 0.8, e);
            float3 rock = lerp(col, float3(0.62, 0.6, 0.56), 0.55) * (1.05 - 0.35 * saturate(dot(d, float2(0.6, 0.6))));
            float3 pc = lerp(col * 0.55, rock, stone);
            float pa = max(stone, shadow * 0.45) * sourceClamp.y;
            o = lerp(o, pc, pa / max(a + pa, 1e-3));
            a = saturate(max(a, pa));
        }
    }
    if (twinH > 0.0) {  // 1.7: wet sand along the shore (shore_foam): just above the water (sourceClamp.x its level,
        // last frame's) near ground that lies under it, darker in a band that comes and goes with the swash
        float above = sourceClamp.x - w.y;  // up is -y
        float band = 0.9 + 0.7 * sin(t * 0.55 + dot(w.xz, float2(0.04, 0.03)));
        if (above > 0.0 && above < band && col.r > col.b * 1.05) {
            float near = 0.0;
            float reach = 30.0 * scale;
            [unroll] for (int k = 0; k < 8; ++k) {
                float ang = float(k) * 0.785398;
                int2 q = clamp(wp + int2(float2(cos(ang), sin(ang)) * reach), int2(0, 0), last);
                if (sceneDepth.Load(int3(q, 0)) >= 0.99999) continue;
                float3 qv = viewAt(q);
                float qy = qv.x * r1 + qv.y * r4 + qv.z * r7 + r10;
                near = max(near, step(sourceClamp.x + 0.1, qy));
            }
            float wet = smoothstep(band, band * 0.4, above) * 0.5 * near;
            o = lerp(o, col * 0.6, wet / max(a + wet, 1e-3));
            a = saturate(max(a, wet));
        }
    }
#ifndef UAGFX_SAFE
    // 1.20, the year and the rain on the whole map (core/seasons.hpp): sourceRect.x winter's snow on what faces up,
    // .y autumn's fallen leaves, .z the green ground drier (autumn, winter), .w greener (spring); sourceClamp.z the
    // rain's wet on the walls, the roofs and the models (rain_wet) - darker, a sheen where one looks along them.
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
        if (sourceRect.y > 0.0) {  // fallen leaves on the ground
            float2 cell = floor(w.xz * 2.4);
            float seed = hash12(cell + 23.7);
            if (seed < sourceRect.y * 0.6 * up) {
                float2 at = (cell + 0.2 + 0.6 * float2(hash12(cell + 1.7), hash12(cell + 6.1))) / 2.4;
                float ang = hash12(cell + 9.9) * 6.283;
                float2 dd = w.xz - at;
                dd = float2(dd.x * cos(ang) - dd.y * sin(ang), dd.x * sin(ang) + dd.y * cos(ang)) / 0.09;
                float leaf = smoothstep(1.0, 0.75, length(dd * float2(1.0, 1.9)));
                float hue = hash12(cell + 3.3);
                float3 lc = hue < 0.4 ? float3(0.85, 0.55, 0.15) : hue < 0.75 ? float3(0.72, 0.28, 0.1)
                                                                           : float3(0.55, 0.42, 0.18);
                lc *= clamp(luma * 2.2, 0.35, 1.1);
                o = lerp(o, lc, leaf / max(a + leaf, 1e-3));
                a = saturate(max(a, leaf));
            }
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
// the light from above under the sea, over the whole picture. sharpen the time, twinW the creatures' light.
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
    float2 px = kind > 12.5 ? (corner * 0.5 + 0.5) * targetSize : s.xy + corner * s.z * 3.5;
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
    } else if (kind == 5) {  // a leaf turning: q.y its angle, q.z its colour
        float2 l = turned(d, q.y);
        float e = length(l / float2(1.0, 0.45 + 0.35 * abs(sin(q.y * 0.7))));
        int ci = int(q.z + 0.5);
        c = ci == 0 ? float3(0.42, 0.62, 0.2) : ci == 1 ? float3(0.86, 0.66, 0.16)
          : ci == 2 ? float3(0.82, 0.38, 0.12) : float3(0.97, 0.76, 0.84);
        c *= 0.85 + 0.25 * step(0.0, l.y);
        a = smoothstep(1.0, 0.8, e);
    } else if (kind == 6) {  // an ember
        glow = float3(1.0, 0.62, 0.22) * exp(-dot(d, d) * 3.0) * 1.4;
    } else if (kind == 7) {  // a puff of breath
        c = float3(0.95, 0.97, 1.0);
        a = exp(-dot(d, d) * 1.6) * (0.75 + 0.25 * valueNoise(d * 2.0 + t));
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
    float3 n = normalize(float3(-slope * 5.0, 1.0));
    int have = max(int(uiPixels.w + 0.5) - 1, 0), at = min(base, have);
    int2 size = max(int2(sourceRect.xy) >> at, int2(1, 1));
    int2 q0 = p << (base - at);
    float3 rgb = tex0.Load(int3(((q0 % size) + size) % size, at)).rgb;
    float sat = max(rgb.r, max(rgb.g, rgb.b)) - min(rgb.r, min(rgb.g, rgb.b));
    float lum = dot(rgb, float3(0.299, 0.587, 0.114));
    float gloss = saturate((0.22 - sat) * 4.0) * smoothstep(0.2, 0.6, lum) * 0.7;
    // 1.21: a window's pane - an even patch clearly darker than the wall about it: alpha 0.75-1 (gloss stays under
    // 0.7), lit at night by shade()
    float local = matLuma(p >> 2, base + 2), wide = matLuma(p >> 4, base + 4);
    float pane = smoothstep(0.1, 0.2, wide - local) * smoothstep(0.35, 0.22, local) *
                 smoothstep(0.08, 0.03, abs(lum - local));
    if (pane > 0.3) gloss = 0.75 + 0.25 * pane;
    return float4(n.xy * 0.5 + 0.5, saturate(0.5 + h / weights * 1.6), gloss);
}

#ifndef UAGFX_SAFE
// 1.11, light from the sky and from around (ao.cpp applySkyLight), before the characters, the picture's copy t2 put
// back changed: twinX the sky's share - what faces up takes the sky's colour (sourceRect.rgb), what faces down the
// ground's (sourceClamp.rgb), as the light from all around would; twinY the bounce - the colours a few steps around
// that face the surface light it a little in their colour (grass greens a wall's foot, a red roof warms the eaves);
// twinW how far (world units). r0-r11 the view -> the world.
float4 sky_light_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) discard;
    int2 last = int2(uiPixels.xy) - 1;
    float3 col = sceneColor.Load(int3(wp, 0)).rgb;
    float3 p = viewAt(wp);
    float3 l = viewAt(max(wp - int2(1, 0), int2(0, 0))), r = viewAt(min(wp + int2(1, 0), last));
    float3 u = viewAt(max(wp - int2(0, 1), int2(0, 0))), d = viewAt(min(wp + int2(0, 1), last));
    float3 dx = abs(r.z - p.z) < abs(p.z - l.z) ? r - p : p - l;
    float3 dy = abs(d.z - p.z) < abs(p.z - u.z) ? d - p : p - u;
    float3 n = normalize(cross(dy, dx));
    if (dot(n, p) > 0.0) n = -n;
    float3 nw = n.x * float3(r0, r1, r2) + n.y * float3(r3, r4, r5) + n.z * float3(r6, r7, r8);
    float3 around = lerp(sourceClamp.rgb, sourceRect.rgb, saturate(0.5 - nw.y * 0.5));  // up is -y
    float3 o = col * lerp(float3(1.0, 1.0, 1.0), around, twinX);
    if (twinY > 0.0) {
        float reach = clamp(twinW * f0 * scale / max(p.z, 1.0), 4.0, 64.0);
        float spin = hash12(float2(wp)) * 6.2831853;
        float3 sum = float3(0.0, 0.0, 0.0);
        float weights = 0.0;
        [unroll] for (int k = 0; k < 8; ++k) {
            float ang = spin + float(k) * 0.7853982;
            float far = reach * (0.45 + 0.55 * frac(float(k) * 0.618 + spin));
            int2 q = clamp(wp + int2(float2(cos(ang), sin(ang)) * far), int2(0, 0), last);
            if (sceneDepth.Load(int3(q, 0)) >= 0.99999) continue;
            float3 to = viewAt(q) - p;
            float dist = length(to);
            if (dist < 1e-3) continue;
            float w = saturate(dot(n, to / dist)) / (1.0 + dist * dist * 0.01);
            sum += sceneColor.Load(int3(q, 0)).rgb * w;
            weights += w;
        }
        float3 bounce = sum / max(weights, 1e-3);
        o += col * bounce * twinY * saturate(weights * 2.0) * 0.35;
    }
    return float4(o, 1.0);
}
#endif

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

#ifndef UAGFX_SAFE
// 1.13, volumetric fog (sun_map.cpp applyFog): along the view from the camera to what the pixel shows, a dozen steps
// through a fog lying low over the ground - thicker below twinX + twinY (the ground's height, up -y, and the fog's
// depth), drifting with noise - each step lit by the sun (2.2: no sun's map, no rays). sourceClamp, sourceRect,
// fogColor the view into the world's x, y and z; twinW the density, uiPixels.z the sun's share; sharpen the time.
// Blended over by its alpha.
float4 fog_light_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    float raw = sceneDepth.Load(int3(wp, 0));
    float3 v = raw >= 0.99999 ? normalize(viewAt(wp)) * 400.0 : viewAt(wp);
    float len = min(length(v), 400.0);
    float3 dir = normalize(v);
    const int kSteps = 12;
    float stride = len / float(kSteps);
    float t = hash12(float2(wp) + frac(sharpen)) * stride;
    float trans = 1.0;
    float3 light = float3(0.0, 0.0, 0.0);
    [loop] for (int k = 0; k < kSteps; ++k, t += stride) {
        float3 q = dir * t;
        float4 q4 = float4(q, 1.0);
        float wy = dot(q4, sourceRect), wx = dot(q4, sourceClamp), wz = dot(q4, fogColor);
        float above = twinX - wy;  // up is -y
        float dens = exp(-max(above, 0.0) / twinY) * twinW;
        dens *= 0.55 + 0.9 * valueNoise(float2(wx, wz) * 0.035 + float2(sharpen * 0.03, sharpen * 0.017));
        if (dens < 1e-4) continue;
        float sun = 1.0;
        float a = 1.0 - exp(-dens * stride);
        float3 lit = float3(0.62, 0.68, 0.76) * 0.55 + float3(1.0, 0.92, 0.78) * sun * uiPixels.z;
        light += trans * a * lit;
        trans *= 1.0 - a;
    }
    float alpha = 1.0 - trans;
    if (alpha < 0.003) discard;
    return float4(light / alpha, alpha);
}
#endif

// 0.80, footprints (ao.cpp applySteps): on the ground (the picture's depth, the camera backwards) about each step of
// t1 - two texels: its place (xyz) and way (w), then its age 0-1 (x) - footprint how many; twinX the kind: 1 snow,
// 2 sand (a foot's print, fading), 3 dust (a puff spreading and thinning). Blended over by its alpha.
float4 steps_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    if (sceneDepth.Load(int3(wp, 0)) >= 0.99999) discard;
    float3 v = viewAt(wp);
    float3 w = v.x * float3(r0, r1, r2) + v.y * float3(r3, r4, r5) + v.z * float3(r6, r7, r8) + float3(r9, r10, r11);
    int kind = int(twinX + 0.5);
    float a = 0.0;
    int count = int(footprint);
    [loop] for (int k = 0; k < count; ++k) {
        float4 s = tex1.Load(int3(2 * k, 0, 0));
        if (abs(w.y - s.y) > 3.0) continue;
        float2 d = w.xz - s.xz;
        if (dot(d, d) > 40.0) continue;
        float age = tex1.Load(int3(2 * k + 1, 0, 0)).x;
        if (kind == 3) {
            float r = 1.0 + 4.0 * age;
            float puff = exp(-dot(d, d) / (r * r)) * (1.0 - age) * (1.0 - age);
            a = max(a, puff * (0.7 + 0.3 * valueNoise(w.xz * 0.8 + s.xz)));
        } else {
            float ca = cos(s.w), sa = sin(s.w);
            float2 l = float2(d.x * ca + d.y * sa, -d.x * sa + d.y * ca);
            float e = length(l / float2(1.1, 0.5));
            a = max(a, smoothstep(1.0, 0.65, e) * (1.0 - age * age));
        }
    }
    if (a < 0.01) discard;
    float3 colour = kind == 1   ? float3(0.42, 0.48, 0.62)
                    : kind == 2 ? float3(0.50, 0.38, 0.24)
                                : float3(0.82, 0.74, 0.60);
    float most = kind == 1 ? 0.5 : kind == 2 ? 0.4 : 0.35;
    return float4(colour, a * most);
}

// 0.73, light in the air (haze.cpp), added onto the picture: about each light of the torches' pass (t1, footprint of
// them, as torch_light_ps reads them) the air glows - the light scattered along the view up to what the pixel shows
// (its depth), so a wall hides the glow behind it - twinX its strength; and under the open sky (twinY its strength)
// rays from the sky where the map's edge shows it, streaming away from the sun (sourceRect.xy, target pixels).
float4 shafts_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
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
        sum += c.rgb * (c.w * along * fall * fall * twinX);
    }
    if (twinY > 0.0) {
        float2 toSun = sourceRect.xy - float2(wp);
        float2 stride = toSun / max(length(toSun), 1.0) * min(length(toSun), 0.45 * length(uiPixels.xy)) / 24.0;
        float2 q = float2(wp) + stride * hash12(float2(wp));  // dithered: no bands
        float rays = 0.0, decay = 1.0;
        int2 last = int2(uiPixels.xy) - 1;
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
// premultiplied - none where nothing reflects - then laid over the world by half_over_ps.
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
    float flat = smoothstep(0.88, 0.97, -nw.y);  // up is -y
    float shine = flat * (1.0 - rim) * smoothstep(0.15, 0.3, dot(-v, n));  // what reflects
    // the rain's wet darkens a rim as the floor about it (taken for floor): no light specks along the edges
    float wet = twinY > 0.0 ? lerp(flat, 1.0, rim) : 0.0;
    if (shine <= 0.0 && wet <= 0.0) return float4(0.0, 0.0, 0.0, 0.0);
    float weight = 0.0;
    float3 c = float3(0.0, 0.0, 0.0);
    if (shine > 0.0) c = reflectionIn(p + n * 0.6, reflect(v, n), uiPixels.xy, weight);
    float fresnel = 0.04 + 0.96 * pow(1.0 - saturate(dot(-v, n)), 5.0);
    float a = shine * weight * twinX * (0.35 + 0.65 * saturate(fresnel * 4.0));
    // 0.97, wet ground (twinY): the ground a little darker all over, and puddles - patches of noise over the world,
    // the same wherever the camera goes - mirroring nearly as water does.
    float dark = 0.0;
    if (twinY > 0.0) {
        float3 world = p.x * float3(r0, r1, r2) + p.y * float3(r3, r4, r5) + p.z * float3(r6, r7, r8) +
                       float3(r9, r10, r11);
        float n0 = valueNoise(world.xz * 0.045) * 0.7 + valueNoise(world.xz * 0.17 + 3.1) * 0.3;
        float puddle = smoothstep(0.58, 0.66, n0) * shine;
        dark = 0.16 * wet * twinY;
        a = max(a, puddle * weight * (0.45 + 0.4 * saturate(fresnel * 3.0)) * twinY);
        // 1.7: rings where the rain's drops fall into the puddles (sharpen the time)
        float2 rc = world.xz * 0.6;
        float2 cid = floor(rc);
        float2 cf = frac(rc) - (0.3 + 0.4 * float2(hash12(cid + 1.3), hash12(cid + 4.1)));
        float age = frac(sharpen * 0.9 + hash12(cid));
        float ring = exp(-pow((length(cf) - age * 0.45) * 30.0, 2.0)) * (1.0 - age) * puddle * twinY;
        c = lerp(c, float3(0.92, 0.95, 1.0), saturate(ring * 0.7));
        a = max(a, ring * 0.45);
    }
    // Premultiplied over: the ground darkened by `dark`, then the reflection over it by `a`.
    return float4(c * a, 1.0 - (1.0 - dark) * (1.0 - a));
}

// 0.98, the sun's flare (haze.cpp): sourceRect.xy the sun past the screen, uiPixels.xy the world's size, twinX the
// strength. How much sky shows on the way to the sun (the depth's far plane) scales a glow at that edge and rings
// along the line from the sun through the middle - added on.
float4 flare_ps(PresentOut i) : SV_Target {
    float2 size = uiPixels.xy;
    float2 sun = sourceRect.xy;
    float2 mid = size * 0.5;
    float sky = 0.0;
    [unroll] for (int k = 0; k < 12; ++k) {
        float2 q = clamp(lerp(mid, sun, 0.5 + 0.045 * float(k)), float2(1.0, 1.0), size - 2.0);
        sky += sceneDepth.Load(int3(int2(q), 0)) >= 0.99999 ? 1.0 : 0.0;
    }
    sky /= 12.0;
    if (sky <= 0.0) discard;
    float2 p = i.pos.xy;
    float span = length(size);
    float2 axis = mid - sun;
    float3 sum = float3(0.0, 0.0, 0.0);
    [unroll] for (int g = 0; g < 5; ++g) {
        float at = g == 0 ? 0.62 : g == 1 ? 0.86 : g == 2 ? 1.18 : g == 3 ? 1.46 : 1.85;
        float radius = (g == 0 ? 0.03 : g == 1 ? 0.055 : g == 2 ? 0.02 : g == 3 ? 0.08 : 0.045) * span;
        float3 tint = g == 0 ? float3(1.0, 0.85, 0.6) : g == 1 ? float3(0.6, 0.8, 1.0) : g == 2 ? float3(1.0, 0.7, 0.5)
                                                    : g == 3 ? float3(0.55, 1.0, 0.7) : float3(0.8, 0.7, 1.0);
        float d = length(p - (sun + axis * at)) / radius;
        sum += tint * (smoothstep(1.0, 0.75, d) * 0.08 + exp(-d * d * 2.5) * 0.05);
    }
    float2 edge = clamp(sun, float2(0.0, 0.0), size);
    float e = length(p - edge) / (0.45 * span);
    sum += float3(1.0, 0.92, 0.75) * exp(-e * e * 2.0) * 0.35;
    return float4(sum * (twinX * sky), 0.0);
}

// 0.86: a half-size premultiplied picture (t0) laid over the world, smoothly (uiPixels.xy the world's size).
float4 half_over_ps(PresentOut i) : SV_Target {
    float4 c = tex0.SampleLevel(samp0, i.pos.xy / uiPixels.xy, 0);
    if (c.a < 0.003) discard;
    return c;
}

// 0.69, the sky (haze.cpp): on a map under the open sky, where nothing is drawn - past the map's edge - a sky
// instead of the void: from the horizon's colour (fogColor) to the zenith's (sourceRect.rgb) by how far up the pixel
// looks in the world (reproject: the camera's axes, up is -y), clouds drifting on a far layer (twinX how many; under
// the map's edge looked down at, a sea of them), at night (twinW 0-1) darker and with stars. twinY its strength
// (alpha), sharpen the time.
float4 sky_ps(PresentOut i) : SV_Target {
    int2 wp = int2(i.pos.xy);
    if (sceneDepth.Load(int3(wp, 0)) < 0.99999) discard;
    float2 g = (float2(wp) - float2(jitterX, jitterY)) / scale;
    float3 dv = normalize(float3((g.x - cx) / f0, (g.y - cy) / f1, 1.0));
    float3 d = normalize(dv.x * float3(r0, r1, r2) + dv.y * float3(r3, r4, r5) + dv.z * float3(r6, r7, r8));
    float up = -d.y;
    float night = twinW;
    float t = sharpen;
    float3 c = lerp(fogColor.rgb, sourceRect.rgb, smoothstep(-0.35, 0.6, up));
    float2 layer = d.xz / (abs(d.y) + 0.3);
    float2 uv = layer * 1.4 + (any(sourceClamp.yz != 0.0) ? sourceClamp.yz : float2(t * 0.010, t * 0.006));  // 1.22
    float n = fbm4(uv);
    float cover = smoothstep(0.62 - 0.3 * twinX, 0.92 - 0.2 * twinX, n) * saturate(twinX * 1.5);
    float lit = saturate(0.55 + (n - fbm4(uv + float2(0.06, 0.04))) * 6.0);  // the side towards the light brighter
    float3 day = lerp(float3(0.78, 0.80, 0.86), float3(1.0, 0.99, 0.96), lit);
    float3 cloud = lerp(day, float3(0.16, 0.18, 0.26) + 0.08 * lit, night);
    c = lerp(c, cloud, cover * smoothstep(-0.95, -0.2, -abs(up)));  // thinner straight up and straight down
    if (night > 0.0) {
        float2 cell = floor(layer * 70.0);
        float h = hash12(cell);
        float2 at = frac(layer * 70.0) - (0.25 + 0.5 * float2(hash12(cell + 7.1), hash12(cell + 3.7)));
        float star = step(0.985, h) * saturate(1.0 - length(at) * 6.0);
        star *= 0.6 + 0.4 * sin(t * (1.5 + 3.0 * hash12(cell + 1.3)) + h * 40.0);
        c += star * night * (1.0 - cover) * float3(0.9, 0.95, 1.0);
    }
#ifndef UAGFX_SAFE
    // 1.18, land past the map's edge (edge_land, twinH): looked down at, the ground goes on - the plane of the
    // ground's height at the player (sourceClamp.x) in the colours of the ground drawn on the screen (5 x 5 of it,
    // t2), in fields and darker woods, fading into the horizon's haze far off - instead of a sea of clouds.
    if (twinH > 0.0 && d.y > 0.02) {
        float3 eye = float3(r9, r10, r11);
        float dist = (sourceClamp.x - eye.y) / d.y;
        float3 ground = float3(0.0, 0.0, 0.0);
        float found = 0.0;
        [unroll] for (int gy = 0; gy < 5; ++gy)
            [unroll] for (int gx = 0; gx < 5; ++gx) {
                int2 q = int2((float2(gx, gy) + 0.5) / 5.0 * uiPixels.xy);
                if (sceneDepth.Load(int3(q, 0)) < 0.99999) {
                    ground += sceneColor.Load(int3(q, 0)).rgb;
                    found += 1.0;
                }
            }
        if (dist > 0.0 && found > 0.0) {
            float3 p = eye + d * dist;
            ground /= found;
            float grain = fbm4(p.xz * 0.012);
            float wood = smoothstep(0.5, 0.7, fbm4(p.xz * 0.004 + 7.3));
            float3 land = ground * (0.75 + 0.5 * grain) * lerp(float3(1.0, 1.0, 1.0), float3(0.55, 0.68, 0.5), wood);
            land = lerp(land, fogColor.rgb, smoothstep(150.0, 1200.0, dist));
            c = lerp(c, land, twinH * smoothstep(0.02, 0.12, d.y));
        }
    }
#endif
    return float4(c, twinY);
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
float4 weather_ps(PresentOut i) : SV_Target {
    float2 p = i.pos.xy / scale;  // game pixels
    float t = sharpen;
    int kind = int(twinX + 0.5);
    float3 sum = float3(0.0, 0.0, 0.0);
    float a = 0.0;
    // 1.23, dust in the light (twinY): indoors the motes show where light falls on what is behind them (the
    // picture's copy, t0) - in the beams, by the torches - and hardly in the dark; tinted by that light
    float lit = 1.0;
    float3 litColour = float3(0.85, 0.80, 0.70);
    if (twinY > 0.0 && kind == 6) {
        float3 under = tex0.Load(int3(int2(i.pos.xy), 0)).rgb;
        float l = dot(under, float3(0.299, 0.587, 0.114));
        lit = lerp(1.0, smoothstep(0.16, 0.6, l) * 2.4, twinY);
        litColour = lerp(litColour, saturate(under * 1.3 + 0.25), 0.6 * twinY);
    }
    [unroll] for (int layer = 0; layer < 3; ++layer) {
        float depth = 0.6 + 0.5 * float(layer);
        float cell = (kind == 1 ? 36.0 : kind == 2 ? 22.0 : kind == 3 ? 110.0 : kind == 4 ? 60.0 : 80.0) * depth;
        float2 fall = kind == 1 ? float2(8.0 * sin(t * 0.21), 34.0)
                      : kind == 2 ? float2(-55.0, 520.0)
                      : kind == 3 ? float2(18.0, 26.0)
                      : kind == 4 ? float2(-300.0, 14.0)
                                  : float2(5.0, kind == 5 ? -3.0 : 2.0);
        // 1.22: the shared wind blows it along the screen (sourceRect.z how far so far, .w how fast now)
        float blown = kind == 1 ? 1.0 : kind == 2 ? 0.6 : kind == 3 ? 1.6 : kind == 4 ? 2.5 : 0.4;
        float2 q = p + sourceRect.xy * depth - fall * depth * t - float2(sourceRect.z * blown, 0.0) * depth;
        fall.x += sourceRect.w * blown;
        float2 id = floor(q / cell);
        float2 f = frac(q / cell);
        float presence = kind == 3 ? 0.18 : kind == 4 ? 0.35 : kind >= 5 ? 0.3 : 0.55;
        if (hash12(id + float2(91.0, 17.0) * float(layer + 1)) > presence * footprint) continue;
        float2 h = float2(hash12(id + float(layer) * 17.0), hash12(id * 1.7 + float(layer) * 3.1));
        float2 d = (f - (0.2 + 0.6 * h)) * cell;  // from the particle, game pixels
        float shape = 0.0;
        float3 colour = float3(1.0, 1.0, 1.0);
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
            float turn = abs(sin(t * 2.1 + h.y * 6.28));
            float2 e = d / float2(max(3.2 * depth * turn, 0.6), 2.0 * depth);
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
        float s = saturate(shape) * alpha;
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

#ifndef UAGFX_SAFE
// 2.0, the new trees from the map (forest.cpp; core/forest.hpp): each tree the driver's own - its vertices in the
// model's room (up along -y, the foot at 0), an instance a tree: its place in the world and turn (about up), its
// scale and number, the colours of its leaves and bark. r0-r11 the world into the view, f0-farZ the game's projection,
// targetSize, scale and jitter as the game's draws; sourceRect.xyz the way to the sun in the world, .w the time;
// sourceClamp.xy the wind's way times its strength, .z the sun's light here (night, indoors less).
struct ForestIn {
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 info : TEXCOORD1;  // open to the sky, the wind's share, the part (0 bark, 1 leaves, 2 frond, 3 twig), own
    float4 at : TEXCOORD2;    // the place, the turn (radians)
    float4 size : TEXCOORD3;  // the scale x y z, the tree's number (0-1)
    float4 leaf : TEXCOORD4;
    float4 bark : TEXCOORD5;
};

struct ForestOut {
    float4 pos : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 info : TEXCOORD1;  // open, part, own, the tree's number
    float3 leaf : TEXCOORD2;
    float3 bark : TEXCOORD3;
    float3 world : TEXCOORD4;  // 2.0.2: for the real sun's map
};

// A forest vertex in the world: the instance's turn, scale and place, then the wind.
float3 forestWorld(ForestIn i, out float3 n) {
    float s = sin(i.at.w), c = cos(i.at.w);
    float3 p = i.pos * i.size.xyz;
    float3 w = float3(p.x * c - p.z * s, p.y, p.x * s + p.z * c) + i.at.xyz;
    n = float3(i.normal.x * c - i.normal.z * s, i.normal.y, i.normal.x * s + i.normal.z * c);
    // the wind: the crown more than the trunk, each tree in its own time, a gust now and then; the leaves flutter
    float t = sourceRect.w;
    float strength = length(sourceClamp.xy);
    float2 way = strength > 1e-4 ? sourceClamp.xy / strength : float2(0.94, 0.33);
    float phase = dot(i.at.xz, float2(0.05, 0.07)) + i.size.w * 6.28;
    float bend = (sin(t * 0.9 + phase) * 0.5 + sin(t * 2.3 + phase * 1.7) * 0.2 + 0.45) * (0.4 + strength);
    w.xz += way * bend * i.info.y * 1.4 * i.size.y;
    if (i.info.z > 0.5 && i.info.z < 4.5) w += normalize(n) * sin(t * 5.5 + i.info.w * 40.0) * 0.06;  // not a puff
    return w;
}

ForestOut forest_vs(ForestIn i) {
    ForestOut o;
    float3 n;
    float3 w = forestWorld(i, n);
    float3 v = float3(dot(float4(w, 1.0), float4(r0, r1, r2, r3)), dot(float4(w, 1.0), float4(r4, r5, r6, r7)),
                      dot(float4(w, 1.0), float4(r8, r9, r10, r11)));
    float z = max(v.z, nearZ + 0.01);
    float gx = v.x / z * f0 + cx, gy = v.y / z * f1 + cy;
    float depth = farZ * (1.0 - nearZ / z) / (farZ - nearZ);
    float x = (gx * scale + 0.5 + jitterX) * 2.0 / targetSize.x - 1.0;
    float y = 1.0 - (gy * scale + 0.5 + jitterY) * 2.0 / targetSize.y;
    o.pos = float4(x * z, y * z, depth * z, z);
    o.normal = n;
    o.uv = i.uv;
    o.info = float4(i.info.x, i.info.z, i.info.w, i.size.w);
    o.leaf = i.leaf.rgb;
    o.bark = i.bark.rgb;
    o.world = w;
    return o;
}

// 2.0.3: an anime crown's puff - a round cloud with a scalloped rim; how much of the place is in it, how near its rim
float forestPuff(float2 uv, float own, out float edge) {
    float2 d = uv - 0.5;
    float r = length(d) * 2.0, ang = atan2(d.y, d.x);
    float rim = 0.86 + 0.05 * sin(ang * 9.0 + own * 20.0) + 0.03 * sin(ang * 17.0 + own * 7.0);
    edge = smoothstep(rim - 0.2, rim - 0.04, r);
    return smoothstep(rim, rim - 0.05, r);
}

// 2.0.2: a card of blossom - seven small flowers of five petals; how much of the place is petal, its flower's heart
float forestBlossom(float2 uv, float own, out float heart) {
    float best = 0.0;
    heart = 0.0;
    [unroll] for (int k = 0; k < 7; ++k) {
        float a = float(k) * 0.9 + own * 6.28;
        float2 d = uv - (0.5 + float2(cos(a), sin(a)) * (k == 0 ? 0.0 : 0.29));
        float rad = length(d), ang = atan2(d.y, d.x);
        float petal = 0.12 + 0.04 * cos(ang * 5.0 + own * 9.0 + float(k));
        float f = saturate(1.0 - rad / petal);
        if (f > best) {
            best = f;
            heart = 1.0 - smoothstep(0.0, 0.035, rad);
        }
    }
    return best;
}

// A card's leaves: one in the middle and eight about it, each an ellipse turned its way; how much of the place is leaf
// and how far into the leaf (for its vein and its edge's light).
// 2.0.3: small leaves - one in each cell of a 4 x 4 grid over the card, shifted and turned its own way (the nine cells
// about the place looked at), the card's corners left out: a round tuft, not 1.10's nine big leaves of a square card.
float forestLeaves(float2 uv, float own, out float vein) {
    float2 g = uv * 4.0;
    float2 cell = floor(g);
    float best = 0.0;
    vein = 0.0;
    [unroll] for (int k = 0; k < 9; ++k) {
        float2 c = cell + float2(float(k % 3) - 1.0, float(k / 3) - 1.0);
        float h1 = hash12(c + own * 13.7), h2 = hash12(c * 1.7 + own * 5.3), h3 = hash12(c * 2.3 + own * 9.1);
        float2 d = g - (c + 0.5 + (float2(h1, h2) - 0.5) * 0.6);
        float ang = h3 * 6.2831853;
        float2 l = float2(d.x * cos(ang) + d.y * sin(ang), -d.x * sin(ang) + d.y * cos(ang));
        float e = (l.x * l.x) / (0.62 * 0.62) + (l.y * l.y) / (0.26 * 0.26);
        float leaf = saturate(1.0 - e);
        if (leaf > best) {
            best = leaf;
            vein = 1.0 - smoothstep(0.0, 0.05, abs(l.y));
        }
    }
    return best * smoothstep(0.52, 0.36, length(uv - 0.5));
}

float4 forest_ps(ForestOut i) : SV_Target {
    float part = i.info.y, own = i.info.z, open = i.info.x;
    float3 L = normalize(sourceRect.xyz);
    float3 n = normalize(i.normal);
    float sun = sourceClamp.z;
    float3 base;
    if (part > 4.5) {  // 2.0.3, an anime puff: cel shading - shade, light and a highlight in steps over the clump's
        // ball, warm in the light and cool in the shade, a bright rim where the ball turns from the camera, a soft ink
        // at the puff's edge
        float edge;
        float mask = forestPuff(i.uv, own, edge);
        clip(mask - 0.5);
        float t = (dot(n, L) * 0.5 + 0.5) * (0.55 + 0.45 * open);
        t = lerp(0.42, t, sun);  // at night and indoors flatter
        float band = smoothstep(0.36, 0.42, t), high = smoothstep(0.62, 0.66, t);
        float3 shade = i.leaf * float3(0.55, 0.68, 1.05) * 0.72;
        float3 lightC = min(i.leaf * 1.25 + float3(0.1, 0.08, -0.02), 1.0);
        float3 c5 = lerp(lerp(shade, i.leaf, band), lightC, high);
        float3 V = -normalize(float3(r8, r9, r10));  // towards the camera
        float rim = pow(1.0 - saturate(abs(dot(n, V))), 3.0) * 0.45 * smoothstep(0.3, 0.6, t);
        c5 += rim * float3(0.9, 0.9, 0.6) * sun;
        c5 *= 1.0 - 0.08 * edge;
        float grey = dot(c5, float3(0.299, 0.587, 0.114));
        return float4(lerp(float3(grey, grey, grey), c5, 1.12), 1.0);
    }
    if (part < 0.5) {  // bark: furrows along the trunk
        float furrow = valueNoise(float2(i.uv.x * 7.0, i.uv.y * 1.5)) * 0.6 +
                       valueNoise(float2(i.uv.x * 19.0, i.uv.y * 4.0)) * 0.4;
        base = i.bark * (0.62 + 0.55 * furrow);
    } else if (part > 1.5 && part < 2.5) {  // a palm's frond: a stem and leaflets along it
        float across = abs(i.uv.x - 0.5) * 2.0;
        float leaflet = 0.5 + 0.5 * sin(i.uv.y * 60.0 + own * 20.0);
        float width = 0.1 + 0.9 * (1.0 - i.uv.y * 0.6) * leaflet;
        clip(width - across);
        base = i.leaf * (across < 0.08 ? 0.75 : 0.9 + 0.2 * leaflet);
    } else if (part > 3.5) {  // 2.0.2: a card of blossom - pale at the petals' tips, a deeper heart
        float heart;
        float petal = forestBlossom(i.uv, own, heart);
        clip(petal - 0.1);
        base = lerp(i.leaf * (0.9 + 0.2 * frac(own * 5.7)), i.leaf * float3(0.75, 0.5, 0.55), heart * 0.6);
        base = lerp(base, float3(1.0, 0.97, 0.97), (1.0 - petal) * 0.35);
    } else if (footprint > 0.5) {  // 2.0.4: the game's own leaves' picture, a piece of it on the card, round
        float2 at = 0.12 + float2(hash12(float2(own * 31.7, 1.3)), hash12(float2(own * 17.1, 7.7))) * 0.46;
        float4 t = tex0.Sample(samp0, at + i.uv * 0.3);
        float2 off = i.uv - 0.5;  // a ragged round edge, lost among the picture's own leaves
        float edge = 0.44 + 0.07 * sin(atan2(off.y, off.x) * 7.0 + own * 13.0);
        float round = smoothstep(edge + 0.06, edge - 0.04, length(off));
        clip(t.a * round - 0.5);
        float3 average = max(float3(asfloat(stage0.x), asfloat(stage0.y), asfloat(stage1.x)), 0.02);
        base = t.rgb * (i.leaf / average) * (part > 2.5 ? 0.85 : 1.0);  // its tree's shade and season
    } else {  // a card of leaves (a fir's twig narrower, darker)
        float vein;
        float leaf = forestLeaves(i.uv, own, vein);
        clip(leaf - 0.08);
        float tone = 0.82 + 0.36 * frac(own * 7.31 + floor(leaf * 3.0) * 0.17);
        base = i.leaf * tone * (part > 2.5 ? 0.8 : 1.0) * (1.0 - 0.15 * vein) * (0.85 + 0.25 * leaf);
    }
    // the light: the sun's on what faces it (wrapped: the leaves let some through), the sky's as open as the place is
    float face = saturate((dot(n, L) + 0.35) / 1.35);
    float sky = 0.4 + 0.6 * open;
    // 2.0.2: the leaves let a little of the sun through: their far side glows warm green
    float through = part > 0.5 ? saturate(-dot(n, L)) * 0.25 * sun : 0.0;
    float3 c = base * (sky * lerp(0.75, 0.55, sun) + face * sun * 0.75 * (0.55 + 0.45 * open)) +
               base * float3(1.1, 1.2, 0.7) * through;
    return float4(c * 1.15, 1.0);
}
#endif
)HLSL";
}

}  // namespace uagfx::gpu
