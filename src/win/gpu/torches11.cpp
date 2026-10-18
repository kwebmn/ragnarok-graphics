#include <windows.h>

#include <d3d11.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "core/d3d7.hpp"
#include "core/fireflies.hpp"
#include "core/game_camera.hpp"
#include "core/torch.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/gpu/shaders.hpp"
#include "win/log.hpp"

// 0.61: the torches' light with Direct3D 11 - as Direct3D 12's torches.cpp draws it (torch_light_ps): each flame the
// game draws lights the ground about it, warm and flickering a little, before the picture's colours. The flame and
// its glow stay the game's (Direct3D 12 draws them anew); no map's lamps or hidden torches (they need the camera).
namespace uagfx::gpu {
namespace {

constexpr int kMaxLights = 64;  // as Direct3D 12's
constexpr float kReach = 45.0f;
constexpr float kLightColor[3] = {1.0f, 0.58f, 0.28f};
constexpr float kMiddle = 0.38f;

// 0.25 (torches.cpp): the map file's torches the game does not show (it knows effect ids up to 629): their colours.
bool hiddenTorch(int32_t id, float tint[3]) {
    static const float kRed[3] = {1.0f, 0.32f, 0.26f}, kGreen[3] = {0.42f, 1.0f, 0.38f};
    static const float kPurple[3] = {0.78f, 0.4f, 1.0f};
    const float* c = id == 690 ? kRed : id == 691 ? kGreen : id == 696 ? kPurple : nullptr;
    if (!c) return false;
    std::copy(c, c + 3, tint);
    return true;
}

}  // namespace

// 2.5.1: as torches.cpp noteFire - the draw's middle and depth from its vertices, how much it lights: the picture's
// fire (or its own light), the game's fade and colour of it, its size on the screen.
void Renderer11::noteFire(const FrameRecord& rec, const DrawCmd& d, float amount, const float colour[3]) {
    if (fireSparks_.size() >= 512) return;
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
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    FireSpark11 s;
    s.gameX = (x0 + x1) * 0.5f;
    s.gameY = (y0 + y1) * 0.5f;
    const float vz = 1.0f / rhw;
    s.view[0] = (s.gameX - p.cx) * vz / p.f0;
    s.view[1] = (s.gameY - p.cy) * vz / p.f1;
    s.view[2] = vz;
    const float size = std::sqrt(std::max(0.0f, (x1 - x0) * (y1 - y0)));  // game pixels
    s.strength = amount * shown * std::clamp(size / 120.0f, 0.15f, 1.5f);
    std::copy(colour, colour + 3, s.colour);
    fireSparks_.push_back(s);
}

// 1.0.1: one of the game's fireflies (effect 45, known by the driver): its middle, size, fade and tint.
bool Renderer11::takeFirefly(const FrameRecord& rec, const DrawCmd& d) {
    if (flies_.size() >= 128) return false;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const uint32_t n = std::min<uint32_t>(d.vertexCount, 8);
    if (n < 3 || d.vertexOffset + size_t(n) * stride > v.size() || !depthViews()) return false;
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
    if (rhw <= 0.0f) return false;
    Fly11 f{};
    f.x = (x0 + x1) * 0.5f;
    f.y = (y0 + y1) * 0.5f;
    f.rhw = rhw;
    f.size = std::sqrt(std::max(0.0f, (x1 - x0) * (y1 - y0)));
    f.alpha = float(diffuse >> 24) / 255.0f;
    fireflies::colourOf(diffuse, f.colour);
    f.seed = float(d.effectSeed) / 255.0f;
    flies_.push_back(f);
    static bool logged = false;
    if (!logged) LOG_INFO("fireflies: the game's fireflies drawn anew (Direct3D 11)");
    logged = true;
    return true;
}

// Each firefly a light added within a box of eight of its radii (the scissor), hidden behind what stands in front of
// it by the depth.
void Renderer11::applyFireflies() {
    flushBatch();
    std::vector<Fly11> flies;
    flies.swap(flies_);
    if (!firefliesWanted() || !depthViews() || !target_.rtv) return;
    targetBound_ = false;
    ID3D11ShaderResourceView* none[4] = {nullptr, nullptr, nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 4, none);
    ctx_->OMSetRenderTargets(1, &target_.rtv, nullptr);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(target_.width), float(target_.height), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    ctx_->OMSetBlendState(blendState(true, D3D11_BLEND_ONE, D3D11_BLEND_ONE,
                                     D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
                                         D3D11_COLOR_WRITE_ENABLE_BLUE),
                          nullptr, 0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(presentVs_, nullptr, 0);
    ctx_->PSSetShader(fireflyPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetShaderResources(3, 1, &depthSrv_);  // sceneDepth : register(t3)
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float user = float(std::clamp(effects_.firefliesBrightness, 0, 200)) / 100.0f;
    const double time = double(GetTickCount64()) / 1000.0;
    const float k = float(scale_);
    for (const Fly11& f : flies) {
        Constants c{};
        c.targetSize[0] = float(target_.width);
        c.targetSize[1] = float(target_.height);
        const float proj6[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
        std::memcpy(c.projection, proj6, sizeof proj6);
        const float r = std::max(f.size * 0.06f, 0.4f) * k;
        c.sourceRect[0] = f.x * k;
        c.sourceRect[1] = f.y * k;
        c.sourceRect[2] = r;
        c.sourceRect[3] = 1.2f;
        c.sourceClamp[1] = 1.0f / f.rhw;
        c.fogColor[0] = f.colour[0];
        c.fogColor[1] = f.colour[1];
        c.fogColor[2] = f.colour[2];
        c.twin[0] = user * f.alpha * fireflies::blink(time, f.seed);
        c.twin[1] = 1.0f;
        if (c.twin[0] <= 0.001f) continue;
        const LONG reach = LONG(std::ceil(r * 8.0f)) + 1;
        const D3D11_RECT box{std::max<LONG>(0, LONG(c.sourceRect[0]) - reach),
                             std::max<LONG>(0, LONG(c.sourceRect[1]) - reach),
                             std::min<LONG>(target_.width, LONG(c.sourceRect[0]) + reach),
                             std::min<LONG>(target_.height, LONG(c.sourceRect[1]) + reach)};
        if (box.left >= box.right || box.top >= box.bottom) continue;
        ctx_->RSSetScissorRects(1, &box);
        setConstants(c);
        ctx_->Draw(3, 0);
    }
    ctx_->PSSetShaderResources(3, 1, none);
    const D3D11_RECT all{0, 0, target_.width, target_.height};
    ctx_->RSSetScissorRects(1, &all);
}

void Renderer11::noteTorch(const FrameRecord& rec, const DrawCmd& d) {
    if (objectTextures_.of(d.texture[0]) != objects::Kind::TorchFlame || flames_.size() >= size_t(kMaxLights)) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > v.size()) return;
    torch::Corner q[4];
    uint32_t diffuse = 0;
    for (int i = 0; i < 4; ++i) {
        const uint8_t* p = v.data() + d.vertexOffset + size_t(i) * stride;
        float xyzw[4], uv[2];
        std::memcpy(xyzw, p, sizeof xyzw);
        std::memcpy(uv, p + 24, sizeof uv);
        q[i] = torch::Corner{xyzw[0], xyzw[1], xyzw[2], xyzw[3], uv[0], uv[1]};
        if (i == 0) std::memcpy(&diffuse, p + 16, sizeof diffuse);
    }
    torch::Flame f;
    if (!torch::flameOf(q, f) || f.rhw <= 0.0f) return;
    f.alpha = std::min(1.0f, float(diffuse >> 24) / 240.0f);
    flames_.push_back(f);
    if (cameraValid_ && !mapWorld_.axes().known()) {  // 2.5.1: where it stands, for the map file's axes (torches.cpp)
        const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
        const float vz = 1.0f / f.rhw, vx = (f.baseX - p.cx) * vz / p.f0, vy = (f.baseY - p.cy) * vz / p.f1;
        const float* m = viewToWorld_;
        mapWorld_.noteTorch(map::Point{vx * m[0] + vy * m[3] + vz * m[6] + m[9],
                                       vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                                       vx * m[2] + vy * m[5] + vz * m[8] + m[11]});
    }
}

void Renderer11::applyTorchLight() {
    EffectSpan span(*this, kTimeLight11);  // 0.89
    lightDone_ = true;
    // 2.5.1: the map's lamps light too (torches.cpp), with the camera and the map file's axes known
    const bool torches = torchesWanted() && !flames_.empty();
    const bool placed = cameraValid_ && mapWorld_.axes().known();  // the map file's places known in the world
    const bool lampsOn = effects_.lamps && effects_.lampsLight > 0 && torchLightPs_ && placed &&
                         !mapWorld_.lamps().empty();
    const bool hidden = torchesWanted() && placed && mapWorld_.world();  // the torches the game does not show
    const bool fire = fireWanted() && !fireSparks_.empty();  // 2.5.1: the skills' fire
    if ((!torches && !lampsOn && !hidden && !fire) || !depth_ || !target_.texture) {
        fireSparks_.clear();
        return;
    }
    if (!lightData_) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = kMaxLights * 2;
        d.Height = 1;
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, &lightData_)) ||
            FAILED(device_->CreateShaderResourceView(lightData_, nullptr, &lightDataSrv_))) {
            if (lightData_) lightData_->Release();
            lightData_ = nullptr;
            torchLightPs_->Release();  // not tried again
            torchLightPs_ = nullptr;
            return;
        }
    }
    if (depthSrvOf_ != depth_) {  // as the shadows read it (actor_shadows11.cpp)
        if (depthSrv_) depthSrv_->Release();
        depthSrv_ = nullptr;
        D3D11_SHADER_RESOURCE_VIEW_DESC v{};
        v.Format = DXGI_FORMAT_R32_FLOAT;
        v.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        v.Texture2D.MipLevels = 1;
        depthSrvOf_ = depth_;
        if (FAILED(device_->CreateShaderResourceView(depth_, &v, &depthSrv_))) depthSrv_ = nullptr;
    }
    if (!depthSrv_) return;
    const unsigned long long now = GetTickCount64();
    if (!clockStart_) clockStart_ = now;
    const double time = double(now - clockStart_) / 1000.0;
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float amount = 0.35f * float(std::clamp(effects_.torchFlicker, 0, 100)) / 100.0f;
    const float strength = 0.4f * float(std::clamp(effects_.torchLight, 0, 200)) / 100.0f;
    const float k = float(scale_);
    const int w = target_.width, h = target_.height;
    float data[kMaxLights * 2][4] = {};
    float lx0 = float(w), ly0 = float(h), lx1 = 0.0f, ly1 = 0.0f;
    int count = 0;
    // A light into the list: its place in the view and reach, its colour and strength; where its reach falls on
    // the screen (the cube about it, each corner projected) widens the lit rectangle.
    const auto addLight = [&](const float view[3], float reach, const float colour[3], float amountOf) {
        std::copy(view, view + 3, data[2 * count]);
        data[2 * count][3] = reach;
        std::copy(colour, colour + 3, data[2 * count + 1]);
        data[2 * count + 1][3] = amountOf;
        for (int corner = 0; corner < 8; ++corner) {
            const float px = view[0] + ((corner & 1) ? reach : -reach);
            const float py = view[1] + ((corner & 2) ? reach : -reach);
            const float pz = std::max(view[2] + ((corner & 4) ? reach : -reach), proj.nearZ);
            const float tx = (px / pz * proj.f0 + proj.cx) * k + 0.5f, ty = (py / pz * proj.f1 + proj.cy) * k + 0.5f;
            lx0 = std::min(lx0, tx);
            ly0 = std::min(ly0, ty);
            lx1 = std::max(lx1, tx);
            ly1 = std::max(ly1, ty);
        }
        ++count;
    };
    if (torches)
        for (const torch::Flame& f : flames_) {
            if (count == kMaxLights) break;
            const float mx = f.baseX + f.upX * kMiddle, my = f.baseY + f.upY * kMiddle;
            const float vz = 1.0f / f.rhw;
            const float vx = (mx - proj.cx) * vz / proj.f0, vy = (my - proj.cy) * vz / proj.f1;
            const uint32_t seed = torch::seedAt(mx, my);  // no camera: by its place on the screen
            const float calm = torch::flicker(time * 0.5, seed ^ 0x5BD1E995u, amount * 0.35f);
            const float view[3] = {vx, vy, vz};
            addLight(view, kReach, kLightColor, strength * calm * f.alpha);
        }
    if (hidden) {  // 2.5.1: the map file's torches the game does not show - their light (the flame stays unseen)
        const map::Axes axes = mapWorld_.axes();
        const float* m = viewToWorld_;
        for (const rsw::Effect& e : mapWorld_.world()->effects) {
            float tint[3];
            if (count == kMaxLights || !hiddenTorch(e.id, tint)) continue;
            // the flame's middle: its base two units down the file's place, the light kMiddle of its 13 units up
            const map::Point wp = map::toWorld(axes, map::Point{e.pos[0], e.pos[1] - 2.0f - 13.0f * kMiddle, e.pos[2]});
            const float dx = wp.x - m[9], dy = wp.y - m[10], dz = wp.z - m[11];
            const float v[3] = {dx * m[0] + dy * m[1] + dz * m[2], dx * m[3] + dy * m[4] + dz * m[5],
                                dx * m[6] + dy * m[7] + dz * m[8]};
            if (v[2] <= proj.nearZ) continue;
            const float gx = v[0] / v[2] * proj.f0 + proj.cx, gy = v[1] / v[2] * proj.f1 + proj.cy;
            const float slack = float(std::max(gameWidth_, gameHeight_)) * 0.25f;
            if (gx < -slack || gy < -slack || gx > float(gameWidth_) + slack || gy > float(gameHeight_) + slack)
                continue;
            const float calm = torch::flicker(time * 0.5, torch::seedAt(wp.x, wp.z) ^ 0x5BD1E995u, amount * 0.35f);
            addLight(v, kReach, tint, strength * calm);
        }
    }
    if (fire) {  // 2.5.1: the fire of skills, gathered by place (cells of 80 game pixels), the brightest 16
        struct Cell {
            float w = 0, view[3] = {0, 0, 0}, colour[3] = {0, 0, 0};
        };
        std::unordered_map<int, Cell> cells;
        for (const FireSpark11& f : fireSparks_) {
            Cell& c = cells[int(std::floor(f.gameX / 80.0f)) * 4096 + int(std::floor(f.gameY / 80.0f))];
            c.w += f.strength;
            for (int i = 0; i < 3; ++i) {
                c.view[i] += f.view[i] * f.strength;
                c.colour[i] += f.colour[i] * f.strength;
            }
        }
        std::vector<Cell> best;
        for (const auto& kv : cells)
            if (kv.second.w > 0.02f) best.push_back(kv.second);
        std::sort(best.begin(), best.end(), [](const Cell& a, const Cell& b) { return a.w > b.w; });
        const float fireStrength = 0.18f * float(std::clamp(effects_.fireLightStrength, 0, 200)) / 100.0f;
        for (size_t i = 0; i < best.size() && i < 16 && count < kMaxLights; ++i) {
            const Cell& c = best[i];
            const float top = std::max({c.colour[0], c.colour[1], c.colour[2], 1e-4f});
            const float view[3] = {c.view[0] / c.w, c.view[1] / c.w, c.view[2] / c.w};
            const float colour[3] = {c.colour[0] / top, c.colour[1] / top, c.colour[2] / top};
            addLight(view, 25.0f + 15.0f * std::min(c.w, 1.0f), colour, fireStrength * std::min(c.w, 1.0f));
        }
    }
    fireSparks_.clear();
    if (lampsOn) {  // 2.5.1: the lamps the map's makers placed, the nearest to the camera first (as torches.cpp)
        const map::Axes axes = mapWorld_.axes();
        const float lampStrength = 0.8f * float(std::clamp(effects_.lampsLight, 0, 200)) / 100.0f;
        const float margin = float(std::max(gameWidth_, gameHeight_)) * 0.25f;
        struct Lamp {
            float view[3], tint[3], strength, reach;
        };
        std::vector<Lamp> lamps;
        const float* m = viewToWorld_;  // the camera backwards, inverted: its rows are orthonormal
        for (const rsw::Light& r : mapWorld_.lamps()) {
            const map::Point wp = map::toWorld(axes, map::Point{r.pos[0], r.pos[1], r.pos[2]});
            const float dx = wp.x - m[9], dy = wp.y - m[10], dz = wp.z - m[11];
            const float v[3] = {dx * m[0] + dy * m[1] + dz * m[2], dx * m[3] + dy * m[4] + dz * m[5],
                                dx * m[6] + dy * m[7] + dz * m[8]};
            const float reach = std::clamp(r.range * 0.45f, 15.0f, 70.0f);
            if (v[2] <= proj.nearZ - reach) continue;
            const float vz = std::max(v[2], proj.nearZ);
            const float gx = v[0] / vz * proj.f0 + proj.cx, gy = v[1] / vz * proj.f1 + proj.cy;
            const float slack = reach * proj.f0 / vz + margin;
            if (gx < -slack || gy < -slack || gx > float(gameWidth_) + slack || gy > float(gameHeight_) + slack)
                continue;
            const float top = std::max({r.color[0], r.color[1], r.color[2], 1e-3f});
            Lamp l{{v[0], v[1], v[2]}, {0.0f, 0.0f, 0.0f}, lampStrength, reach};
            for (int c = 0; c < 3; ++c) l.tint[c] = 0.65f + 0.35f * r.color[c] / top;  // 2.1.5: a third of it
            if (r.color[0] >= r.color[1] && r.color[1] >= r.color[2] * 0.9f && r.color[0] > 0.5f)  // a fire's
                l.strength *= torch::flicker(time * 0.5, torch::seedAt(wp.x, wp.z), amount * 0.25f);
            lamps.push_back(l);
        }
        std::sort(lamps.begin(), lamps.end(), [](const Lamp& a, const Lamp& b) { return a.view[2] < b.view[2]; });
        for (const Lamp& l : lamps) {
            if (count == kMaxLights) break;
            addLight(l.view, l.reach, l.tint, l.strength);
        }
    }
    const D3D11_RECT lit{LONG(std::max(0.0f, std::floor(lx0))), LONG(std::max(0.0f, std::floor(ly0))),
                         LONG(std::min(float(w), std::ceil(lx1))), LONG(std::min(float(h), std::ceil(ly1)))};
    if (!count || lit.right <= lit.left || lit.bottom <= lit.top) return;
    ctx_->UpdateSubresource(lightData_, 0, nullptr, data, UINT(sizeof data), 0);
    // The picture's copy (scene_, as the colours make it) read, the lit pixels written.
    if (scene_.width != w || scene_.height != h) {
        dropTarget(scene_);
        if (!makeTarget(scene_, w, h)) return;
    }
    ctx_->CopyResource(scene_.texture, target_.texture);
    targetBound_ = false;
    ID3D11ShaderResourceView* none[4] = {nullptr, nullptr, nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 4, none);
    ctx_->OMSetRenderTargets(1, &target_.rtv, nullptr);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    ctx_->RSSetScissorRects(1, &lit);
    Constants c{};
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = k;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.footprint = float(count);
    c.sourceRect[0] = effects_.roundLight ? 1.0f : 0.0f;  // 0.84
    c.sharpen = effects_.rimLight ? 0.6f * float(std::clamp(effects_.rimStrength, 0, 200)) / 100.0f : 0.0f;  // 0.68
    setConstants(c);
    ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO,
                                     D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
                                         D3D11_COLOR_WRITE_ENABLE_BLUE),
                          nullptr, 0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(presentVs_, nullptr, 0);
    ctx_->PSSetShader(torchLightPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    ID3D11ShaderResourceView* views[4] = {nullptr, lightDataSrv_, scene_.srv, depthSrv_};  // t1, t2, t3
    ctx_->PSSetShaderResources(0, 4, views);
    ctx_->Draw(3, 0);
    ctx_->PSSetShaderResources(0, 4, none);
    if (!lightLogged_)
        LOG_INFO("torches: %d lit (Direct3D 11: their light%s%s; the flames the game's)", count,
                 lampsOn ? " and the map's lamps" : "", fire ? ", the skills' fire" : "");
    lightLogged_ = true;
}

}  // namespace uagfx::gpu
