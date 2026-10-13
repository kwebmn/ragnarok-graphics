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
    uint3 pad;
    float4 sourceRect;
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

// D3D7 puts pixel centres on integer coordinates, D3D12 on half-integers: x + 0.5 (calibration.md §3).
PsIn draw_vs(VsIn i) {
    PsIn o;
    float w = i.pos.w > 0.0 ? 1.0 / i.pos.w : 1.0;
    float x = (i.pos.x + 0.5) * 2.0 / targetSize.x - 1.0;
    float y = 1.0 - (i.pos.y + 0.5) * 2.0 / targetSize.y;
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

float4 draw_ps(PsIn i) : SV_Target {
    float4 t0 = tex0.Sample(samp0, ((stage0.y >> 16) & 7) == 1 ? i.uv1 : i.uv0);
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
    if (((alphaTest >> 16) & 1) != 0 && !passes(alphaTest & 255, round(c.a * 255.0), (alphaTest >> 8) & 255)) discard;
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

float4 present_ps(PresentOut i) : SV_Target {
    return float4(tex0.Sample(samp0, i.uv).rgb, 1);
}
)HLSL";
}

}  // namespace uagfx::gpu
