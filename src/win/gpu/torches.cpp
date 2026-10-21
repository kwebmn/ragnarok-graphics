#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "core/effect_light.hpp"
#include "core/fireflies.hpp"
#include "core/game_camera.hpp"
#include "core/map_glows.hpp"
#include "core/portal.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr float kReach = 45.0f;       // how far a torch's light reaches, world units (a cell is ten)
constexpr float kLightColor[3] = {1.0f, 0.58f, 0.28f};  // warm, as a wood fire
constexpr float kGlowRadius = 2.2f;  // the glow's pass, flame heights around the flame's middle
constexpr float kMiddle = 0.38f;     // the flame's middle, of its height from the base

D3D12_RECT clipped(float x0, float y0, float x1, float y1, int w, int h) {
    D3D12_RECT r{LONG(std::max(0.0f, std::floor(x0))), LONG(std::max(0.0f, std::floor(y0))),
                 LONG(std::min(float(w), std::ceil(x1))), LONG(std::min(float(h), std::ceil(y1)))};
    return r;
}

}  // namespace

// A world draw of one of the game's torch layers (core/torch.hpp) - known by its texture (Renderer::noteObjectTexture):
// true when it is not to be drawn, the glow not at all, the flame noted for applyTorches. Drawn as the game has it
// when the torches cannot be drawn anew here (no shaders, no copy of the picture) or the frame's are drawn already.
bool Renderer::Impl::takeTorch(const FrameRecord& rec, const DrawCmd& d) {
    const objects::Kind kind = objectTextures.of(d.texture[0]);
    if (kind != objects::Kind::TorchGlow && kind != objects::Kind::TorchFlame) return false;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > v.size()) return false;
    torch::Corner q[4];
    uint32_t diffuse = 0;
    for (int i = 0; i < 4; ++i) {
        const uint8_t* p = v.data() + d.vertexOffset + size_t(i) * stride;
        float xyzw[4], uv[2];
        std::memcpy(xyzw, p, sizeof xyzw);
        std::memcpy(uv, p + 24, sizeof uv);  // after the position, the diffuse and the specular colours
        q[i] = torch::Corner{xyzw[0], xyzw[1], xyzw[2], xyzw[3], uv[0], uv[1]};
        if (i == 0) std::memcpy(&diffuse, p + 16, sizeof diffuse);
    }
    if (!torchReady()) return false;
    if (kind == objects::Kind::TorchGlow) return true;
    torch::Flame f;
    // 2.6.17: a frame of the flame flameOf does not read is not shown as the game's either - the new flame keeps the
    // torch's place a moment (torch::Keeper); the game's blurred drop flashed up in it every couple of seconds
    if (!torch::flameOf(q, f) || f.rhw <= 0.0f) return effects.torchFlame;
    f.alpha = std::min(1.0f, float(diffuse >> 24) / 240.0f);  // the .act shows the flame at 240
    // 2.6.18: the torch's second effect is the same torch (torch::take); no room for another: the game's own
    const size_t before = torchFlames.size();
    if (!torch::take(torchFlames, f, kMaxTorches)) return false;
    if (torchFlames.size() == before && !torchesMergedLogged) {
        LOG_INFO("torches: a torch's second effect (the game burns each twice) taken as the same torch");
        torchesMergedLogged = true;
    }
    if (viewToWorldValid && !mapWorld.axes().known()) {  // 0.25: where it stands, for the map file's axes
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float vz = 1.0f / f.rhw, vx = (f.baseX - p.cx) * vz / p.f0, vy = (f.baseY - p.cy) * vz / p.f1;
        const float* m = viewToWorld;
        mapWorld.noteTorch(map::Point{vx * m[0] + vy * m[3] + vz * m[6] + m[9],
                                      vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                                      vx * m[2] + vy * m[5] + vz * m[8] + m[11]});
    }
    if (!torchesLogged) LOG_INFO("torches: the map's torches drawn anew");
    torchesLogged = true;
    return effects.torchFlame;  // 0.23.2: off - the game's own flame, the new glow and light around it
}

// 2.6.12: with the torches' pass off, a flame the game draws (known as takeTorch knows it, drawn as the game has it)
// noted for the creatures alone - the embers rise from it (applyCritters).
void Renderer::Impl::noteCritterFlame(const FrameRecord& rec, const DrawCmd& d) {
    if (objectTextures.of(d.texture[0]) != objects::Kind::TorchFlame) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    torch::Flame f;
    if (d.vertexOffset + 4 * stride <= v.size() && torch::flameOfQuad(v.data() + d.vertexOffset, stride, f))
        torch::take(critterFlames, f, kMaxTorches);  // 2.6.18: a torch's two effects one fire
}

// 0.26: a fire-coloured effect drawn this frame (draw.cpp) - its middle and depth from its vertices, how much it
// lights: its picture's fire (0.36: or its own light), the game's fade and colour of it, its size on the screen.
void Renderer::Impl::noteFire(const FrameRecord& rec, const DrawCmd& d, float amount, const float colour[3], bool orb) {
    if (fireSparks.size() >= 512) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const uint32_t n = std::min<uint32_t>(d.vertexCount, 256);
    if (n == 0 || d.vertexOffset + size_t(n) * stride > v.size()) return;
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f, rhw = 0.0f, shown = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* p = v.data() + d.vertexOffset + size_t(i) * stride;
        float xyzw[4];
        uint32_t diffuse = 0;
        std::memcpy(xyzw, p, sizeof xyzw);
        std::memcpy(&diffuse, p + 16, sizeof diffuse);
        x0 = std::min(x0, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        x1 = std::max(x1, xyzw[0]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3];
        const uint32_t top = std::max({diffuse >> 16 & 255u, diffuse >> 8 & 255u, diffuse & 255u});
        shown += float(diffuse >> 24) / 255.0f * float(top) / 255.0f;
    }
    rhw /= float(n);
    shown /= float(n);
    if (rhw <= 0.0f || shown <= 0.01f) return;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    FireSpark s;
    s.gameX = (x0 + x1) * 0.5f;
    s.gameY = (y0 + y1) * 0.5f;
    const float vz = 1.0f / rhw;
    s.view[0] = (s.gameX - p.cx) * vz / p.f0;
    s.view[1] = (s.gameY - p.cy) * vz / p.f1;
    s.view[2] = vz;
    const float size = std::sqrt(std::max(0.0f, (x1 - x0) * (y1 - y0)));  // game pixels
    s.strength = amount * shown * std::clamp(size / 120.0f, 0.15f, 1.5f);
    std::copy(colour, colour + 3, s.colour);
    s.orb = orb;
    fireSparks.push_back(s);
}

// 0.82: the skills' sparks about the places gathered (embers), added onto the world.
bool Renderer::Impl::applyEmbers() {
    const std::vector<Ember> places = std::move(embers);
    embers.clear();
    if (!ensureScene("skills' sparks")) return true;
    ID3D12PipelineState* pso = addPipeline(embersPs, embersPipelines, upscale ? 1 : samples, "skills' sparks");
    if (!pso) return true;
    if (!embersData) {
        embersData = createTexture2D(device, int(kMaxEmbers * 2), 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                     D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
        embersDataSrv = embersData ? allocSrv() : kNoSrv;
        if (!embersData || embersDataSrv == kNoSrv) {
            releaseCom(embersData);
            releaseCom(embersPs);
            return true;
        }
        embersDataState = D3D12_RESOURCE_STATE_COPY_DEST;
        device->CreateShaderResourceView(embersData, nullptr, cpuSrvHandle(embersDataSrv));
    }
    float data[kMaxEmbers * 2][4] = {};
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (size_t i = 0; i < places.size() && i < kMaxEmbers; ++i) {
        const Ember& e = places[i];
        data[2 * i][0] = e.x;
        data[2 * i][1] = e.y;
        data[2 * i][2] = e.size;
        data[2 * i][3] = e.strength;
        std::copy(e.colour, e.colour + 3, data[2 * i + 1]);
        data[2 * i + 1][3] = e.fire ? 1.0f : 0.0f;
        x0 = std::min(x0, e.x - 3.0f * e.size);
        y0 = std::min(y0, e.y - 3.0f * e.size);
        x1 = std::max(x1, e.x + 3.0f * e.size);
        y1 = std::max(y1, e.y + 3.0f * e.size);
    }
    const Upload u = allocUpload(sizeof data, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!u.cpu) return true;
    std::memcpy(u.cpu, data, sizeof data);
    D3D12_TEXTURE_COPY_LOCATION src{}, to{};
    src.pResource = slots[slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = u.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    src.PlacedFootprint.Footprint.Width = UINT(kMaxEmbers * 2);
    src.PlacedFootprint.Footprint.Height = 1;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = UINT(sizeof data);  // 768: a multiple of 256
    to.pResource = embersData;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    transition(embersData, embersDataState, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
    transition(embersData, embersDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const int w = sceneWidth, h = sceneHeight;
    Constants c{};
    c.scale = float(upscale ? worldScale : scale);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.footprint = float(std::min(places.size(), kMaxEmbers));
    c.sharpen = waterTime();
    const D3D12_RECT lit = clipped(x0, y0, x1, y1, w, h);
    if (lit.right <= lit.left || lit.bottom <= lit.top) return true;
    const UINT list1 = embersDataSrv;
    if (upscale)
        return effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneColorSrv, false, list1, &lit);
    return effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneColorSrv, false, list1, &lit);
}

// 0.38: a portal's draw (draw.cpp): its middle in the view and on the screen, gathered by place for its glow and swirl.
void Renderer::Impl::notePortal(const FrameRecord& rec, const DrawCmd& d) {
    if (portalSparks.size() >= 256) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const uint32_t n = std::min<uint32_t>(d.vertexCount, 64);
    if (n == 0 || d.vertexOffset + size_t(n) * stride > v.size()) return;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f, rhw = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, v.data() + d.vertexOffset + size_t(i) * stride, sizeof xyzw);
        x0 = std::min(x0, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        x1 = std::max(x1, xyzw[0]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3];
    }
    rhw /= float(n);
    if (rhw <= 0.0f) return;
    FireSpark s{};
    s.gameX = (x0 + x1) * 0.5f;
    s.gameY = (y0 + y1) * 0.5f;
    const float vz = 1.0f / rhw;
    s.view[0] = (s.gameX - p.cx) * vz / p.f0;
    s.view[1] = (s.gameY - p.cy) * vz / p.f1;
    s.view[2] = vz;
    s.strength = std::max(x1 - x0, y1 - y0);  // its size on the screen, game pixels
    portalSparks.push_back(s);
}

// 0.31: a cell of lava (draw.cpp): its middle in the view, to light around it.
void Renderer::Impl::noteLava(const FrameRecord& rec, const DrawCmd& d) {
    if (lavaCells.size() >= 4096) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    FireSpark s{};
    float rhw = 0.0f;
    for (uint32_t i = 0; i < 4; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, v.data() + d.vertexOffset + size_t(i) * stride, sizeof xyzw);
        s.gameX += xyzw[0] * 0.25f;
        s.gameY += xyzw[1] * 0.25f;
        rhw += xyzw[3] * 0.25f;
    }
    if (rhw <= 0.0f) return;
    const float vz = 1.0f / rhw;
    s.view[0] = (s.gameX - p.cx) * vz / p.f0;
    s.view[1] = (s.gameY - p.cy) * vz / p.f1;
    s.view[2] = vz;
    s.strength = 1.0f;
    lavaCells.push_back(s);
}

// 0.30: a face of one of the forest's beams (draw.cpp): its middle and its foot - the end its texture's v reaches 1
// at, the ground - in the view, from its vertices.
void Renderer::Impl::noteRay(const FrameRecord& rec, const DrawCmd& d) {
    if (rayParts.size() >= 512) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const uint32_t n = std::min<uint32_t>(d.vertexCount, 64);
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    RayPart r{};
    float sx = 0.0f, sy = 0.0f;
    int all = 0, feet = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* q = v.data() + d.vertexOffset + size_t(i) * stride;
        float xyzw[4], uv[2];
        std::memcpy(xyzw, q, sizeof xyzw);
        std::memcpy(uv, q + 24, sizeof uv);
        if (xyzw[3] <= 0.0f) continue;
        const float vz = 1.0f / xyzw[3];
        const float view[3] = {(xyzw[0] - p.cx) * vz / p.f0, (xyzw[1] - p.cy) * vz / p.f1, vz};
        for (int k = 0; k < 3; ++k) r.middle[k] += view[k];
        sx += xyzw[0];
        sy += xyzw[1];
        ++all;
        if (uv[1] >= 0.5f) {
            for (int k = 0; k < 3; ++k) r.foot[k] += view[k];
            ++feet;
        }
    }
    if (all == 0 || feet == 0) return;
    for (int k = 0; k < 3; ++k) {
        r.middle[k] /= float(all);
        r.foot[k] /= float(feet);
    }
    r.gameX = sx / float(all);
    r.gameY = sy / float(all);
    rayParts.push_back(r);
}

// 0.28: a firefly the game draws (effect 45, known by the driver: d.effect) - noted for applyTorches, not drawn: its
// middle, size and depth from its quad, the game's fade and tint of it. Drawn as the game has it when the fireflies
// cannot be drawn anew here.
bool Renderer::Impl::takeFirefly(const FrameRecord& rec, const DrawCmd& d) {
    if (fireflyDraws.size() >= kMaxFireflies) return false;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const uint32_t n = std::min<uint32_t>(d.vertexCount, 8);
    if (n < 3 || d.vertexOffset + size_t(n) * stride > v.size()) return false;
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f, rhw = 0.0f;
    uint32_t diffuse = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* p = v.data() + d.vertexOffset + size_t(i) * stride;
        float xyzw[4];
        std::memcpy(xyzw, p, sizeof xyzw);
        if (i == 0) std::memcpy(&diffuse, p + 16, sizeof diffuse);
        x0 = std::min(x0, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        x1 = std::max(x1, xyzw[0]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3];
    }
    rhw /= float(n);
    if (rhw <= 0.0f || !torchReady() || !fireflyReady()) return false;
    Firefly f;
    f.gameX = (x0 + x1) * 0.5f;
    f.gameY = (y0 + y1) * 0.5f;
    f.rhw = rhw;
    f.size = std::sqrt(std::max(0.0f, (x1 - x0) * (y1 - y0)));
    f.alpha = float(diffuse >> 24) / 255.0f;
    fireflies::colourOf(diffuse, f.colour);
    f.seed = float(d.effectSeed) / 255.0f;
    fireflyDraws.push_back(f);
    if (!firefliesLogged) LOG_INFO("fireflies: the game's fireflies drawn anew");
    firefliesLogged = true;
    return true;
}

// 0.28: the fireflies' pipeline for this target - their light added (alpha 0, the mask under DLSS untouched).
bool Renderer::Impl::fireflyReady() { return lightPipeline(fireflyPs, fireflyPipelines, "fireflies"); }

bool Renderer::Impl::lightPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[5], const char* what) {
    if (!ps || !open) return false;
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    const UINT at = upscale ? 4 : code;
    if (pipelines[at]) return true;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, ps, DXGI_FORMAT_B8G8R8A8_UNORM, upscale ? 1 : 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = D3D12_BLEND_ONE;
    b.DestBlend = D3D12_BLEND_ONE;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&pipelines[at])))) {
        pipelines[at] = nullptr;
        LOG_ERROR("%s: the pipeline failed: not drawn anew", what);
        releaseCom(ps);  // not tried again
        return false;
    }
    return true;
}

// The shaders, the pipelines for this target and the copies of the picture: false when any is missing (logged once).
bool Renderer::Impl::torchReady() {
    if (!torchLightVs || !torchAccumPs || !torchApplyPs || !torchFirePs || !open) return false;
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    const UINT fire = upscale ? 4 : code;
    if (!torchAccumPipeline) {  // 2.5: each light's quad into the buffer of light, added
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, torchLightVs, torchAccumPs, DXGI_FORMAT_R16G16B16A16_FLOAT, 1);
        D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
        b.BlendEnable = TRUE;
        b.SrcBlend = D3D12_BLEND_ONE;
        b.DestBlend = D3D12_BLEND_ONE;
        b.BlendOp = D3D12_BLEND_OP_ADD;
        b.SrcBlendAlpha = D3D12_BLEND_ONE;
        b.DestBlendAlpha = D3D12_BLEND_ONE;
        b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        b.LogicOp = D3D12_LOGIC_OP_NOOP;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&torchAccumPipeline)))) {
            torchAccumPipeline = nullptr;
            LOG_ERROR("torches: the light's pipeline failed: they stay the game's");
            releaseCom(torchAccumPs);  // not tried again
            return false;
        }
    }
    if (!torchLightPipelines[code]) {  // the buffer's sums into the picture
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, torchApplyPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
        p.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&torchLightPipelines[code])))) {
            torchLightPipelines[code] = nullptr;
            LOG_ERROR("torches: the light's pipeline onto %u samples failed: they stay the game's", 1u << code);
            releaseCom(torchApplyPs);  // not tried again
            return false;
        }
    }
    if (!torchFirePipelines[fire]) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, torchFirePs, DXGI_FORMAT_B8G8R8A8_UNORM, upscale ? 1 : 1u << code);
        D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
        b.BlendEnable = TRUE;  // premultiplied: the glow adds light, the flame hides some of what is behind it
        b.SrcBlend = D3D12_BLEND_ONE;
        b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOp = D3D12_BLEND_OP_ADD;
        b.SrcBlendAlpha = D3D12_BLEND_ONE;  // under DLSS the world's alpha is its mask (0.22.3): the flame marks it
        b.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        b.LogicOp = D3D12_LOGIC_OP_NOOP;
        if (!upscale)
            b.RenderTargetWriteMask =
                D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&torchFirePipelines[fire])))) {
            torchFirePipelines[fire] = nullptr;
            LOG_ERROR("torches: the fire's pipeline failed: they stay the game's");
            releaseCom(torchFirePs);
            return false;
        }
    }
    if (!torchData) {  // a texel a torch for the light's pass
        torchData = createTexture2D(device, int(kMaxLights * 2), 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                    D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
        torchDataSrv = torchData ? allocSrv() : kNoSrv;
        if (!torchData || torchDataSrv == kNoSrv) {
            LOG_ERROR("torches: no memory for their list: they stay the game's");
            releaseCom(torchData);
            releaseCom(torchAccumPs);
            return false;
        }
        torchDataState = D3D12_RESOURCE_STATE_COPY_DEST;
        device->CreateShaderResourceView(torchData, nullptr, cpuSrvHandle(torchDataSrv));
    }
    return ensureScene("torches");
}

// 2.5: the buffer of light at the world's size - made anew when the size changes; false: none at this size.
bool Renderer::Impl::ensureTorchLight(int w, int h) {
    if (torchLight && w == torchLightW && h == torchLightH) return true;
    if (torchLightFailed && w == torchLightW && h == torchLightH) return false;
    discard(torchLight);
    torchLight = nullptr;
    torchLightW = w;
    torchLightH = h;
    D3D12_CLEAR_VALUE none{};
    none.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    torchLight = createTexture2D(device, w, h, 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
                                 D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, &none);
    if (torchLight && torchLightSrv == kNoSrv) torchLightSrv = allocSrv();
    if (!torchLight || torchLightSrv == kNoSrv) {
        LOG_ERROR("torches: no video memory for their light at %dx%d: they stay the game's", w, h);
        releaseCom(torchLight);
        torchLightFailed = true;
        return false;
    }
    nameResource(torchLight, "torches' light");
    torchLightState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    torchLightFailed = false;
    device->CreateShaderResourceView(torchLight, nullptr, cpuSrvHandle(torchLightSrv));
    device->CreateRenderTargetView(torchLight, nullptr, rtvHandle(kRtvTorch));
    return true;
}

namespace {

// A light of the light's pass (torch_light_ps, two texels): its place in the view, strength, colour and reach.
struct LightItem {
    float view[3];
    float strength;
    float color[3];
    float reach;  // world units
};

// 0.25: the map file's torches the game does not show (it knows effect ids up to 629): their colours.
bool hiddenTorch(int32_t id, float tint[3]) {
    static const float kRed[3] = {1.0f, 0.32f, 0.26f}, kGreen[3] = {0.42f, 1.0f, 0.38f};
    static const float kPurple[3] = {0.78f, 0.4f, 1.0f};
    const float* c = id == 690 ? kRed : id == 691 ? kGreen : id == 696 ? kPurple : nullptr;
    if (!c) return false;
    std::copy(c, c + 3, tint);
    return true;
}

// A place of the game's world in the view (the camera backwards, inverted: its rows are orthonormal).
map::Point toView(const float m[12], const map::Point& w) {
    const float dx = w.x - m[9], dy = w.y - m[10], dz = w.z - m[11];
    return map::Point{dx * m[0] + dy * m[1] + dz * m[2], dx * m[3] + dy * m[4] + dz * m[5],
                      dx * m[6] + dy * m[7] + dz * m[8]};
}

}  // namespace

// Once the world is drawn: the light of this frame's flames and lamps into the picture (one pass, from its copy),
// then each flame and the glow around it (a pass each, cut to it). 0.25: with the map file read and its axes known,
// the torches the game does not show and the lamps' lights join those the game draws.
bool Renderer::Impl::applyTorches() {
    torchDone = true;  // the world's draws after this are the game's own
    EffectTime timing(*this, kTimeLights);  // 0.39.2
    mapWorld.poll();
    std::vector<torch::Flame> flames;
    flames.swap(torchFlames);
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float time = waterTime();  // the effects' clock
    const float amount = torch::flickerAmount(effects.torchFlicker);  // 2.6.16: the slider's ends seen
    // 2.6.17: each of the game's flames followed from frame to frame - its torch's seed kept, its fade smoothed, a
    // frame without it bridged (torch::Keeper): the new flame jumped to another shape every couple of seconds
    std::vector<uint32_t> torchSeeds;
    for (const torch::Flame& f : flames) torchSeeds.push_back(torch::placeSeed(f, proj, viewToWorld, viewToWorldValid));
    torchKeeper.update(flames, torchSeeds, double(time));
    // 2.6.18: each of these stands for the game's two effects of its torch (torch::take) - lit, hot and drawn as the
    // two were (torch::kGameEffects); the torches the game does not show, added after them, are one each
    const size_t gameFlames = flames.size();
    const rsw::World* layout = viewToWorldValid && mapWorld.axes().known() ? mapWorld.world() : nullptr;
    const map::Axes axes = mapWorld.axes();
    // Off the screen by more than this (game pixels) a flame or a lamp's middle is not drawn nor lit from.
    const float margin = float(std::max(gameWidth, gameHeight)) * 0.25f;
    const auto onScreen = [&](float gx, float gy, float slack) {
        return gx > -slack && gy > -slack && gx < float(gameWidth) + slack && gy < float(gameHeight) + slack;
    };
    if (layout && torchWanted())
        for (const rsw::Effect& e : layout->effects) {
            torch::Flame f;
            if (flames.size() >= kMaxTorches || !hiddenTorch(e.id, f.tint)) continue;
            const map::Point base = map::toWorld(axes, map::Point{e.pos[0], e.pos[1] - 2.0f, e.pos[2]});
            const map::Point vb = toView(viewToWorld, base);
            const map::Point vt = toView(viewToWorld, map::Point{base.x, base.y - 13.0f, base.z});  // 13 units tall
            if (vb.z <= proj.nearZ || vt.z <= proj.nearZ) continue;
            f.baseX = vb.x / vb.z * proj.f0 + proj.cx;
            f.baseY = vb.y / vb.z * proj.f1 + proj.cy;
            f.upX = vt.x / vt.z * proj.f0 + proj.cx - f.baseX;
            f.upY = vt.y / vt.z * proj.f1 + proj.cy - f.baseY;
            f.halfWidth = std::hypot(f.upX, f.upY) * (torch::kHalfWidth / torch::kHeight);
            f.rhw = 1.0f / vb.z;
            if (!onScreen(f.baseX, f.baseY, margin)) continue;
            flames.push_back(f);
            torchSeeds.push_back(torch::seedAt(base.x, base.z));  // its exact place: a steady seed
        }
    if (torchWanted()) critterFlames = flames;  // 2.5.4: the embers rise from them; off: noteCritterFlame's
    // 0.28: the fireflies - the game's (their flash by their primitive's seed) and, the map file read, those the
    // maps' makers placed that the game does not show (713, 714, 946), drifting about their places.
    struct Fly {
        float gx, gy, vz, r, bright, colour[3];  // game pixels, view depth, the core's radius, brightness
        float sharp = 1.2f, halo = 1.0f;         // a firefly's; 0.29: the maps' glows softer
    };
    std::vector<Fly> flies;
    if (fireflyWanted() && fireflyReady()) {
        const float user = float(std::clamp(effects.firefliesBrightness, 0, 200)) / 100.0f;
        for (const Firefly& g : fireflyDraws) {
            const float bright = user * g.alpha * fireflies::blink(time, g.seed);
            flies.push_back(Fly{g.gameX, g.gameY, 1.0f / g.rhw, std::max(g.size * 0.06f, 0.4f), bright,
                                {g.colour[0], g.colour[1], g.colour[2]}});
        }
        if (layout) {
            uint32_t index = 0, drawn = 0;
            for (const rsw::Effect& e : layout->effects) {
                if (e.id != 713 && e.id != 714 && e.id != 946) continue;
                const float seed = float(((++index) * 2654435761u) >> 8 & 0xFFFFu) / 65535.0f;
                if (flies.size() >= kMaxFireflies) break;
                const map::Point o = fireflies::wander(time, seed);
                const map::Point w = map::toWorld(axes, map::Point{e.pos[0] + o.x, e.pos[1] + o.y, e.pos[2] + o.z});
                const map::Point v = toView(viewToWorld, w);
                if (v.z <= proj.nearZ) continue;
                const float gx = v.x / v.z * proj.f0 + proj.cx, gy = v.y / v.z * proj.f1 + proj.cy;
                if (!onScreen(gx, gy, 32.0f)) continue;
                flies.push_back(Fly{gx, gy, v.z, 0.22f * proj.f0 / v.z, user * fireflies::blink(time, seed),
                                    {1.0f, 0.86f, 0.3f}});
                ++drawn;
            }
            if (drawn && !mapFirefliesLogged) LOG_INFO("fireflies: the map file's own drawn (%u in view)", drawn);
            mapFirefliesLogged = mapFirefliesLogged || drawn > 0;
        }
    }
    fireflyDraws.clear();
    // 0.29: the maps' hidden lights (core/map_glows.hpp): an orb floats and breathes, a pool lights the ground about
    // it, a cloud of dust drifts - and the orbs and pools light around them, the nearest 24.
    struct Cloud {
        float gx, gy, vz, perUnit, radius, seed, bright, colour[3];  // perUnit: game pixels a world unit there
    };
    std::vector<Cloud> clouds;
    std::vector<LightItem> glowLights;
    if (layout && mapGlowsWanted() && fireflyReady()) {
        const float user = float(std::clamp(effects.mapGlowsBrightness, 0, 200)) / 100.0f;
        uint32_t index = 0, drawn = 0;
        for (const rsw::Effect& e : layout->effects) {
            map_glows::Glow g;
            if (!map_glows::glowOf(e.id, g)) continue;
            const float seed = float(((++index) * 2246822519u) >> 8 & 0xFFFFu) / 65535.0f;
            const float lift = g.shape == map_glows::Shape::Orb ? map_glows::bob(time, seed) : 0.0f;
            const map::Point w = map::toWorld(axes, map::Point{e.pos[0], e.pos[1] + lift, e.pos[2]});
            const map::Point v = toView(viewToWorld, w);
            if (v.z <= proj.nearZ) continue;
            const float gx = v.x / v.z * proj.f0 + proj.cx, gy = v.y / v.z * proj.f1 + proj.cy;
            const float perUnit = proj.f0 / v.z;
            const float breath = g.shape == map_glows::Shape::Dust ? 1.0f : map_glows::pulse(time, seed);
            const float slack = std::max(g.reach, g.size) * perUnit + 32.0f;
            if (!onScreen(gx, gy, slack)) continue;
            if (g.light > 0.0f)
                glowLights.push_back(LightItem{{v.x, v.y, v.z}, g.light * breath * user,
                                               {g.colour[0], g.colour[1], g.colour[2]}, g.reach});
            if (!onScreen(gx, gy, g.size * perUnit + 32.0f)) continue;
            if (g.shape == map_glows::Shape::Dust) {
                if (clouds.size() < 32 && dustPs)
                    clouds.push_back(Cloud{gx, gy, v.z, perUnit, g.size, seed, 0.35f * user,
                                           {g.colour[0], g.colour[1], g.colour[2]}});
            } else if (flies.size() < kMaxFireflies + 64) {
                const bool pool = g.shape == map_glows::Shape::Pool;
                Fly f{gx, gy, v.z, g.size * perUnit, (pool ? 0.18f : 0.75f) * breath * user,
                      {g.colour[0], g.colour[1], g.colour[2]}};
                f.sharp = pool ? 0.15f : 0.4f;
                f.halo = pool ? 0.6f : 1.6f;
                flies.push_back(f);
            }
            ++drawn;
        }
        if (drawn && !mapGlowsLogged) LOG_INFO("hidden lights: the map file's drawn (%u in view)", drawn);
        mapGlowsLogged = mapGlowsLogged || drawn > 0;
    }
    // 0.30: the forest's beams, gathered by place (cells of 64 game pixels): dust drifting in each, light where it
    // falls - the nearest 12.
    if (!rayParts.empty()) {
        struct Beam {
            float middle[3] = {0, 0, 0}, foot[3] = {0, 0, 0};
            float gx = 0, gy = 0;
            int n = 0;
        };
        std::unordered_map<int, Beam> beams;
        for (const RayPart& r : rayParts) {
            Beam& b = beams[int(std::floor(r.gameX / 64.0f)) * 4096 + int(std::floor(r.gameY / 64.0f))];
            for (int k = 0; k < 3; ++k) {
                b.middle[k] += r.middle[k];
                b.foot[k] += r.foot[k];
            }
            b.gx += r.gameX;
            b.gy += r.gameY;
            ++b.n;
        }
        std::vector<Beam> closest;
        for (auto& kv : beams) {
            Beam b = kv.second;
            for (int k = 0; k < 3; ++k) {
                b.middle[k] /= float(b.n);
                b.foot[k] /= float(b.n);
            }
            b.gx /= float(b.n);
            b.gy /= float(b.n);
            closest.push_back(b);
        }
        std::sort(closest.begin(), closest.end(),
                  [](const Beam& a, const Beam& b) { return a.middle[2] < b.middle[2]; });
        const float user = float(std::clamp(effects.raysBrightness, 0, 200)) / 100.0f;
        static const float kSun[3] = {0.95f, 1.0f, 0.82f};
        for (size_t i = 0; i < closest.size() && i < 12; ++i) {
            const Beam& b = closest[i];
            if (b.middle[2] <= proj.nearZ) continue;
            glowLights.push_back(
                LightItem{{b.foot[0], b.foot[1], b.foot[2]}, 0.22f * user, {kSun[0], kSun[1], kSun[2]}, 20.0f});
            if (dustPs && clouds.size() < 32)
                clouds.push_back(Cloud{b.gx, b.gy, b.middle[2], proj.f0 / b.middle[2], 6.0f,
                                       float(i) * 0.137f + 0.05f, 0.3f * user, {1.0f, 0.97f, 0.86f}});
        }
        rayParts.clear();
    }
    // 0.23.3: the shader bounds it. 0.25.2: four fifths of 0.25's at 100 % ("too much and too dense" once the torch
    // has come up from its fade); 0.25.3: a little less again ("ещё чуточку"); 0.25.4: near half ("in the original
    // not so bright" - the game's torch lights nothing, its glow is baked into the ground).
    const float strength = 0.4f * float(std::clamp(effects.torchLight, 0, 200)) / 100.0f;
    std::vector<LightItem> lights;
    skillAirFirst = skillAirEnd = 0;  // 2.7.36: none until the skills' come
    const float darkNow = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;  // 2.7.18: how much night it is
    std::vector<float> flickers(flames.size(), 1.0f), seeds(flames.size(), 0.0f);
    for (size_t i = 0; i < flames.size(); ++i) {
        const torch::Flame& f = flames[i];
        const float mx = f.baseX + f.upX * kMiddle, my = f.baseY + f.upY * kMiddle;  // game pixels
        const float vz = 1.0f / f.rhw;
        const float vx = (mx - proj.cx) * vz / proj.f0, vy = (my - proj.cy) * vz / proj.f1;
        // its torch's: its own flicker and flame whichever way the camera looks (2.6.17: kept as it is followed)
        const uint32_t s =
            i < torchSeeds.size() ? torchSeeds[i] : torch::placeSeed(f, proj, viewToWorld, viewToWorldValid);
        flickers[i] = torch::flicker(time, s, amount);
        seeds[i] = float(s % 1000u) / 1000.0f;
        if (strength <= 0.0f) continue;
        // 0.25.1: the light on the ground flickers less and slower than the flame ("мигает слишком сильно"): a
        // third of its swing at half its pace.
        const float calm = torch::flicker(double(time) * 0.5, s ^ 0x5BD1E995u, amount * 0.35f);
        const float layers = i < gameFlames ? torch::kGameEffects : 1.0f;  // 2.6.18
        LightItem l{{vx, vy, vz}, strength * calm * f.alpha * layers, {}, kReach};
        const bool hidden = f.tint[0] < 0.99f || f.tint[1] < 0.99f || f.tint[2] < 0.99f;  // its own colour
        std::copy(hidden ? f.tint : kLightColor, (hidden ? f.tint : kLightColor) + 3, l.color);
        lights.push_back(l);
    }
    // 0.26: the fire of skills, gathered by place (cells of 80 game pixels), the brightest 16. A little, as a torch:
    // the shader bounds it.
    size_t skillPlaces = 0, orbDraws = 0, orbPlaces = 0;  // 2.7.20: for the log at night
    float skillBrightest = 0.0f;
    if (fireWanted() && !fireSparks.empty()) {
        // 2.7.22: the orbs of light (Ruwach's, Sight's) first, by caster - one calm light each over its head
        // (effect_light::orbLight; 2.7.21's cells split an orbit into lights by the ground that jumped as it flew,
        // the user: «слишком ярко ... чтобы было спокойнее»)
        // 2.7.24: a ground light (its strength negative, the shaders light what lies near the ground alone),
        // effect_light::kGroundLight over the player's ground (the camera's target) - else o.height over the middle
        float up[3] = {0.0f, 0.0f, 0.0f};
        if (viewToWorldValid) camera::upInView(viewToWorld, up);
        const bool groundKnown = viewToWorldValid && cameraTargetZ > 0.0f;
        const float groundY = groundKnown ? camera::targetHeight(viewToWorld, cameraTargetZ) : 0.0f;
        const float fire = float(std::clamp(effects.fireLightStrength, 0, 200)) / 100.0f;
        for (const effect_light::OrbPlace& p : effect_light::gatherOrbs(fireSparks, 160.0f)) {
            if (lights.size() >= kMaxLights) break;
            const effect_light::OrbLight o = effect_light::orbLight(p.w, darkNow, fire);
            const float top = std::max({p.colour[0], p.colour[1], p.colour[2], 1e-4f});
            const float centre[3] = {p.view[0] / p.w, p.view[1] / p.w, p.view[2] / p.w};
            const float raise = groundKnown ? effect_light::raiseOverGround(centre, viewToWorld, groundY) : o.height;
            LightItem l{{centre[0] + raise * up[0], centre[1] + raise * up[1], centre[2] + raise * up[2]},
                        -o.strength, {p.colour[0] / top, p.colour[1] / top, p.colour[2] / top}, o.reach};
            lights.push_back(l);
            ++orbPlaces;
        }
        struct Cell {
            float w = 0, view[3] = {0, 0, 0}, colour[3] = {0, 0, 0};
        };
        std::unordered_map<int, Cell> cells;
        for (const FireSpark& f : fireSparks) {
            if (f.orb) {  // 2.7.21: an orb of light's - lit above
                ++orbDraws;
                continue;
            }
            Cell& c = cells[int(std::floor(f.gameX / 80.0f)) * 4096 + int(std::floor(f.gameY / 80.0f))];
            c.w += f.strength;
            for (int k = 0; k < 3; ++k) {
                c.view[k] += f.view[k] * f.strength;
                c.colour[k] += f.colour[k] * f.strength;
            }
        }
        std::vector<Cell> best;
        for (const auto& kv : cells)
            if (kv.second.w > 0.02f) best.push_back(kv.second);
        std::sort(best.begin(), best.end(), [](const Cell& a, const Cell& b) { return a.w > b.w; });
        // 0.26.1: a third of 0.26's, the brightest place no more than one fire ("светит слишком сильно, намного
        // помягче"), a shorter reach.
        // 2.7.18: brighter at night (the user: «скилы должны освещать»); 2.7.36: up to 1.4 times, not 1.8
        const float fireStrength =
            effect_light::skillStrength(float(std::clamp(effects.fireLightStrength, 0, 200)) / 100.0f, darkNow);
        if (heatWanted()) {  // 0.35: the hot air over burning skills (their middles, game pixels to the target's)
            const float hk = float(upscale ? worldScale : scale);
            for (size_t i = 0; i < best.size() && i < 16; ++i) {
                const Cell& c = best[i];
                const float vz = c.view[2] / c.w;
                if (vz <= proj.nearZ) continue;
                const float gx = c.view[0] / c.w / vz * proj.f0 + proj.cx;
                const float gy = c.view[1] / c.w / vz * proj.f1 + proj.cy;
                heatSpots.push_back(HeatSpot{gx * hk, gy * hk, 70.0f * hk, 0.6f * std::min(c.w, 1.0f) * heatUser()});
            }
        }
        embers.clear();  // 0.82: the places for the skills' sparks
        const float ek = float(upscale ? worldScale : scale);
        for (size_t i = 0; i < best.size() && embers.size() < kMaxEmbers && effects.skillSparks; ++i) {
            const Cell& c = best[i];
            const float vz = c.view[2] / c.w;
            if (vz <= proj.nearZ) continue;
            const float top = std::max({c.colour[0], c.colour[1], c.colour[2], 1e-4f});
            Ember e{};
            e.x = (c.view[0] / c.w / vz * proj.f0 + proj.cx) * ek + 0.5f + (upscale ? jitter[0] : 0.0f);
            e.y = (c.view[1] / c.w / vz * proj.f1 + proj.cy) * ek + 0.5f + (upscale ? jitter[1] : 0.0f);
            e.size = 7.0f * proj.f0 / vz * ek;  // about seven units across
            e.strength = 0.55f * std::min(c.w, 1.0f) * float(std::clamp(effects.skillSparksAmount, 0, 200)) / 100.0f;
            for (int k = 0; k < 3; ++k) e.colour[k] = c.colour[k] / top;
            e.fire = e.colour[0] > 0.8f && e.colour[2] < 0.6f;
            embers.push_back(e);
        }
        skillAirFirst = UINT(lights.size());
        for (size_t i = 0; i < best.size() && i < 16 && lights.size() < kMaxLights; ++i) {
            const Cell& c = best[i];
            const float top = std::max({c.colour[0], c.colour[1], c.colour[2], 1e-4f});
            const float w = effect_light::skillWeight(c.w, darkNow);  // 2.7.20: at night a floor
            LightItem l{{c.view[0] / c.w, c.view[1] / c.w, c.view[2] / c.w}, fireStrength * w,
                        {c.colour[0] / top, c.colour[1] / top, c.colour[2] / top}, 25.0f + 15.0f * w};
            lights.push_back(l);
        }
        skillAirEnd = UINT(lights.size());
        skillPlaces = best.size();
        skillBrightest = best.empty() ? 0.0f : best[0].w;
    }
    const float since = time - skillLightsTold;  // 2.7.20: at night, what of the skills lights, now and then
    if (darkNow > 0.0f && (since > 10.0f || since < 0.0f)) {
        skillLightsTold = time;
        LOG_INFO("skills' light at night: %s, %zu draws in %zu places, the brightest %.2f; %zu of orbs of light, %zu "
                 "casters",
                 fireWanted() ? "on" : "off", fireSparks.size() - orbDraws, skillPlaces, skillBrightest, orbDraws,
                 orbPlaces);
    }
    fireSparks.clear();
    // 0.28: the fireflies light a little around them, the nearest 16.
    if (!flies.empty()) {
        std::vector<size_t> order(flies.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return flies[a].vz < flies[b].vz; });
        for (size_t i = 0, used = 0; i < order.size() && used < 16 && lights.size() < kMaxLights; ++i) {
            const Fly& f = flies[order[i]];
            if (f.bright < 0.05f) continue;
            const float vx = (f.gx - proj.cx) * f.vz / proj.f0, vy = (f.gy - proj.cy) * f.vz / proj.f1;
            lights.push_back(
                LightItem{{vx, vy, f.vz}, 0.12f * f.bright, {f.colour[0], f.colour[1], f.colour[2]}, 12.0f});
            ++used;
        }
    }
    // 2.7.18: the fireflies in the grass at night light the ground under them a little, pulsing with their glow
    if (nightGlowsWanted())
        for (const NightGlow& g : nightGlows) {
            if (lights.size() >= kMaxLights) break;
            const map::Point v = toView(viewToWorld, g.at);
            if (v.z <= proj.nearZ) continue;
            lights.push_back(LightItem{{v.x, v.y, v.z}, 0.12f * g.lit, {0.75f, 1.0f, 0.42f}, 9.0f});
        }
    // 0.31: the lava's heat, gathered by place (cells of 96 game pixels): the nearest 12 light around, breathing.
    if (!lavaCells.empty() && lavaWanted()) {
        struct Pool {
            float view[3] = {0, 0, 0};
            int n = 0;
        };
        std::unordered_map<int, Pool> pools;
        for (const FireSpark& s : lavaCells) {
            Pool& q = pools[int(std::floor(s.gameX / 96.0f)) * 4096 + int(std::floor(s.gameY / 96.0f))];
            for (int k = 0; k < 3; ++k) q.view[k] += s.view[k];
            ++q.n;
        }
        std::vector<Pool> heat;
        for (auto& kv : pools) {
            Pool q = kv.second;
            for (int k = 0; k < 3; ++k) q.view[k] /= float(q.n);
            heat.push_back(q);
        }
        std::sort(heat.begin(), heat.end(), [](const Pool& a, const Pool& b) { return a.view[2] < b.view[2]; });
        const float user = float(std::clamp(effects.lavaBrightness, 0, 200)) / 100.0f;
        for (size_t i = 0; i < heat.size() && i < 12 && lights.size() < kMaxLights; ++i) {
            const Pool& q = heat[i];
            const float breath = 0.85f + 0.15f * std::sin(time * 1.1f + float(i) * 2.3f);
            const float strength = 0.35f * user * breath;
            LightItem l{{q.view[0], q.view[1] - 3.0f, q.view[2]}, strength, {1.0f, 0.42f, 0.12f}, 35.0f};
            lights.push_back(l);
            if (heatWanted() && q.view[2] > proj.nearZ) {  // 0.35: the hot air over the lava
                const float hk = float(upscale ? worldScale : scale);
                const float gx = q.view[0] / q.view[2] * proj.f0 + proj.cx;
                const float gy = q.view[1] / q.view[2] * proj.f1 + proj.cy;
                heatSpots.push_back(HeatSpot{gx * hk, gy * hk, 110.0f * hk, 0.45f * heatUser()});
            }
        }
    }
    lavaCells.clear();
    // 0.38: the portals - a soft blue glow about each and (in the heat haze's pass) the air turning in it. 0.39.1:
    // this frame's draws gathered into what stands there, followed from frame to frame - their places eased, their
    // strength fading in and out - so that the glow holds steady while the rings rise and come and go (0.38 lit each
    // frame's gathering by a grid: as the rings crossed its lines the lights doubled and went out - it blinked).
    const float portalStep = std::clamp(time - portalTime, 0.0f, 0.25f);
    portalTime = time;
    if (!portalsWanted()) portalTracks.clear();
    if (portalsWanted()) {
        struct Gate {
            float view[3] = {0, 0, 0}, gx = 0, gy = 0, size = 0;
            int n = 0;
            bool claimed = false;
        };
        std::vector<Gate> seen;
        for (const FireSpark& s : portalSparks) {
            Gate* g = nullptr;
            for (Gate& o : seen)
                if (std::hypot(o.gx / float(o.n) - s.gameX, o.gy / float(o.n) - s.gameY) <
                    std::max(80.0f, std::max(o.size, s.strength) * 0.8f))
                    g = &o;
            if (!g) {
                seen.push_back(Gate{});
                g = &seen.back();
            }
            for (int k = 0; k < 3; ++k) g->view[k] += s.view[k];
            g->gx += s.gameX;
            g->gy += s.gameY;
            g->size = std::max(g->size, s.strength);
            ++g->n;
        }
        for (Gate& g : seen) {
            for (int k = 0; k < 3; ++k) g.view[k] /= float(g.n);
            g.gx /= float(g.n);
            g.gy /= float(g.n);
        }
        const float ease = 1.0f - std::exp(-portalStep * 5.0f);
        for (PortalTrack& t : portalTracks) {
            Gate* best = nullptr;
            float bestD = 140.0f;
            for (Gate& g : seen) {
                const float d = std::hypot(g.gx - t.gx, g.gy - t.gy);
                if (!g.claimed && d < bestD) {
                    bestD = d;
                    best = &g;
                }
            }
            if (best) {
                best->claimed = true;
                for (int k = 0; k < 3; ++k) t.view[k] += (best->view[k] - t.view[k]) * ease;
                t.gx += (best->gx - t.gx) * ease;
                t.gy += (best->gy - t.gy) * ease;
                t.size += (best->size - t.size) * ease;
                t.strength = std::min(1.0f, t.strength + portalStep / 0.4f);
            } else {
                t.strength -= portalStep / 1.2f;  // its rings gone a moment: it fades rather than goes out
            }
        }
        for (const Gate& g : seen)
            if (!g.claimed && portalTracks.size() < 16)
                portalTracks.push_back(PortalTrack{{g.view[0], g.view[1], g.view[2]}, g.gx, g.gy, g.size, 0.0f});
        portalTracks.erase(std::remove_if(portalTracks.begin(), portalTracks.end(),
                                          [](const PortalTrack& t) { return t.strength <= 0.0f; }),
                           portalTracks.end());
        const float user = float(std::clamp(effects.portalsStrength, 0, 200)) / 100.0f;
        const float hk = float(upscale ? worldScale : scale);
        int lit = 0;
        for (const PortalTrack& t : portalTracks) {
            const float shown = t.strength * t.strength * (3.0f - 2.0f * t.strength);  // eased in and out
            if (shown <= 0.0f) continue;
            // 2.7.48: under the new portal its light in its lilac and softer, the air's swirl half - no white blot
            const bool anew = newPortalWanted();
            if (lit++ < 8 && lights.size() < kMaxLights) {
                LightItem l{{t.view[0], t.view[1] - 4.0f, t.view[2]}, (anew ? 0.21f : 0.35f) * user * shown,
                            {anew ? 0.80f : 0.45f, anew ? 0.74f : 0.75f, 1.0f}, 30.0f};
                lights.push_back(l);
            }
            if (heatPs) {
                const float r = std::clamp(t.size * 0.6f, 30.0f, 140.0f) * hk;
                heatSpots.push_back(
                    HeatSpot{t.gx * hk, t.gy * hk, r, (anew ? -0.35f : -0.7f) * std::min(user, 1.5f) * shown});
            }
        }
    }
    portalSparks.clear();
    // 0.29: the maps' hidden lights, the nearest 24.
    std::sort(glowLights.begin(), glowLights.end(),
              [](const LightItem& a, const LightItem& b) { return a.view[2] < b.view[2]; });
    for (size_t i = 0; i < glowLights.size() && i < 24 && lights.size() < kMaxLights; ++i)
        lights.push_back(glowLights[i]);
    // 0.25: the lamps the map's makers placed, the nearest to the camera first, as many as the pass takes.
    if (layout && lampsWanted() && effects.lampsLight > 0) {
        const float lampStrength = 0.8f * float(std::clamp(effects.lampsLight, 0, 200)) / 100.0f;
        std::vector<LightItem> lamps;
        for (const rsw::Light& r : mapWorld.lamps()) {
            const map::Point w = map::toWorld(axes, map::Point{r.pos[0], r.pos[1], r.pos[2]});
            const map::Point v = toView(viewToWorld, w);
            const float reach = std::clamp(r.range * 0.45f, 15.0f, 70.0f);
            if (v.z <= proj.nearZ - reach) continue;
            const float vz = std::max(v.z, proj.nearZ);
            const float gx = v.x / vz * proj.f0 + proj.cx, gy = v.y / vz * proj.f1 + proj.cy;
            if (!onScreen(gx, gy, reach * proj.f0 / vz + margin)) continue;
            const float top = std::max({r.color[0], r.color[1], r.color[2], 1e-3f});
            // 2.1.5: a third of the colour only - the game has the lamps' colours in its light already; full ones
            // (0.2 0.3 0.4 made blue) laid rainbow patches over c_tower2's floor.
            float tint[3];
            for (int k = 0; k < 3; ++k) tint[k] = 0.65f + 0.35f * r.color[k] / top;
            LightItem l{{v.x, v.y, v.z}, lampStrength, {tint[0], tint[1], tint[2]}, reach};
            // A fire's colour - red the most, then green, then blue - flickers, each its own way.
            if (r.color[0] >= r.color[1] && r.color[1] >= r.color[2] * 0.9f && r.color[0] > 0.5f)
                l.strength *= torch::flicker(double(time) * 0.5, torch::seedAt(w.x, w.z), amount * 0.25f);
            lamps.push_back(l);
        }
        std::sort(lamps.begin(), lamps.end(),
                  [](const LightItem& a, const LightItem& b) { return a.view[2] < b.view[2]; });
        for (const LightItem& l : lamps) {
            if (lights.size() >= kMaxLights) break;
            lights.push_back(l);
        }
    }
    if (lights.size() > kMaxLights) lights.resize(kMaxLights);
    if ((flames.empty() && lights.empty() && flies.empty() && clouds.empty()) || !torchReady()) return true;
    if (!copySceneColor() || !copySceneDepth()) return false;
    const float k = float(upscale ? worldScale : scale);
    const float jx = upscale ? jitter[0] : 0.0f, jy = upscale ? jitter[1] : 0.0f;
    const int w = sceneWidth, h = sceneHeight;
    const float glowStrength = float(std::clamp(effects.torchGlow, 0, 200)) / 100.0f;
    // 0.23.2: the flame softer than 0.23's at 100 % ("too harsh"), none where the game's own is drawn - the torches
    // the game does not show always have the new one (0.25).
    const float brightness = 0.65f * float(std::clamp(effects.torchBrightness, 0, 200)) / 100.0f;  // 0.25.4: was 0.8
    ID3D12Resource* dst = upscale ? world : target;
    D3D12_RESOURCE_STATES& dstState = upscale ? worldState : targetState;
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(upscale ? kRtvWorld : kRtvTarget);
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    Constants c{};
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    if (viewToWorldValid) std::copy(viewToWorld, viewToWorld + 12, c.reproject);  // 2.7.24: the ground lights' up
    c.scale = k;
    c.jitter[0] = jx;
    c.jitter[1] = jy;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    if (!lights.empty()) {  // the list to the GPU: two texels a light
        float data[kMaxLights * 2][4] = {};
        float lx0 = float(w), ly0 = float(h), lx1 = 0.0f, ly1 = 0.0f;  // where the light falls on the screen
        for (size_t i = 0; i < lights.size(); ++i) {
            const LightItem& l = lights[i];
            // 0.39.2: its place and reach first, its colour and strength second - the pass reads the second only
            // for the lights that reach a pixel.
            std::copy(l.view, l.view + 3, data[2 * i]);
            data[2 * i][3] = l.reach;
            std::copy(l.color, l.color + 3, data[2 * i + 1]);
            data[2 * i + 1][3] = l.strength;
            // 0.39.2: where its reach falls on the screen - the cube about it, each corner projected, the nearer ones
            // further out. A light up in the air lights the ground well below it on the screen: 0.25-0.39.1 cut it off
            // at a square about the light's own place (the skills' light came out square).
            for (int corner = 0; corner < 8; ++corner) {
                const float px = l.view[0] + ((corner & 1) ? l.reach : -l.reach);
                const float py = l.view[1] + ((corner & 2) ? l.reach : -l.reach);
                const float pz = std::max(l.view[2] + ((corner & 4) ? l.reach : -l.reach), proj.nearZ);
                const float tx = (px / pz * proj.f0 + proj.cx) * k + 0.5f + jx;
                const float ty = (py / pz * proj.f1 + proj.cy) * k + 0.5f + jy;
                lx0 = std::min(lx0, tx);
                ly0 = std::min(ly0, ty);
                lx1 = std::max(lx1, tx);
                ly1 = std::max(ly1, ty);
            }
        }
        const Upload u = allocUpload(sizeof data, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (!u.cpu) return true;  // no upload memory this frame: the torches wait for the next
        std::memcpy(u.cpu, data, sizeof data);
        D3D12_TEXTURE_COPY_LOCATION src{}, to{};
        src.pResource = slots[slot].upload;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = u.offset;
        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        src.PlacedFootprint.Footprint.Width = kMaxLights * 2;
        src.PlacedFootprint.Footprint.Height = 1;
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = UINT(sizeof data);  // 2048: a multiple of 256
        to.pResource = torchData;
        to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        transition(torchData, torchDataState, D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
        transition(torchData, torchDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        c.footprint = float(lights.size());
        shaftLights = UINT(lights.size());  // 0.73: for the light in the air
        c.sourceRect[0] = effects.roundLight ? 1.0f : 0.0f;  // 0.84
        c.sharpen = effects.rimLight ? 0.6f * float(std::clamp(effects.rimStrength, 0, 200)) / 100.0f : 0.0f;  // 0.68
        const D3D12_RECT lit = clipped(lx0, ly0, lx1, ly1, w, h);
        if (lit.right > lit.left && lit.bottom > lit.top && ensureTorchLight(w, h)) {
            // 2.5: each light's quad adds into the buffer of light (torch_light_vs, torch_accum_ps), cleared first
            StageSetup linearClamp;
            linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
            linearClamp.magFilter = d3d7::TFG_LINEAR;
            linearClamp.minFilter = d3d7::TFN_LINEAR;
            linearClamp.mipFilter = d3d7::TFP_NONE;
            if (!makeRoom(linearClamp, linearClamp, 0, 5)) return false;
            placeSamples(false);
            const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp, 0);
            const D3D12_GPU_DESCRIPTOR_HANDLE textures = textureTable(sceneDepthSrv, torchDataSrv);
            transition(torchLight, torchLightState, D3D12_RESOURCE_STATE_RENDER_TARGET);
            const D3D12_CPU_DESCRIPTOR_HANDLE lightRtv = rtvHandle(kRtvTorch);
            const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            list->OMSetRenderTargets(1, &lightRtv, FALSE, nullptr);
            list->ClearRenderTargetView(lightRtv, none, 1, &lit);
            const D3D12_VIEWPORT vp{0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
            list->RSSetViewports(1, &vp);
            list->RSSetScissorRects(1, &lit);
            list->SetGraphicsRootSignature(root);
            list->SetPipelineState(torchAccumPipeline);
            c.targetSize[0] = float(w);
            c.targetSize[1] = float(h);
            list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
            list->SetGraphicsRootDescriptorTable(kRootTextures, textures);
            list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
            list->SetGraphicsRootDescriptorTable(kRootScene, sceneTable(sceneColorSrv, sceneDepthSrv, sceneColorSrv));
            list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            list->DrawInstanced(6, UINT(lights.size()), 0, 0);
            boundPipeline = nullptr;  // the next draws bind everything again
            boundTextures = boundSamplers = ~0ull;
            pass = kPassNone;
            transition(torchLight, torchLightState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            // 2.7.47: the picture before the night (t1, applyNight's) - a lit surface in its own colours, with the
            // look night_ps gave it; under the open sky at night no brighter than by day (twinH)
            Constants lc = c;
            for (int i = 0; i < 3; ++i) lc.fogColor[i] = dayKept ? dayTint[i] : 1.0f;
            lc.fogColor[3] = dayKept ? daySaturation : 0.0f;
            lc.sourceClamp[0] = dayKept ? dayFlash : 0.0f;
            lc.twin[3] = dayKept ? effect_light::dayBack(mapOutdoors, effects.nightNow) : 0.0f;
            if (!effectPass(torchLightPipelines[code], dst, dstState, rtv, w, h, lc, torchLightSrv, true,
                            dayKept ? dayColorSrv : sceneColorSrv, &lit))
                return false;
        }
    }
    if (heatWanted())  // 0.35: the hot air over each flame
        for (size_t i = 0; i < flames.size(); ++i) {
            const torch::Flame& f = flames[i];
            const float h = std::hypot(f.upX, f.upY) * k;
            if (h < 2.0f) continue;
            const float hx = (f.baseX + f.upX * 0.9f) * k + 0.5f + jx, hy = (f.baseY + f.upY * 0.9f) * k + 0.5f + jy;
            const float layers = i < gameFlames ? torch::kGameEffects : 1.0f;  // 2.6.18
            heatSpots.push_back(HeatSpot{hx, hy, h * 1.4f, 0.5f * f.alpha * layers * heatUser()});
        }
    for (size_t i = 0; i < flames.size(); ++i) {
        const torch::Flame& f = flames[i];
        const float bx = f.baseX * k + 0.5f + jx, by = f.baseY * k + 0.5f + jy;
        const float ux = f.upX * k, uy = f.upY * k;
        const float fh = std::hypot(ux, uy);
        if (fh < 2.0f) continue;  // too small to see
        const bool hidden = f.tint[0] < 0.99f || f.tint[1] < 0.99f || f.tint[2] < 0.99f;
        c.sourceRect[0] = bx;
        c.sourceRect[1] = by;
        c.sourceRect[2] = ux;
        c.sourceRect[3] = uy;
        c.sourceClamp[0] = f.halfWidth * k;
        c.sourceClamp[1] = 1.0f / f.rhw;
        c.sourceClamp[2] = i < gameFlames ? torch::kGameEffects : 1.0f;  // 2.6.18: drawn as the game's two were
        c.sourceClamp[3] = seeds[i];
        c.uiPixels[2] = flickers[i];
        c.uiPixels[3] = time;
        c.twin[0] = glowStrength;
        c.twin[1] = f.alpha;
        c.twin[2] = effects.torchFlame || hidden ? brightness : 0.0f;
        std::copy(f.tint, f.tint + 3, c.fogColor);
        const float mx = bx + ux * kMiddle, my = by + uy * kMiddle, r = fh * kGlowRadius;
        const D3D12_RECT around = clipped(mx - r, my - r, mx + r, my + r, w, h);
        if (around.right <= around.left || around.bottom <= around.top) continue;
        if (!effectPass(torchFirePipelines[upscale ? 4 : code], dst, dstState, rtv, w, h, c, sceneColorSrv, true,
                        kNoSrv, &around))
            return false;
    }
    for (const Fly& f : flies) {  // 0.28: each firefly's dot and halo (a pass each, cut to it)
        if (f.bright < 0.01f) continue;
        const float fx = f.gx * k + 0.5f + jx, fy = f.gy * k + 0.5f + jy, r = std::max(f.r * k, 0.6f);
        c.sourceRect[0] = fx;
        c.sourceRect[1] = fy;
        c.sourceRect[2] = r;
        c.sourceRect[3] = f.sharp;
        c.sourceClamp[1] = f.vz;
        c.twin[0] = f.bright;
        c.twin[1] = f.halo;
        std::copy(f.colour, f.colour + 3, c.fogColor);
        const D3D12_RECT around = clipped(fx - 8.0f * r, fy - 8.0f * r, fx + 8.0f * r, fy + 8.0f * r, w, h);
        if (around.right <= around.left || around.bottom <= around.top) continue;
        if (!effectPass(fireflyPipelines[upscale ? 4 : code], dst, dstState, rtv, w, h, c, sceneColorSrv, true, kNoSrv,
                        &around))
            return false;
    }
    if (!clouds.empty() && lightPipeline(dustPs, dustPipelines, "the towers' dust"))
        for (const Cloud& d : clouds) {  // 0.29: each cloud of dust (a pass each, cut to it)
            const float dx = d.gx * k + 0.5f + jx, dy = d.gy * k + 0.5f + jy, perUnit = d.perUnit * k;
            const float reach = d.radius * perUnit * 1.3f + 4.0f;
            c.sourceRect[0] = dx;
            c.sourceRect[1] = dy;
            c.sourceRect[2] = perUnit;
            c.sourceRect[3] = d.radius;
            c.sourceClamp[1] = d.vz;
            c.sourceClamp[3] = d.seed;
            c.uiPixels[3] = time;
            c.twin[0] = d.bright;
            std::copy(d.colour, d.colour + 3, c.fogColor);
            const D3D12_RECT around = clipped(dx - reach, dy - reach, dx + reach, dy + reach, w, h);
            if (around.right <= around.left || around.bottom <= around.top) continue;
            if (!effectPass(dustPipelines[upscale ? 4 : code], dst, dstState, rtv, w, h, c, sceneColorSrv, true, kNoSrv,
                            &around))
                return false;
        }
    if (!portalTracks.empty() && newPortalWanted()) return drawNewPortals(dst, dstState, rtv, code, w, h, time);
    return true;
}

// 2.7.48: a portal's foot - where the axis through its middle meets the map's ground (MapWorld's .gnd); without the
// map's file, or a ground far off its middle (another place's), 6 units under its middle.
void Renderer::Impl::portalFoot(const PortalTrack& t, const float up[3], float foot[3]) const {
    for (int i = 0; i < 3; ++i) foot[i] = t.view[i] - up[i] * 6.0f;
    if (!viewToWorldValid) return;
    const float* m = viewToWorld;
    const float wx = t.view[0] * m[0] + t.view[1] * m[3] + t.view[2] * m[6] + m[9];
    const float wz = t.view[0] * m[2] + t.view[1] * m[5] + t.view[2] * m[8] + m[11];
    float gy = 0.0f;
    if (!mapWorld.groundAt(wx, wz, gy)) return;
    const map::Point v = toView(viewToWorld, map::Point{wx, gy, wz});
    if (std::hypot(v.x - t.view[0], v.y - t.view[1], v.z - t.view[2]) > 30.0f) return;
    foot[0] = v.x;
    foot[1] = v.y;
    foot[2] = v.z;
}

// 2.7.48: the new portal (portal_ps; core/portal.hpp the game's warp zone) over each portal's place, cut to its box,
// from a copy of the picture as it is now - its lights and flames in it (the ground under the walls ripples).
bool Renderer::Impl::drawNewPortals(ID3D12Resource* dst, D3D12_RESOURCE_STATES& dstState,
                                    D3D12_CPU_DESCRIPTOR_HANDLE rtv, UINT code, int w, int h, float time) {
    if (!portalPipelines[code]) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, portalPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
        p.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&portalPipelines[code])))) {
            portalPipelines[code] = nullptr;
            LOG_ERROR("portals: the new portal's pipeline onto %u samples failed: the game's stays", 1u << code);
            releaseCom(portalPs);  // not tried again: newPortalWanted() is false from now on
            return true;
        }
    }
    if (!copySceneColor() || !copySceneDepth()) return false;
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    float up[3] = {0.0f, -1.0f, 0.0f};
    if (viewToWorldValid) camera::upInView(viewToWorld, up);
    const float k = float(upscale ? worldScale : scale);
    const float jx = upscale ? jitter[0] : 0.0f, jy = upscale ? jitter[1] : 0.0f;
    const float user = float(std::clamp(effects.portalsStrength, 0, 200)) / 100.0f;
    const std::array<portal::Ring, portal::kRings> rings = portal::ringsAt(double(time));
    Constants c{};
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = k;
    c.jitter[0] = jx;
    c.jitter[1] = jy;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    const float twin[4] = {rings[0].radius, rings[0].alpha, rings[1].radius, rings[1].alpha};
    std::copy(twin, twin + 4, c.twin);
    c.sourceClamp[0] = rings[2].radius;
    c.sourceClamp[1] = rings[2].alpha;
    c.sourceClamp[2] = time;
    std::copy(up, up + 3, c.fogColor);
    c.fogColor[3] = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    int index = 0;
    for (const PortalTrack& t : portalTracks) {
        const float shown = t.strength * t.strength * (3.0f - 2.0f * t.strength);
        if (shown <= 0.0f) continue;
        float foot[3], box[4];
        portalFoot(t, up, foot);
        if (!portal::screenBox(foot, up, proj, 45.0f, box)) continue;
        const D3D12_RECT around = clipped(box[0] * k + jx, box[1] * k + jy, box[2] * k + jx, box[3] * k + jy, w, h);
        if (around.right <= around.left || around.bottom <= around.top) continue;
        std::copy(foot, foot + 3, c.sourceRect);
        c.sourceRect[3] = shown * user;
        c.footprint = float(++index) * 0.618f;  // its own sparks
        c.uiPixels[2] = (foot[0] / foot[2] * proj.f0 + proj.cx) * k + 0.5f + jx;
        c.uiPixels[3] = (foot[1] / foot[2] * proj.f1 + proj.cy) * k + 0.5f + jy;
        if (!effectPass(portalPipelines[code], dst, dstState, rtv, w, h, c, sceneColorSrv, true, kNoSrv, &around))
            return false;
    }
    return true;
}

}  // namespace uagfx::gpu
