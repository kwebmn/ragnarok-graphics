#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/game_camera.hpp"
#include "core/living.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 1.7: a tree's leaves drawn (draw.cpp): its crown in the world, for the leaves falling from it (leaf_fall).
void Renderer::Impl::noteCrown(const FrameRecord& rec, const DrawCmd& d) {
    if (!viewToWorldValid || frameCrowns.size() >= 32) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f, rhw = 0.0f;
    uint32_t n = 0;
    for (uint32_t i = 0; i < d.vertexCount && d.vertexOffset + (i + 1) * stride <= v.size(); ++i, ++n) {
        float xyzw[4];
        std::memcpy(xyzw, v.data() + d.vertexOffset + i * stride, sizeof xyzw);
        x0 = std::min(x0, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        x1 = std::max(x1, xyzw[0]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3];
    }
    if (!n || rhw <= 0.0f || x1 - x0 < 8.0f) return;
    rhw /= float(n);
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const float vz = 1.0f / rhw, vx = ((x0 + x1) * 0.5f - p.cx) * vz / p.f0, vy = ((y0 + y1) * 0.5f - p.cy) * vz / p.f1;
    critters::Crown c;
    c.centre = critters::Point{vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                               vx * m[2] + vy * m[5] + vz * m[8] + m[11]};
    c.radius = std::clamp((x1 - x0) * 0.5f * vz / p.f0, 1.0f, 12.0f);
    frameCrowns.push_back(c);
}

bool Renderer::Impl::crittersWanted() const {
    const critters::Switches on = critterSwitches();
    return crittersPs && on.any() && viewToWorldValid;
}

critters::Switches Renderer::Impl::critterSwitches() const {
    critters::Switches on;
    on.butterflies = effects.butterflies;
    on.dragonflies = effects.butterflies && effects.dragonflies;  // 2.4
    on.birds = effects.birds;
    on.leaves = effects.leafFall;
    on.embers = effects.embers;
    on.breath = effects.breath;
    on.splashes = effects.splashes;
    on.drips = effects.caveDrips;
    on.underwater = effects.underwater;
    on.nightFireflies = effects.nightFireflies;  // 2.6.11
    return on;
}

// 1.7: the living world's creatures and particles (core/critters.hpp) - moved on, turned to the screen and drawn
// over the world by critters_ps, after the torches (the embers shine out of the night).
bool Renderer::Impl::applyCritters() {
    crittersDone = true;
    crumb("living creatures");
    const float now = waterTime();
    critters::Frame f;
    f.now = double(now);
    f.dt = crittersTime > 0.0f ? std::clamp(now - crittersTime, 0.0f, 0.1f) : 0.0f;
    crittersTime = now;
    const float* m = viewToWorld;
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float tz = cameraTargetZ;
    if (cameraValid && tz > 0.0f) {  // the point the camera looks at: the player
        f.centre = critters::Point{tz * m[6] + m[9], tz * m[7] + m[10], tz * m[8] + m[11]};
        f.centreKnown = true;
    } else {  // else the feet nearest the middle of the screen (the view's z axis)
        float nearest = 1e30f;
        for (const map::Point& p : frameFeet) {
            const float x = p.x - m[9], y = p.y - m[10], z = p.z - m[11];
            const float along = x * m[6] + y * m[7] + z * m[8];
            const float off = x * x + y * y + z * z - along * along;
            if (along > 0.0f && off < nearest) {
                nearest = off;
                f.centre = critters::Point{p.x, p.y, p.z};
                f.centreKnown = true;
            }
        }
    }
    if (!f.centreKnown) return true;
    float best = 1e30f;
    for (const map::Point& p : frameFeet) {  // the player's feet: the ground's height there
        f.feet.push_back(critters::Point{p.x, p.y, p.z});
        const float d = std::hypot(p.x - f.centre.x, p.z - f.centre.z);
        if (d < best && d < 6.0f) {
            best = d;
            f.centre.y = p.y;
        }
    }
    for (const footsteps::Step& s : footTracker.steps())
        if (s.born > crittersStepsSeen) f.steps.push_back(critters::Point{s.x, s.y, s.z});
    for (const footsteps::Step& s : footTracker.steps()) crittersStepsSeen = std::max(crittersStepsSeen, s.born);
    for (const torch::Flame& fl : critterFlames) {  // the flames' tips (2.5.4: as the torches' pass had them)
        critters::Point at;
        if (f.fires.size() < 32 && critters::fireOf(fl, proj, m, at)) f.fires.push_back(at);
    }
    f.crowns = frameCrowns;
    frameCrowns.clear();
    f.waterKnown = waterLevelKnown;
    f.waterLevel = waterLevel;
    f.day = living::daytime(minuteNow());
    f.night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    critterWorld.update(critterSwitches(), f);
    const std::vector<critters::Sprite>& all = critterWorld.sprites();
    const bool shafts = effects.underwater && mapPlace.underwater;
    if (all.empty() && !shafts) return true;
    if (!ensureScene("living creatures")) return true;
    ID3D12PipelineState* pso = crittersPipeline(upscale ? 1 : samples);
    if (!pso) return true;
    if (!crittersData) {
        crittersData = createTexture2D(device, int(critters::kMaxSprites * 4), 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                       D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
        crittersDataSrv = crittersData ? allocSrv() : kNoSrv;
        if (!crittersData || crittersDataSrv == kNoSrv) {
            LOG_ERROR("living creatures: no memory for their list: none");
            releaseCom(crittersData);
            releaseCom(crittersPs);
            return true;
        }
        crittersDataState = D3D12_RESOURCE_STATE_COPY_DEST;
        device->CreateShaderResourceView(crittersData, nullptr, cpuSrvHandle(crittersDataSrv));
    }
    // The world back into the view: the inverse of viewToWorld's 3 x 3 (its rows the view's axes in the world).
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], h = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * h) + c3 * (d * i - e * h);
    if (std::fabs(det) < 1e-8f) return true;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * h - d * k) / det, (a * k - c3 * h) / det, (c3 * d - a * g) / det,
                          (d * i - e * h) / det, (b * h - a * i) / det, (a * e - b * d) / det};
    const float k0 = float(upscale ? worldScale : scale);
    const float jx = upscale ? jitter[0] : 0.0f, jy = upscale ? jitter[1] : 0.0f;
    // w = v.x * row0 + v.y * row1 + v.z * row2 + t, so v = (w - t) * inverse (row vector times the inverse).
    const auto toView = [&](const critters::Point& w, float out[3]) {
        const float x = w.x - m[9], y = w.y - m[10], z = w.z - m[11];
        out[0] = x * inv[0] + y * inv[3] + z * inv[6];
        out[1] = x * inv[1] + y * inv[4] + z * inv[7];
        out[2] = x * inv[2] + y * inv[5] + z * inv[8];
    };
    const auto toScreen = [&](const float v[3], float& sx, float& sy) {
        sx = (v[0] / v[2] * proj.f0 + proj.cx) * k0 + 0.5f + jx;
        sy = (v[1] / v[2] * proj.f1 + proj.cy) * k0 + 0.5f + jy;
    };
    const float squash = std::clamp(std::fabs(m[7]), 0.2f, 1.0f);  // a circle on the ground seen from the camera
    const int w = sceneWidth, ht = sceneHeight;
    static float data[critters::kMaxSprites * 4][4];
    std::memset(data, 0, sizeof data);
    size_t n = 0;
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (const critters::Sprite& s : all) {
        if (n >= critters::kMaxSprites) break;
        float v[3];
        toView(s.at, v);
        if (v[2] < proj.nearZ + 1.0f) continue;
        float sx, sy;
        toScreen(v, sx, sy);
        const float size = s.size * proj.f0 * k0 / v[2];
        if (sx < -4.0f * size || sy < -4.0f * size || sx > float(w) + 4.0f * size || sy > float(ht) + 4.0f * size)
            continue;
        float* t0 = data[4 * n];
        t0[0] = sx;
        t0[1] = sy;
        t0[2] = std::max(size, 1.0f);
        t0[3] = v[2];
        float* t1 = data[4 * n + 1];
        t1[0] = float(s.kind);
        t1[1] = s.p1;
        t1[2] = s.p2;
        t1[3] = s.alpha;
        float* t2 = data[4 * n + 2];
        if (s.headingX != 0.0f || s.headingZ != 0.0f) {  // the heading turned to the screen
            float tip[3];
            toView(critters::Point{s.at.x + s.headingX, s.at.y, s.at.z + s.headingZ}, tip);
            if (tip[2] > proj.nearZ) {
                float tx, ty;
                toScreen(tip, tx, ty);
                t2[0] = std::atan2(ty - sy, tx - sx);
            }
        }
        t2[1] = squash;
        if (s.kind == critters::Dragonfly && waterLevelKnown) {  // over the water only: its point on the surface
            float q[3];
            toView(critters::Point{s.at.x, waterLevel, s.at.z}, q);
            if (q[2] > proj.nearZ) {
                float* t3 = data[4 * n + 3];
                toScreen(q, t3[0], t3[1]);
                t3[2] = q[2];
                t3[3] = 1.0f;
            }
        }
        x0 = std::min(x0, sx - 3.0f * size);
        y0 = std::min(y0, sy - 3.0f * size);
        x1 = std::max(x1, sx + 3.0f * size);
        y1 = std::max(y1, sy + 3.0f * size);
        ++n;
    }
    if (shafts && n < critters::kMaxSprites) {  // the light from above under the sea, over the whole picture
        data[4 * n][2] = 1.0f;
        data[4 * n + 1][0] = 13.0f;
        data[4 * n + 1][3] = 1.0f;
        ++n;
    }
    if (!n) return true;
    const Upload u = allocUpload(sizeof data, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!u.cpu) return true;
    std::memcpy(u.cpu, data, sizeof data);
    D3D12_TEXTURE_COPY_LOCATION src{}, to{};
    src.pResource = slots[slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = u.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    src.PlacedFootprint.Footprint.Width = UINT(critters::kMaxSprites * 4);
    src.PlacedFootprint.Footprint.Height = 1;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = UINT(sizeof data);  // 12288: a multiple of 256
    to.pResource = crittersData;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    if (!copySceneDepth()) return false;
    transition(crittersData, crittersDataState, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
    transition(crittersData, crittersDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Constants c{};
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = k0;
    c.jitter[0] = jx;
    c.jitter[1] = jy;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(ht);
    c.sharpen = now;
    c.twin[2] = 1.0f - 0.65f * f.night;  // the creatures' light
    if (!crittersLogged) LOG_INFO("living creatures: on %s", mapName.c_str());
    crittersLogged = true;
    D3D12_RECT clip{0, 0, w, ht};
    if (!shafts) {
        clip.left = LONG(std::clamp(x0, 0.0f, float(w)));
        clip.top = LONG(std::clamp(y0, 0.0f, float(ht)));
        clip.right = LONG(std::clamp(x1 + 1.0f, 0.0f, float(w)));
        clip.bottom = LONG(std::clamp(y1 + 1.0f, 0.0f, float(ht)));
        if (clip.right <= clip.left || clip.bottom <= clip.top) return true;
    }
    const UINT vertices = UINT(6 * n);
    if (upscale)
        return effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, ht, c, sceneColorSrv, true, crittersDataSrv,
                          &clip, vertices);
    return effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, ht, c, sceneColorSrv, true, crittersDataSrv,
                      &clip, vertices);
}

// Premultiplied over the world, the quads critters_vs makes (onto the target, B8G8R8A8, its samples, or the upscaled
// world).
ID3D12PipelineState* Renderer::Impl::crittersPipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (crittersPipelines[code] || !crittersPs || !crittersVs) return crittersPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, crittersVs, crittersPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = D3D12_BLEND_ONE;
    b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&crittersPipelines[code])))) {
        LOG_ERROR("living creatures: the pipeline onto %u samples failed: none", 1u << code);
        crittersPipelines[code] = nullptr;
        releaseCom(crittersPs);  // not tried again
    }
    return crittersPipelines[code];
}

}  // namespace uagfx::gpu
