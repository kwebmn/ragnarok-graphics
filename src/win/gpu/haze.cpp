#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cstring>

#include "core/game_camera.hpp"
#include "core/view3d.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.34: blended over the target (its samples) or the world under DLSS (one sample) by its alpha.
ID3D12PipelineState* Renderer::Impl::mistPipeline(UINT n) {
    return alphaPipeline(mistPs, mistPipelines, n, "mists");
}

// Once the world is drawn and lit (before its colours): the low mists - the morning's over the fields and the towns,
// a swamp's (2.6.8: the far haze and the dungeons' dark and low fog, the pass's own until now, gone at the player's
// word; it was the mists' switch too).
bool Renderer::Impl::applyMist() {
    mistDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeMist);  // 0.39.2
    // 1.7: a low fog in the morning over the fields and the towns (morning_fog), a swamp's mist always (swamp)
    const float morning = effects.morningFog && mapOutdoors && !mapDesert ? living::morningFog(minuteNow()) : 0.0f;
    const bool swampy = effects.swamp && mapPlace.swamp;
    if (morning <= 0.0f && !swampy) return true;
    if (!ensureScene("mists")) return true;
    ID3D12PipelineState* pso = mistPipeline(upscale ? 1 : samples);
    if (!pso) return true;
    if (!copySceneDepth()) return false;
    const int w = sceneWidth, h = sceneHeight;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    // 2.2.2: the low fog lies at the ground's height at the character (sourceClamp.x, .y 1 when known) - five depths
    // across the screen gave it until now, and walking past walls (their tops, the void beyond) it jumped up and
    // down: the walls went light and dark by turns (alde_dun03)
    if (cameraValid && cameraTargetZ > 0.0f) {
        c.sourceClamp[0] = camera::targetHeight(viewToWorld, cameraTargetZ);
        c.sourceClamp[1] = 1.0f;
    }
    if (swampy) {  // 1.7: a swamp's mist, always
        static const float kMurk[3] = {0.62f, 0.68f, 0.55f};
        std::copy(kMurk, kMurk + 3, c.sourceRect);
        c.sourceRect[3] = 0.3f;
    }
    if (morning * 0.6f > c.sourceRect[3]) {  // the morning's, thicker
        static const float kMist[3] = {0.84f, 0.87f, 0.9f};
        std::copy(kMist, kMist + 3, c.sourceRect);
        c.sourceRect[3] = std::min(1.0f, morning * 0.6f);
    }
    c.sharpen = waterTime();
    if (!mistLogged) LOG_INFO("mists: %s over %s", swampy ? "a swamp's" : "the morning's", mapName.c_str());
    mistLogged = true;
    return worldPass(pso, w, h, c, sceneDepthSrv, true);
}

// 3.0.19-beta.6 (the 3D view, core/view3d): the game's camera from behind or from the eyes sees past all the game ever
// drew - a sky where nothing is drawn (the horizon's colour up to the zenith's, the sun the map's light comes from,
// soft clouds; a dark vault in a dungeon) and a haze to it over what is, by its distance (sky3d_ps). After the mists.
bool Renderer::Impl::applySky3d() {  // the haze over what is drawn, at the world's end
    sky3dDone = true;
    if (!skyFillDone && !applySkyFill()) return false;  // no blended draw this picture: the sky now
    return skyPass(false);
}

// 3.0.20-beta.48: the sky where nothing is drawn - before the world's blended draws
bool Renderer::Impl::applySkyFill() {
    skyFillDone = true;
    return skyPass(true);
}

bool Renderer::Impl::skyPass(bool fill) {
    if (!ensureScene("3D sky")) return true;
    const UINT n = upscale ? 1 : samples;
    ID3D12PipelineState* pso = fill ? alphaPipeline(sky3dPs, sky3dPipelines, n, "3D sky")
                                    : alphaPipeline(sky3dHazePs, sky3dHazePipelines, n, "3D sky's haze");
    if (!pso) return true;
    if (!copySceneDepth()) return false;
    const int w = sceneWidth, h = sceneHeight;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    // 3.0.20-beta.49: the open sky's parts - stars, the moon, the dawn's glow ([view3d] stars, moon, dawn)
    c.uiPixels[2] = float(view3d::skyParts(effects.skyStars, effects.skyMoon, effects.skyDawn));
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    const bool vault = !mapOutdoors;
    const view3d::Sky sky = view3d::skyOf(night, vault);  // the horizon's colour, the haze's reach
    std::copy(sky.horizon, sky.horizon + 3, c.sourceRect);
    c.sourceRect[3] = view3d::hazeReach(sky.reach, camera::sightNow());  // 3.0.19-beta.24: whole at the sight
    float toSun[3] = {0.0f, 0.0f, 0.0f};
    if (!vault && mapSun(toSun)) std::copy(toSun, toSun + 3, c.sourceClamp);
    c.twin[0] = night;
    c.twin[1] = vault ? 1.0f : 0.0f;
    c.sharpen = waterTime();
    // 3.0.19-beta.32: a dungeon's vault (t1: its heights a cell; fogColor its cell's units, its cells, the up axis's
    // sign; twinW/twinH the ground's x and z signs)
    const UINT vaultT = vault && fill ? vaultTexture() : kNoSrv;
    if (vaultT != kNoSrv) {
        const map::Axes axes = mapWorld.axes();
        c.footprint = 1.0f;
        c.fogColor[0] = vaultMap.zoom;
        c.fogColor[1] = float(vaultMap.width);
        c.fogColor[2] = float(vaultMap.height);
        c.fogColor[3] = float(axes.y);
        c.twin[2] = float(axes.x);
        c.twin[3] = float(axes.z);
        // 3.0.19-beta.38 (R-S3NQBF): its rock by the map's own light (sourceClamp - no sun under a vault)
        const rsw::World* rw = mapWorld.world();
        const float none[3] = {0, 0, 0};
        cave_vault::rockOf(rw && rw->lightKnown, rw ? rw->lightAmbient : none, rw ? rw->lightDiffuse : none,
                           c.sourceClamp);
    }
    if (fill && sky3dLogged != mapName) {
        sky3dLogged = mapName;
        LOG_INFO("3D sky: on %s - %s, the night %.2f, the sun %s (field of view %.0f)", mapName.c_str(),
                 vault ? "a dark vault" : "the open sky", night,
                 c.sourceClamp[0] != 0.0f || c.sourceClamp[1] != 0.0f || c.sourceClamp[2] != 0.0f ? "the map's"
                                                                                                    : "none",
                 camera::fovNow());
    }
    return worldPass(pso, w, h, c, sceneDepthSrv, true, vaultT);
}

UINT Renderer::Impl::vaultTexture() {
    if (!effects.caveVault || vaultFailed || !cave_vault::forMap(mapName)) return kNoSrv;
    const uint64_t key = mapWorld.laidKey();  // the ground read and its axes known
    if (!key || !mapWorld.ground()) return kNoSrv;
    if (key != vaultKey) {
        vaultKey = key;
        vaultUploaded = false;
        // 3.0.20-beta.46 (R-3T9MZC «колоны как будто в воздухе»): down to the posts that reach its walls' height
        size_t met = 0;
        vaultMade = cave_vault::build(*mapWorld.ground(), mapWorld.posts(), vaultMap, &met);
        if (vaultMade)
            LOG_INFO("3D sky: %s - a vault over its passages, its relief mirrored about its walls' top %.0f (its "
                     "floors %.0f), %dx%d heights %.1f apart; it comes down to %zu of the map's %zu posts",
                     mapName.c_str(), vaultMap.wallTop, vaultMap.floorLevel, vaultMap.width, vaultMap.height,
                     double(vaultMap.zoom), met, mapWorld.posts().size());
    }
    if (!vaultMade) return kNoSrv;
    if (!vaultUploaded) {
        if (!vaultData || vaultTexW != vaultMap.width || vaultTexH != vaultMap.height) {
            if (vaultData) discard(vaultData);
            vaultData = createTexture2D(device, vaultMap.width, vaultMap.height, 1, DXGI_FORMAT_R32_FLOAT,
                                        D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
            if (vaultData && vaultSrv == kNoSrv) vaultSrv = allocSrv();
            if (!vaultData || vaultSrv == kNoSrv) {
                LOG_ERROR("3D sky: no memory for the vault's heights - the dark sky in the dungeons");
                releaseCom(vaultData);
                vaultFailed = true;  // not tried again
                return kNoSrv;
            }
            vaultState = D3D12_RESOURCE_STATE_COPY_DEST;
            device->CreateShaderResourceView(vaultData, nullptr, cpuSrvHandle(vaultSrv));
            vaultTexW = vaultMap.width;
            vaultTexH = vaultMap.height;
        }
        const UINT row = UINT(vaultMap.width) * 4, pitch = (row + 255u) & ~255u;  // rows 256 bytes apart
        const Upload u = allocUpload(UINT64(pitch) * UINT(vaultMap.height), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (!u.cpu) return kNoSrv;  // the next frame again
        for (int y = 0; y < vaultMap.height; ++y)
            std::memcpy(u.cpu + size_t(y) * pitch, vaultMap.up.data() + size_t(y) * size_t(vaultMap.width), row);
        D3D12_TEXTURE_COPY_LOCATION src{}, to{};
        src.pResource = slots[slot].upload;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = u.offset;
        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
        src.PlacedFootprint.Footprint.Width = UINT(vaultMap.width);
        src.PlacedFootprint.Footprint.Height = UINT(vaultMap.height);
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = pitch;
        to.pResource = vaultData;
        to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        transition(vaultData, vaultState, D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
        transition(vaultData, vaultState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        vaultUploaded = true;
    }
    return vaultSrv;
}

// 0.73: the light in the air - about the torches' pass's lights (their list left in torchData) and, under the open
// sky, rays from where the sky shows past the map's edge (shafts_ps), added on.
bool Renderer::Impl::applyShafts() {
    shaftsDone = true;
    EffectTime timing(*this, kTimeShafts);
    if (!ensureScene("light in the air") || !torchReady() || !shaftsPs || !halfOverPs) return true;
    // 2.5.7: at half the world's size into a target of its own, then added over the world smoothly
    if (!shaftsHalfPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, shaftsPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&shaftsHalfPipeline)))) {
            shaftsHalfPipeline = nullptr;
            LOG_ERROR("light in the air: the pipeline failed: none");
            releaseCom(shaftsPs);  // not tried again
            return true;
        }
    }
    ID3D12PipelineState* pso = addPipeline(halfOverPs, halfAddPipelines, upscale ? 1 : samples, "light in the air");
    if (!pso) return true;
    const int hw = std::max(1, (sceneWidth + 1) / 2), hh = std::max(1, (sceneHeight + 1) / 2);
    if (!shaftsTarget || hw != shaftsWidth || hh != shaftsHeight) {
        discard(shaftsTarget);
        shaftsTarget = createTexture2D(device, hw, hh, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                       D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                       nullptr);
        shaftsState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        shaftsWidth = hw;
        shaftsHeight = hh;
        if (!shaftsTarget) {
            LOG_ERROR("light in the air: no video memory at %dx%d: none", hw, hh);
            releaseCom(shaftsPs);
            return true;
        }
        nameResource(shaftsTarget, "light in the air");
        device->CreateRenderTargetView(shaftsTarget, nullptr, rtvHandle(kRtvShafts));
        device->CreateShaderResourceView(shaftsTarget, nullptr, cpuSrvHandle(shaftsSrv));
    }
    if (!copySceneDepth()) return false;
    transition(torchData, torchDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const int w = sceneWidth, h = sceneHeight;
    const float user = float(std::clamp(effects.shaftsStrength, 0, 200)) / 100.0f;
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.footprint = float(shaftLights);
    c.twin[0] = 0.22f * user * (1.0f + 0.6f * night);  // the glow shows more in the dark
    c.twin[1] = mapOutdoors && effects.skyRays ? 0.5f * user * (1.0f - night) : 0.0f;  // 2.4: a box of their own
    // The sun up and to the left past the screen - against the way the characters' shadows fall (actor_shadows.cpp)
    float sun[2];
    sunOnScreen(sun);
    c.sourceRect[0] = float(w) * (0.5f - sun[0] * 1.6f);
    c.sourceRect[1] = float(h) * (0.5f - sun[1] * 1.6f);
    c.sourceRect[2] = 1.0f;
    c.sourceRect[3] = 2.0f;  // 2.5.7: a pixel for the world's two by two
    c.twin[2] = float(skillAirFirst);  // 2.7.36: the skills' lights, a third of the glow
    c.twin[3] = float(skillAirEnd);
    const UINT lights = torchDataSrv;
    transition(shaftsTarget, shaftsState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    list->ClearRenderTargetView(rtvHandle(kRtvShafts), none, 0, nullptr);
    if (!effectPass(shaftsHalfPipeline, shaftsTarget, shaftsState, rtvHandle(kRtvShafts), hw, hh, c, sceneColorSrv,
                    true, lights))
        return false;
    transition(shaftsTarget, shaftsState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return worldPass(pso, w, h, c, shaftsSrv, false);
}

// 0.72: the floors reflect what stands on them - indoors and in the rain (floor_reflect_ps, from the picture's copy).
bool Renderer::Impl::applyFloorReflections() {
    floorReflectDone = true;
    EffectTime timing(*this, kTimeFloor);
    if (!ensureScene("floor reflections") || !halfOverPs || !floorReflectPs) return true;
    // 0.86: at half the world's size (a quarter of the rays), then laid over it smoothly.
    if (!floorReflectPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, floorReflectPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&floorReflectPipeline)))) {
            floorReflectPipeline = nullptr;
            releaseCom(floorReflectPs);
            LOG_ERROR("floor reflections: the pipeline failed: none");
            return true;
        }
    }
    ID3D12PipelineState* over =
        blendPipeline(halfOverPs, halfOverPipelines, upscale ? 1 : samples, "floor reflections", 3);
    if (!over) return true;
    const int w = sceneWidth, h = sceneHeight;
    const int hw = std::max(1, (w + 1) / 2), hh = std::max(1, (h + 1) / 2);
    if (!floorTarget || hw != floorWidth || hh != floorHeight) {
        discard(floorTarget);  // 2.4.3
        floorTarget = createTexture2D(device, hw, hh, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                      nullptr);
        floorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        floorWidth = hw;
        floorHeight = hh;
        if (!floorTarget) {
            LOG_ERROR("floor reflections: no video memory at %dx%d", hw, hh);
            releaseCom(floorReflectPs);
            return true;
        }
        device->CreateRenderTargetView(floorTarget, nullptr, rtvHandle(kRtvFloor));
        device->CreateShaderResourceView(floorTarget, nullptr, cpuSrvHandle(floorSrv));
    }
    if (!copySceneColor() || !copySceneDepth()) return false;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.twin[0] = floorReflectAmount();
    c.twin[1] = wetWanted() ? 1.0f : 0.0f;  // 0.97
    c.twin[2] = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;  // 2.7.36: the puddles' sky darker at night
    c.sharpen = waterTime();                // 1.7: the drops' rings in the puddles
    if (!floorReflectLogged) LOG_INFO("floor reflections: on %s, at %dx%d", mapName.c_str(), hw, hh);
    floorReflectLogged = true;
    if (!effectPass(floorReflectPipeline, floorTarget, floorState, rtvHandle(kRtvFloor), hw, hh, c, sceneColorSrv,
                    true))
        return false;
    transition(floorTarget, floorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return worldPass(over, w, h, c, floorSrv, false);
}

}  // namespace uagfx::gpu
