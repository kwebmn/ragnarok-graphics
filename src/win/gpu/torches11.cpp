#include <windows.h>

#include <d3d11.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "core/d3d7.hpp"
#include "core/effect_light.hpp"
#include "core/portal.hpp"
#include "core/fireflies.hpp"
#include "core/game_camera.hpp"
#include "core/map_glows.hpp"
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
constexpr float kGlowRadius = 2.2f;  // 2.5.2: the flame's glow, flame heights around its middle (torches.cpp)
constexpr size_t kMaxTorches = 32;   // flames drawn anew in a frame (impl.hpp)

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
void Renderer11::noteFire(const FrameRecord& rec, const DrawCmd& d, float amount, const float colour[3], bool orb) {
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
    s.orb = orb;
    fireSparks_.push_back(s);
}

// 1.0.1: one of the game's fireflies (effect 45, known by the driver): its middle, size, fade and tint.
bool Renderer11::takeFirefly(const FrameRecord& rec, const DrawCmd& d) {
    if (!effects_.fireflyDots) return true;  // 3.0.7-beta.3 (as torches.cpp): their light alone, from their places
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
    gradeNow();  // 3.0.5-beta.4: the colours put off meet the world before the fireflies, as before
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

bool Renderer11::noteTorch(const FrameRecord& rec, const DrawCmd& d) {
    if (objectTextures_.of(d.texture[0]) != objects::Kind::TorchFlame) return true;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    torch::Flame f;
    if (d.vertexOffset + 4 * stride > v.size() || !torch::flameOfQuad(v.data() + d.vertexOffset, stride, f))
        return true;
    if (!torch::take(flames_, f, kMaxTorches)) return false;
    if (cameraValid_ && !mapWorld_.axes().known()) {  // 2.5.1: where it stands, for the map file's axes (torches.cpp)
        const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
        const float vz = 1.0f / f.rhw, vx = (f.baseX - p.cx) * vz / p.f0, vy = (f.baseY - p.cy) * vz / p.f1;
        const float* m = viewToWorld_;
        mapWorld_.noteTorch(map::Point{vx * m[0] + vy * m[3] + vz * m[6] + m[9],
                                       vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                                       vx * m[2] + vy * m[5] + vz * m[8] + m[11]});
    }
    return true;
}

void Renderer11::noteCritterFlame(const FrameRecord& rec, const DrawCmd& d) {
    if (objectTextures_.of(d.texture[0]) != objects::Kind::TorchFlame) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    torch::Flame f;
    if (d.vertexOffset + 4 * stride <= v.size() && torch::flameOfQuad(v.data() + d.vertexOffset, stride, f))
        torch::take(critterFlames_, f, kMaxTorches);  // 2.6.18: a torch's two effects one fire
}

bool Renderer11::takeTorch(const FrameRecord& rec, const DrawCmd& d) {
    const objects::Kind kind = objectTextures_.of(d.texture[0]);
    if (kind != objects::Kind::TorchGlow && kind != objects::Kind::TorchFlame) return false;
    if (kind == objects::Kind::TorchGlow) return flamesWanted();  // the new flame's pass draws a glow of its own
    // 2.6.18: no room for another torch - the game's own flame; torch_flame off - the game's own, the new glow and
    // light around it
    const bool noted = !(torchesWanted() || flamesWanted()) || noteTorch(rec, d);
    return flamesWanted() && effects_.torchFlame && noted;
}

// Once the world is drawn, before its colours (as torches.cpp applyTorches): the light of this frame's flames, the
// map's lamps and hidden torches, the skills' fire and the maps' glows into the picture (one pass, from its copy);
// 2.5.2: then each flame and the glow around it, the map's own fireflies, its glowing orbs and pools and its clouds
// of dust (a pass each, cut to it).
void Renderer11::applyTorchLight() {
    EffectSpan span(*this, kTimeLight11);  // 0.89
    lightDone_ = true;
    const bool placed = cameraValid_ && mapWorld_.axes().known();  // the map file's places known in the world
    const rsw::World* layout = placed ? mapWorld_.world() : nullptr;
    const bool torches = torchesWanted() && !flames_.empty();
    const bool lampsOn = effects_.lamps && effects_.lampsLight > 0 && torchLightPs_ && placed &&
                         !mapWorld_.lamps().empty();
    const bool hidden = effects_.torch && layout;  // the torches the game does not show
    const bool fire = fireWanted() && !fireSparks_.empty();  // 2.5.1: the skills' fire
    const bool glows = layout && effects_.mapGlows && effects_.mapGlowsBrightness > 0 && fireflyPs_;  // 2.5.2
    const bool mapFlies = layout && firefliesWanted();
    std::vector<torch::Flame> flames;
    flames.swap(flames_);
    // 2.6.17 (torches.cpp): each of the game's flames followed from frame to frame - its torch's seed kept, its fade
    // smoothed, a frame without it bridged (torch::Keeper)
    std::vector<uint32_t> torchSeeds;
    {
        const camera::Projection seen = camera::projectionFor(gameWidth_, gameHeight_);
        for (const torch::Flame& f : flames)
            torchSeeds.push_back(torch::placeSeed(f, seen, viewToWorld_, cameraValid_));
        torchKeeper_.update(flames, torchSeeds, double(waterTime()));
    }
    const size_t gameFlames = flames.size();  // 2.6.18 (torches.cpp): each the game's two effects of its torch
    std::vector<FireSpark11> sparks;
    sparks.swap(fireSparks_);
    std::vector<FireSpark11> lavaCells;  // 2.5.2: the lava's (noteLava)
    lavaCells.swap(lavaCells_);
    std::vector<FireSpark11> portalSparks;  // 2.5.3: the portals' draws (notePortal), the forest's beams (noteRay)
    portalSparks.swap(portalSparks_);
    std::vector<RayPart11> rayParts;
    rayParts.swap(rayParts_);
    if (!portalsWanted()) portalTracks_.clear();
    const bool nightFlies = nightGlowsWanted();  // 2.7.18
    if ((!torches && !lampsOn && !hidden && !fire && !glows && !mapFlies && lavaCells.empty() &&
         portalSparks.empty() && portalTracks_.empty() && rayParts.empty() && !(flamesWanted() && !flames.empty()) &&
         !nightFlies) ||
        !depth_ || !target_.texture || !depthViews())
        return;
    if (!lightData_ && torchLightPs_) {
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
        }
    }
    const float time = waterTime();  // 2.5.2: the effects' clock, as Direct3D 12's
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float amount = torch::flickerAmount(effects_.torchFlicker);  // 2.6.16
    const float strength = 0.4f * float(std::clamp(effects_.torchLight, 0, 200)) / 100.0f;
    const float k = float(scale_);
    const int w = target_.width, h = target_.height;
    const float margin = float(std::max(gameWidth_, gameHeight_)) * 0.25f;
    const float* m = viewToWorld_;  // the camera backwards, inverted: its rows are orthonormal
    const map::Axes axes = mapWorld_.axes();
    const auto toView = [m](const map::Point& p, float v[3]) {
        const float dx = p.x - m[9], dy = p.y - m[10], dz = p.z - m[11];
        v[0] = dx * m[0] + dy * m[1] + dz * m[2];
        v[1] = dx * m[3] + dy * m[4] + dz * m[5];
        v[2] = dx * m[6] + dy * m[7] + dz * m[8];
    };
    const auto onScreen = [&](float gx, float gy, float slack) {
        return gx > -slack && gy > -slack && gx < float(gameWidth_) + slack && gy < float(gameHeight_) + slack;
    };
    // 0.25 (torches.cpp): the map file's torches the game does not show - a flame each, in its colour
    if (hidden)
        for (const rsw::Effect& e : layout->effects) {
            torch::Flame f;
            if (flames.size() >= kMaxTorches || !hiddenTorch(e.id, f.tint)) continue;
            const map::Point base = map::toWorld(axes, map::Point{e.pos[0], e.pos[1] - 2.0f, e.pos[2]});
            float vb[3], vt[3];
            toView(base, vb);
            toView(map::Point{base.x, base.y - 13.0f, base.z}, vt);  // 13 units tall
            if (vb[2] <= proj.nearZ || vt[2] <= proj.nearZ) continue;
            f.baseX = vb[0] / vb[2] * proj.f0 + proj.cx;
            f.baseY = vb[1] / vb[2] * proj.f1 + proj.cy;
            f.upX = vt[0] / vt[2] * proj.f0 + proj.cx - f.baseX;
            f.upY = vt[1] / vt[2] * proj.f1 + proj.cy - f.baseY;
            f.halfWidth = std::hypot(f.upX, f.upY) * (torch::kHalfWidth / torch::kHeight);
            f.rhw = 1.0f / vb[2];
            if (!onScreen(f.baseX, f.baseY, margin)) continue;
            flames.push_back(f);
            torchSeeds.push_back(torch::seedAt(base.x, base.z));  // its exact place: a steady seed
        }
    // The flames' own flicker: each by its place in the world - whichever way the camera looks
    std::vector<float> flickers(flames.size(), 1.0f), seeds(flames.size(), 0.0f), calms(flames.size(), 1.0f);
    for (size_t i = 0; i < flames.size(); ++i) {
        const torch::Flame& f = flames[i];
        const uint32_t s = i < torchSeeds.size() ? torchSeeds[i] : torch::placeSeed(f, proj, m, cameraValid_);
        flickers[i] = torch::flicker(time, s, amount);
        seeds[i] = float(s % 1000u) / 1000.0f;
        calms[i] = torch::flicker(double(time) * 0.5, s ^ 0x5BD1E995u, amount * 0.35f);
    }
    // 0.28, 0.29 (torches.cpp): the fireflies of the map's makers (713, 714, 946) drifting about their places; the
    // maps' hidden lights - an orb floats and breathes, a pool lights the ground about it, a cloud of dust drifts
    struct Dot {
        float gx, gy, vz, r, bright, colour[3], sharp, halo;
    };
    struct Cloud {
        float gx, gy, vz, perUnit, radius, seed, bright, colour[3];
    };
    struct Item {
        float view[3], reach, colour[3], strength;
    };
    std::vector<Dot> dots;
    std::vector<Cloud> clouds;
    std::vector<Item> glowLights;
    if (mapFlies && effects_.fireflyDots) {  // 3.0.7-beta.3: the fireflies themselves in the air
        const float user = float(std::clamp(effects_.firefliesBrightness, 0, 200)) / 100.0f;
        uint32_t index = 0;
        for (const rsw::Effect& e : layout->effects) {
            if (e.id != 713 && e.id != 714 && e.id != 946) continue;
            const float seed = float(((++index) * 2654435761u) >> 8 & 0xFFFFu) / 65535.0f;
            if (dots.size() >= 96) break;
            const map::Point o = fireflies::wander(time, seed);
            float v[3];
            toView(map::toWorld(axes, map::Point{e.pos[0] + o.x, e.pos[1] + o.y, e.pos[2] + o.z}), v);
            if (v[2] <= proj.nearZ) continue;
            const float gx = v[0] / v[2] * proj.f0 + proj.cx, gy = v[1] / v[2] * proj.f1 + proj.cy;
            if (!onScreen(gx, gy, 32.0f)) continue;
            dots.push_back(Dot{gx, gy, v[2], 0.22f * proj.f0 / v[2], user * fireflies::blink(time, seed),
                               {1.0f, 0.86f, 0.3f}, 1.2f, 1.0f});
        }
    }
    if (glows) {
        const float user = float(std::clamp(effects_.mapGlowsBrightness, 0, 200)) / 100.0f;
        uint32_t index = 0;
        for (const rsw::Effect& e : layout->effects) {
            map_glows::Glow g;
            if (!map_glows::glowOf(e.id, g)) continue;
            const float seed = float(((++index) * 2246822519u) >> 8 & 0xFFFFu) / 65535.0f;
            const float lift = g.shape == map_glows::Shape::Orb ? map_glows::bob(time, seed) : 0.0f;
            float v[3];
            toView(map::toWorld(axes, map::Point{e.pos[0], e.pos[1] + lift, e.pos[2]}), v);
            if (v[2] <= proj.nearZ) continue;
            const float gx = v[0] / v[2] * proj.f0 + proj.cx, gy = v[1] / v[2] * proj.f1 + proj.cy;
            const float perUnit = proj.f0 / v[2];
            const float breath = g.shape == map_glows::Shape::Dust ? 1.0f : map_glows::pulse(time, seed);
            if (!onScreen(gx, gy, std::max(g.reach, g.size) * perUnit + 32.0f)) continue;
            if (g.light > 0.0f)
                glowLights.push_back(Item{{v[0], v[1], v[2]}, g.reach, {g.colour[0], g.colour[1], g.colour[2]},
                                          g.light * breath * user});
            if (!onScreen(gx, gy, g.size * perUnit + 32.0f)) continue;
            if (g.shape == map_glows::Shape::Dust) {
                if (clouds.size() < 32 && dustPs_)
                    clouds.push_back(Cloud{gx, gy, v[2], perUnit, g.size, seed, 0.35f * user,
                                           {g.colour[0], g.colour[1], g.colour[2]}});
            } else if (g.shape == map_glows::Shape::Orb && !effects_.mapGlowOrbs) {
                // 3.0.7-beta.1 (as torches.cpp): its light alone
            } else if (dots.size() < 96 + 64) {
                const bool pool = g.shape == map_glows::Shape::Pool;
                dots.push_back(Dot{gx, gy, v[2], g.size * perUnit, (pool ? 0.18f : 0.75f) * breath * user,
                                   {g.colour[0], g.colour[1], g.colour[2]}, pool ? 0.15f : 0.4f,
                                   pool ? 0.6f : 1.6f});
            }
        }
    }
    // The list of lights for the light's pass, two texels each
    float data[kMaxLights * 2][4] = {};
    float lx0 = float(w), ly0 = float(h), lx1 = 0.0f, ly1 = 0.0f;
    int count = 0;
    skillAirFirst_ = skillAirEnd_ = 0;  // 2.7.36: none until the skills' come
    // A light into the list: its place in the view and reach, its colour and strength; where its reach falls on
    // the screen (the cube about it, each corner projected) widens the lit rectangle.
    const auto addLight = [&](const float view[3], float reach, const float colour[3], float amountOf) {
        if (count == kMaxLights) return;
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
    const float darkNow = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;  // 2.7.18: how much night it is
    if (torchesWanted())
        for (size_t i = 0; i < flames.size(); ++i) {  // the game's flames and the hidden ones
            const torch::Flame& f = flames[i];
            const float mx = f.baseX + f.upX * kMiddle, my = f.baseY + f.upY * kMiddle;
            const float vz = 1.0f / f.rhw;
            const float view[3] = {(mx - proj.cx) * vz / proj.f0, (my - proj.cy) * vz / proj.f1, vz};
            const bool own = f.tint[0] < 0.99f || f.tint[1] < 0.99f || f.tint[2] < 0.99f;  // a hidden one's colour
            const float layers = i < gameFlames ? torch::kGameEffects : 1.0f;  // 2.6.18
            addLight(view, kReach, own ? f.tint : kLightColor, strength * calms[i] * f.alpha * layers);
        }
    if (fire) {  // 2.5.1: the fire of skills, gathered by place (cells of 80 game pixels), the brightest 16
        // 2.7.22 (torches.cpp): the orbs of light by caster first, one calm light each over its head
        // 2.7.24 (torches.cpp): a ground light over the player's ground, its strength negative
        float up[3] = {0.0f, 0.0f, 0.0f};
        if (cameraValid_) camera::upInView(viewToWorld_, up);
        const bool groundKnown = cameraValid_ && cameraTargetZ_ > 0.0f;
        const float groundY = groundKnown ? camera::targetHeight(viewToWorld_, cameraTargetZ_) : 0.0f;
        const float fireAmount = float(std::clamp(effects_.fireLightStrength, 0, 200)) / 100.0f;
        for (const effect_light::OrbPlace& p : effect_light::gatherOrbs(sparks, 160.0f)) {
            const effect_light::OrbLight o = effect_light::orbLight(p.w, darkNow, fireAmount);
            const float top = std::max({p.colour[0], p.colour[1], p.colour[2], 1e-4f});
            const float centre[3] = {p.view[0] / p.w, p.view[1] / p.w, p.view[2] / p.w};
            const float raise = groundKnown ? effect_light::raiseOverGround(centre, viewToWorld_, groundY) : o.height;
            const float view[3] = {centre[0] + raise * up[0], centre[1] + raise * up[1], centre[2] + raise * up[2]};
            const float colour[3] = {p.colour[0] / top, p.colour[1] / top, p.colour[2] / top};
            addLight(view, o.reach, colour, -o.strength);
        }
        struct Cell {
            float w = 0, view[3] = {0, 0, 0}, colour[3] = {0, 0, 0};
        };
        std::unordered_map<int, Cell> cells;
        for (const FireSpark11& f : sparks) {
            if (f.orb) continue;  // 2.7.21: an orb of light's - lit above
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
        const float fireStrength =  // 2.7.18: brighter at night; 2.7.36: up to 1.4 times, not 1.8
            effect_light::skillStrength(float(std::clamp(effects_.fireLightStrength, 0, 200)) / 100.0f, darkNow);
        for (size_t i = 0; i < best.size() && embers_.size() < 24 && effects_.skillSparks && embersPs_; ++i) {
            const Cell& c = best[i];  // 2.5.3 (torches.cpp 0.82): the places for the skills' sparks
            const float vz = c.view[2] / c.w;
            if (vz <= proj.nearZ) continue;
            const float top = std::max({c.colour[0], c.colour[1], c.colour[2], 1e-4f});
            Ember11 e{};
            e.x = (c.view[0] / c.w / vz * proj.f0 + proj.cx) * k + 0.5f;
            e.y = (c.view[1] / c.w / vz * proj.f1 + proj.cy) * k + 0.5f;
            e.size = 7.0f * proj.f0 / vz * k;  // about seven units across
            e.strength = 0.55f * std::min(c.w, 1.0f) * float(std::clamp(effects_.skillSparksAmount, 0, 200)) / 100.0f;
            for (int i2 = 0; i2 < 3; ++i2) e.colour[i2] = c.colour[i2] / top;
            e.fire = e.colour[0] > 0.8f && e.colour[2] < 0.6f;
            embers_.push_back(e);
        }
        if (heatWanted())  // 2.5.3: the hot air over burning skills (their middles, game pixels to the target's)
            for (size_t i = 0; i < best.size() && i < 16; ++i) {
                const Cell& c = best[i];
                const float vz = c.view[2] / c.w;
                if (vz <= proj.nearZ) continue;
                const float gx = c.view[0] / c.w / vz * proj.f0 + proj.cx;
                const float gy = c.view[1] / c.w / vz * proj.f1 + proj.cy;
                heatSpots_.push_back(HeatSpot11{gx * k, gy * k, 70.0f * k, 0.6f * std::min(c.w, 1.0f) * heatUser()});
            }
        skillAirFirst_ = count;
        for (size_t i = 0; i < best.size() && i < 16; ++i) {
            const Cell& c = best[i];
            const float top = std::max({c.colour[0], c.colour[1], c.colour[2], 1e-4f});
            const float view[3] = {c.view[0] / c.w, c.view[1] / c.w, c.view[2] / c.w};
            const float colour[3] = {c.colour[0] / top, c.colour[1] / top, c.colour[2] / top};
            const float w = effect_light::skillWeight(c.w, darkNow);  // 2.7.20 (torches.cpp): at night a floor
            addLight(view, 25.0f + 15.0f * w, colour, fireStrength * w);
        }
        skillAirEnd_ = count;
    }
    {  // 0.28: the fireflies light a little around them, the nearest 16 (2.5.2: the map's dots, its orbs and pools)
        std::vector<size_t> order(dots.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return dots[a].vz < dots[b].vz; });
        for (size_t i = 0, used = 0; i < order.size() && used < 16; ++i) {
            const Dot& f = dots[order[i]];
            if (f.bright < 0.05f) continue;
            const float view[3] = {(f.gx - proj.cx) * f.vz / proj.f0, (f.gy - proj.cy) * f.vz / proj.f1, f.vz};
            addLight(view, 12.0f, f.colour, 0.12f * f.bright);
            ++used;
        }
    }
    if (nightFlies)  // 2.7.18 (torches.cpp): the fireflies in the grass at night light the ground under them
        for (const NightGlow11& g : nightGlows_) {
            float view[3];
            toView(g.at, view);
            if (view[2] <= proj.nearZ) continue;
            static const float kGreen[3] = {0.75f, 1.0f, 0.42f};
            addLight(view, 9.0f, kGreen, 0.12f * g.lit);
        }
    if (!lavaCells.empty()) {  // 0.31 (torches.cpp): the lava's heat by place (96 px cells), the nearest 12
        struct Pool {
            float view[3] = {0, 0, 0};
            int n = 0;
        };
        std::unordered_map<int, Pool> pools;
        for (const FireSpark11& s : lavaCells) {
            Pool& q = pools[int(std::floor(s.gameX / 96.0f)) * 4096 + int(std::floor(s.gameY / 96.0f))];
            for (int i = 0; i < 3; ++i) q.view[i] += s.view[i];
            ++q.n;
        }
        std::vector<Pool> heat;
        for (auto& kv : pools) {
            Pool q = kv.second;
            for (int i = 0; i < 3; ++i) q.view[i] /= float(q.n);
            heat.push_back(q);
        }
        std::sort(heat.begin(), heat.end(), [](const Pool& a, const Pool& b) { return a.view[2] < b.view[2]; });
        const float user = float(std::clamp(effects_.lavaBrightness, 0, 200)) / 100.0f;
        static const float kLava[3] = {1.0f, 0.42f, 0.12f};
        for (size_t i = 0; i < heat.size() && i < 12; ++i) {
            const float breath = 0.85f + 0.15f * std::sin(time * 1.1f + float(i) * 2.3f);
            const float view[3] = {heat[i].view[0], heat[i].view[1] - 3.0f, heat[i].view[2]};
            addLight(view, 35.0f, kLava, 0.35f * user * breath);
            if (heatWanted() && heat[i].view[2] > proj.nearZ) {  // 2.5.3: the hot air over the lava
                const float gx = heat[i].view[0] / heat[i].view[2] * proj.f0 + proj.cx;
                const float gy = heat[i].view[1] / heat[i].view[2] * proj.f1 + proj.cy;
                heatSpots_.push_back(HeatSpot11{gx * k, gy * k, 110.0f * k, 0.45f * heatUser()});
            }
        }
    }
    // 0.38 (torches.cpp): the portals followed from frame to frame (their places eased, their strength fading in and
    // out): a soft light about each - 2.7.54 by the map warps' floors' middles; the air's swirl gone
    const float portalStep = std::clamp(time - portalTime_, 0.0f, 0.25f);
    portalTime_ = time;
    if (portalsWanted()) {
        std::vector<portal::Seen> seen;  // 2.7.50: followed by core/portal.hpp (0.39.1's dropped every new one)
        for (const FireSpark11& s : portalSparks)
            seen.push_back(portal::Seen{{s.view[0], s.view[1], s.view[2]}, s.gameX, s.gameY, s.strength, s.left,
                                        s.right, s.bottom});
        portal::follow(portalTracks_, seen, portalStep);
        const float user = float(std::clamp(effects_.portalsStrength, 0, 200)) / 100.0f;
        static const float kLavender[3] = {0.86f, 0.86f, 1.0f};  // 2.7.55: the warp's own
        int shownCount = 0;
        for (const PortalTrack11& t : portalTracks_) {
            const float shown = t.strength * t.strength * (3.0f - 2.0f * t.strength);  // eased in and out
            if (shown <= 0.0f) continue;
            // 2.7.52 (torches.cpp portalGround): 4 units over its foot (portal::footOf: its floor's middle)
            if (shownCount++ < 8 && cameraValid_) {
                float up[3], foot[3];
                camera::upInView(viewToWorld_, up);
                portal::footOf(t, up[2], proj, foot);
                if (foot[2] > 0.0f) {
                    const float view[3] = {foot[0] + up[0] * 4.0f, foot[1] + up[1] * 4.0f, foot[2] + up[2] * 4.0f};
                    addLight(view, 30.0f, kLavender, effect_light::portalStrength(user, darkNow) * shown);
                }
            }
        }
    }
    // 0.30 (torches.cpp): the forest's beams, gathered by place (cells of 64 game pixels): dust drifting in each,
    // light where it falls - the nearest 12
    if (!rayParts.empty()) {
        struct Beam {
            float middle[3] = {0, 0, 0}, foot[3] = {0, 0, 0};
            float gx = 0, gy = 0;
            int n = 0;
        };
        std::unordered_map<int, Beam> beams;
        for (const RayPart11& r : rayParts) {
            Beam& b = beams[int(std::floor(r.gameX / 64.0f)) * 4096 + int(std::floor(r.gameY / 64.0f))];
            for (int i = 0; i < 3; ++i) {
                b.middle[i] += r.middle[i];
                b.foot[i] += r.foot[i];
            }
            b.gx += r.gameX;
            b.gy += r.gameY;
            ++b.n;
        }
        std::vector<Beam> closest;
        for (auto& kv : beams) {
            Beam b = kv.second;
            for (int i = 0; i < 3; ++i) {
                b.middle[i] /= float(b.n);
                b.foot[i] /= float(b.n);
            }
            b.gx /= float(b.n);
            b.gy /= float(b.n);
            closest.push_back(b);
        }
        std::sort(closest.begin(), closest.end(),
                  [](const Beam& a, const Beam& b) { return a.middle[2] < b.middle[2]; });
        const float user = float(std::clamp(effects_.raysBrightness, 0, 200)) / 100.0f;
        for (size_t i = 0; i < closest.size() && i < 12; ++i) {
            const Beam& b = closest[i];
            if (b.middle[2] <= proj.nearZ) continue;
            glowLights.push_back(Item{{b.foot[0], b.foot[1], b.foot[2]}, 20.0f, {0.95f, 1.0f, 0.82f}, 0.22f * user});
            if (dustPs_ && clouds.size() < 32)
                clouds.push_back(Cloud{b.gx, b.gy, b.middle[2], proj.f0 / b.middle[2], 6.0f,
                                       float(i) * 0.137f + 0.05f, 0.3f * user, {1.0f, 0.97f, 0.86f}});
        }
    }
    std::sort(glowLights.begin(), glowLights.end(),
              [](const Item& a, const Item& b) { return a.view[2] < b.view[2]; });
    for (size_t i = 0; i < glowLights.size() && i < 24; ++i)  // 0.29: the nearest 24 of the maps' glows
        addLight(glowLights[i].view, glowLights[i].reach, glowLights[i].colour, glowLights[i].strength);
    if (mapFlies) {  // 3.0.7-beta.3 (torches.cpp): the maps' fireflies each a lamp at its place, the nearest 16
        const float user = float(std::clamp(effects_.firefliesBrightness, 0, 200)) / 100.0f;
        std::vector<Item> flyLamps;
        for (const rsw::Effect& e : layout->effects) {
            if (!fireflies::isFirefly(e.id)) continue;
            float v[3];
            toView(map::toWorld(axes, map::Point{e.pos[0], e.pos[1], e.pos[2]}), v);
            if (v[2] <= proj.nearZ - fireflies::kLampReach) continue;
            const float vz = std::max(v[2], proj.nearZ);
            if (!onScreen(v[0] / vz * proj.f0 + proj.cx, v[1] / vz * proj.f1 + proj.cy,
                          fireflies::kLampReach * proj.f0 / vz + margin))
                continue;
            Item l{{v[0], v[1], v[2]}, fireflies::kLampReach, {1.0f, 1.0f, 1.0f}, fireflies::kLampStrength * user};
            fireflies::lampColour(e.pos[0], e.pos[2], l.colour);
            flyLamps.push_back(l);
        }
        std::sort(flyLamps.begin(), flyLamps.end(), [](const Item& a, const Item& b) { return a.view[2] < b.view[2]; });
        for (size_t i = 0; i < flyLamps.size() && i < 16; ++i)
            addLight(flyLamps[i].view, flyLamps[i].reach, flyLamps[i].colour, flyLamps[i].strength);
    }
    if (lampsOn) {  // 2.5.1: the lamps the map's makers placed, the nearest to the camera first (as torches.cpp)
        const float lampStrength = 0.8f * float(std::clamp(effects_.lampsLight, 0, 200)) / 100.0f;
        std::vector<Item> lamps;
        for (const rsw::Light& r : mapWorld_.lamps()) {
            const map::Point wp = map::toWorld(axes, map::Point{r.pos[0], r.pos[1], r.pos[2]});
            float v[3];
            toView(wp, v);
            const float reach = std::clamp(r.range * 0.45f, 15.0f, 70.0f);
            if (v[2] <= proj.nearZ - reach) continue;
            const float vz = std::max(v[2], proj.nearZ);
            if (!onScreen(v[0] / vz * proj.f0 + proj.cx, v[1] / vz * proj.f1 + proj.cy, reach * proj.f0 / vz + margin))
                continue;
            const float top = std::max({r.color[0], r.color[1], r.color[2], 1e-3f});
            Item l{{v[0], v[1], v[2]}, reach, {0.0f, 0.0f, 0.0f}, lampStrength};
            for (int c = 0; c < 3; ++c) l.colour[c] = 0.65f + 0.35f * r.color[c] / top;  // 2.1.5: a third of it
            if (r.color[0] >= r.color[1] && r.color[1] >= r.color[2] * 0.9f && r.color[0] > 0.5f)  // a fire's
                l.strength *= torch::flicker(double(time) * 0.5, torch::seedAt(wp.x, wp.z), amount * 0.25f);
            lamps.push_back(l);
        }
        std::sort(lamps.begin(), lamps.end(), [](const Item& a, const Item& b) { return a.view[2] < b.view[2]; });
        for (const Item& l : lamps) addLight(l.view, l.reach, l.colour, l.strength);
    }
    for (const torch::Flame& f : flames) {  // 2.5.4: the flames' tips in the world, for the embers rising from them
        critters::Point at;
        if (critterFires_.size() < 32 && critters::fireOf(f, proj, m, at)) critterFires_.push_back(at);
    }
    if (heatWanted())  // 2.5.3: the hot air over each flame
        for (size_t i = 0; i < flames.size(); ++i) {
            const torch::Flame& f = flames[i];
            const float fh = std::hypot(f.upX, f.upY) * k;
            if (fh < 2.0f) continue;
            const float layers = i < gameFlames ? torch::kGameEffects : 1.0f;  // 2.6.18
            heatSpots_.push_back(HeatSpot11{(f.baseX + f.upX * 0.9f) * k + 0.5f, (f.baseY + f.upY * 0.9f) * k + 0.5f,
                                            fh * 1.4f, 0.5f * f.alpha * layers * heatUser()});
        }
    const D3D11_RECT lit{LONG(std::max(0.0f, std::floor(lx0))), LONG(std::max(0.0f, std::floor(ly0))),
                         LONG(std::min(float(w), std::ceil(lx1))), LONG(std::min(float(h), std::ceil(ly1)))};
    const bool light = count && torchLightPs_ && lightData_ && lit.right > lit.left && lit.bottom > lit.top;
    const bool drawFlames = flamesWanted() && !flames.empty();
    if (!light && !drawFlames && dots.empty() && clouds.empty()) return;
    // 3.0.5-beta.20: the picture before the night (scene_ still applyNight's copy) - only for light to draw
    if (dayWanted_ && light) keepDay(nightLook_);
    // The picture's copy (scene_) read by the light and the flames - taken before either writes the picture
    if (!copyScene()) return;
    targetBound_ = false;
    ID3D11ShaderResourceView* none[4] = {nullptr, nullptr, nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 4, none);
    ctx_->OMSetRenderTargets(1, &target_.rtv, nullptr);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(presentVs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    passSamplers();
    const UINT8 rgb = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
    Constants c{};
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    if (cameraValid_) std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);  // 2.7.24: the ground lights' up
    c.targetSize[0] = float(w);
    c.targetSize[1] = float(h);
    c.scale = k;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    // A pass of the current shader cut to [x0, x1) x [y0, y1) of the picture
    const auto cut = [&](const Constants& pc, float x0, float y0, float x1, float y1) {
        const D3D11_RECT box{LONG(std::max(0.0f, std::floor(x0))), LONG(std::max(0.0f, std::floor(y0))),
                             LONG(std::min(float(w), std::ceil(x1))), LONG(std::min(float(h), std::ceil(y1)))};
        if (box.right <= box.left || box.bottom <= box.top) return;
        ctx_->RSSetScissorRects(1, &box);
        setConstants(pc);
        ctx_->Draw(3, 0);
    };
    if (light) {  // the light from the list (torch_light_ps: t1 the list, t2 the copy, t3 the depth)
        ctx_->UpdateSubresource(lightData_, 0, nullptr, data, UINT(sizeof data), 0);
        shaftLights_ = count;  // 2.5.5: the light in the air reads the list too
        Constants lc = c;
        lc.footprint = float(count);
        lc.sourceRect[0] = effects_.roundLight ? 1.0f : 0.0f;  // 0.84
        lc.sharpen = effects_.rimLight ? 0.6f * float(std::clamp(effects_.rimStrength, 0, 200)) / 100.0f : 0.0f;
        // 2.7.47: t0 the picture before the night (applyNight's) - a lit surface in its own colours, with the look
        // night_ps gave it; under the open sky at night no brighter than by day (twinH)
        for (int i = 0; i < 3; ++i) lc.fogColor[i] = dayKept_ ? dayTint_[i] : 1.0f;
        lc.fogColor[3] = dayKept_ ? daySaturation_ : 0.0f;
        lc.sourceClamp[0] = 0.0f;  // no lightning on Direct3D 11
        lc.twin[3] = dayKept_ ? effect_light::dayBack(outdoors_, effects_.nightNow) : 0.0f;
        ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO, rgb), nullptr, 0xFFFFFFFF);
        ctx_->PSSetShader(torchLightPs_, nullptr, 0);
        ID3D11ShaderResourceView* views[4] = {dayKept_ ? day_.srv : scene_.srv, lightDataSrv_, scene_.srv, depthSrv_};
        ctx_->PSSetShaderResources(0, 4, views);
        cut(lc, float(lit.left), float(lit.top), float(lit.right), float(lit.bottom));
        ctx_->PSSetShaderResources(0, 4, none);
    }
    ID3D11ShaderResourceView* sceneViews[4] = {nullptr, nullptr, scene_.srv, depthSrv_};
    ctx_->PSSetShaderResources(0, 4, sceneViews);  // t2 the copy, t3 the depth
    if (drawFlames) {  // 2.5.2: each flame and the glow around it (torch_fire_ps), premultiplied over
        const float glowStrength = float(std::clamp(effects_.torchGlow, 0, 200)) / 100.0f;
        const float brightness = 0.65f * float(std::clamp(effects_.torchBrightness, 0, 200)) / 100.0f;
        ctx_->OMSetBlendState(blendState(true, d3d7::BLEND_ONE, d3d7::BLEND_INVSRCALPHA, rgb), nullptr, 0xFFFFFFFF);
        ctx_->PSSetShader(torchFirePs_, nullptr, 0);
        for (size_t i = 0; i < flames.size(); ++i) {
            const torch::Flame& f = flames[i];
            const float bx = f.baseX * k + 0.5f, by = f.baseY * k + 0.5f, ux = f.upX * k, uy = f.upY * k;
            const float fh = std::hypot(ux, uy);
            if (fh < 2.0f) continue;  // too small to see
            const bool own = f.tint[0] < 0.99f || f.tint[1] < 0.99f || f.tint[2] < 0.99f;
            Constants fc = c;
            fc.sourceRect[0] = bx;
            fc.sourceRect[1] = by;
            fc.sourceRect[2] = ux;
            fc.sourceRect[3] = uy;
            fc.sourceClamp[0] = f.halfWidth * k;
            fc.sourceClamp[1] = 1.0f / f.rhw;
            fc.sourceClamp[2] = i < gameFlames ? torch::kGameEffects : 1.0f;  // 2.6.18: drawn as the game's two were
            fc.sourceClamp[3] = seeds[i];
            fc.uiPixels[2] = flickers[i];
            fc.uiPixels[3] = time;
            fc.twin[0] = glowStrength;
            fc.twin[1] = f.alpha;
            fc.twin[2] = effects_.torchFlame || own ? brightness : 0.0f;
            std::copy(f.tint, f.tint + 3, fc.fogColor);
            const float mx = bx + ux * kMiddle, my = by + uy * kMiddle, r = fh * kGlowRadius;
            cut(fc, mx - r, my - r, mx + r, my + r);
        }
    }
    if (!dots.empty()) {  // 0.28, 0.29: each firefly's, orb's and pool's dot and halo (firefly_ps), added
        ctx_->OMSetBlendState(blendState(true, d3d7::BLEND_ONE, d3d7::BLEND_ONE, rgb), nullptr, 0xFFFFFFFF);
        ctx_->PSSetShader(fireflyPs_, nullptr, 0);
        for (const Dot& f : dots) {
            if (f.bright < 0.01f) continue;
            const float fx = f.gx * k + 0.5f, fy = f.gy * k + 0.5f, r = std::max(f.r * k, 0.6f);
            Constants dc = c;
            dc.sourceRect[0] = fx;
            dc.sourceRect[1] = fy;
            dc.sourceRect[2] = r;
            dc.sourceRect[3] = f.sharp;
            dc.sourceClamp[1] = f.vz;
            dc.twin[0] = f.bright;
            dc.twin[1] = f.halo;
            std::copy(f.colour, f.colour + 3, dc.fogColor);
            cut(dc, fx - 8.0f * r, fy - 8.0f * r, fx + 8.0f * r, fy + 8.0f * r);
        }
    }
    if (!clouds.empty()) {  // 0.29: each cloud of dust (dust_ps), added
        ctx_->OMSetBlendState(blendState(true, d3d7::BLEND_ONE, d3d7::BLEND_ONE, rgb), nullptr, 0xFFFFFFFF);
        ctx_->PSSetShader(dustPs_, nullptr, 0);
        for (const Cloud& d : clouds) {
            const float dx = d.gx * k + 0.5f, dy = d.gy * k + 0.5f, perUnit = d.perUnit * k;
            const float reach = d.radius * perUnit * 1.3f + 4.0f;
            Constants cc = c;
            cc.sourceRect[0] = dx;
            cc.sourceRect[1] = dy;
            cc.sourceRect[2] = perUnit;
            cc.sourceRect[3] = d.radius;
            cc.sourceClamp[1] = d.vz;
            cc.sourceClamp[3] = d.seed;
            cc.uiPixels[3] = time;
            cc.twin[0] = d.bright;
            std::copy(d.colour, d.colour + 3, cc.fogColor);
            cut(cc, dx - reach, dy - reach, dx + reach, dy + reach);
        }
    }
    ctx_->PSSetShaderResources(0, 4, none);
    const D3D11_RECT all{0, 0, w, h};
    ctx_->RSSetScissorRects(1, &all);
    if (!lightLogged_)
        LOG_INFO("torches: %d lights, %zu flames, %zu glows, %zu clouds of dust (Direct3D 11%s%s)", count,
                 drawFlames ? flames.size() : size_t(0), dots.size(), clouds.size(),
                 lampsOn ? ", the map's lamps" : "", fire ? ", the skills' fire" : "");
    lightLogged_ = lightLogged_ || count > 0 || drawFlames;
}

}  // namespace uagfx::gpu
