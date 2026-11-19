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

bool Renderer::Impl::skySun(float toSun[3]) const {
    float way[3];
    if (!mapSun(way)) return false;
    view3d::skySun(effects.dayCycle ? effects.dayNow : -1, way, toSun);
    return true;
}

int Renderer::Impl::skyPartsNow() const {
    // a map hung in the sky: by its name, or by its ground's file where the own ground was read (no land past its
    // edges made for it); the sea past wet edges likewise
    const OwnGround* g = mapWorld.ownGround();
    const bool hung = view3d::hungMap(mapName) || (g && g->outskirts.chunks.empty());
    const bool sea = g && !g->seaFrame.empty();
    return view3d::skyParts(effects.skyStars, effects.skyMoon, effects.skyDawn,
                            view3d::rangesFor(effects.skyRanges, mapOutdoors, hung, sea, mapPlace.sand,
                                              mapPlace.snow),
                            effects.dayCycle);  // 3.0.22-beta.15: the moon opposite the moving sun
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
    if (fill) horizonFrame();  // 3.0.22-beta.11: the horizon's strip - its bake's next slice (horizon.cpp)
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
    c.uiPixels[2] = float(skyPartsNow());  // 3.0.20-beta.72: and the far ranges along the horizon
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    const bool vault = !mapOutdoors;
    const view3d::Sky sky = view3d::skyOf(night, vault);  // the horizon's colour, the haze's reach
    std::copy(sky.horizon, sky.horizon + 3, c.sourceRect);
    c.sourceRect[3] = view3d::hazeReach(sky.reach, camera::sightNow());  // 3.0.19-beta.24: whole at the sight
    float toSun[3] = {0.0f, 0.0f, 0.0f};
    if (!vault && skySun(toSun)) std::copy(toSun, toSun + 3, c.sourceClamp);  // 3.0.22-beta.15: by the clock
    c.twin[0] = night;
    c.twin[1] = vault ? 1.0f : 0.0f;
    c.sharpen = waterTime();
    // 3.0.19-beta.32: a dungeon's vault (t1: its heights a cell; fogColor its cell's units, its cells, the up axis's
    // sign; twinW/twinH the ground's x and z signs)
    // 3.0.20-beta.57 (R-J3GKD9): in a house the room the player stands in - the haze's pass paints the others out, so
    // it takes the ceiling too
    const int room = vault ? roomNow() : 0;
    const UINT vaultT = vault && (fill || room) ? vaultTexture() : kNoSrv;
    UINT rockT = kNoSrv;  // 3.0.20-beta.54
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
        const bool lit = rw && rw->lightKnown;
        const float* ambient = rw ? rw->lightAmbient : none;
        const float* diffuse = rw ? rw->lightDiffuse : none;
        cave_vault::rockOf(lit, ambient, diffuse, c.sourceClamp);
        // 3.0.20-beta.52: a ceiling's height (0: a vault); 3.0.20-beta.57: over each room's own floor - far over
        // all of them here (no wall of the ground stands over a ceiling of its own height)
        c.sourceClamp[3] = vaultMap.perRoom ? 1e6f : vaultMap.ceiling;
        if (room && vaultMap.ceiling > 0.0f) {  // 3.0.20-beta.57: the rooms a cell (t0), the player's
            const UINT roomsT = roomsTexture();
            if (roomsT != kNoSrv) {
                rockT = roomsT;
                c.hole[6] = float(room);
                if (roomsLogged != mapName) {
                    roomsLogged = mapName;
                    LOG_INFO("3D sky: on %s the room the player stands in (%d of %d) drawn alone - the others as "
                             "if nothing were there ([view3d] one_room)", mapName.c_str(), room,
                             mapWorld.rooms() ? mapWorld.rooms()->count : 0);
                }
            }
        }
        // 3.0.20-beta.54 (the owner's «делай 1-9», 5, part 2): a ceiling in its walls' upper part's colour; a
        // dungeon's vault in the map's own rock - a patch of it laid as a mosaic (t0, hole: its mean colour, its
        // side, its levels, its texels a unit), or its colour alone on the vault's stone ([view3d] vault_rock)
        if (vaultMap.ceiling > 0.0f) {
            float walls[3];
            const bool known = mapWorld.wallsColour(walls);
            cave_vault::ceilingColour(known, walls, lit, ambient, diffuse, c.sourceClamp);
        } else if (effects.vaultRock && mapWorld.vaultRock().known) {
            const MapWorld::VaultRock& rock = mapWorld.vaultRock();
            float light[3];
            cave_vault::rockLight(lit, ambient, diffuse, light);
            rockT = vaultRockTexture();
            if (rockT != kNoSrv) {
                std::copy(rock.mean, rock.mean + 3, c.hole);
                c.hole[3] = float(rock.patch.width);
                c.hole[4] = float(vaultRockLevels);
                c.hole[5] = rock.texels;
                std::copy(light, light + 3, c.sourceClamp);
            } else {
                for (int k = 0; k < 3; ++k) c.sourceClamp[k] = std::clamp(rock.mean[k] * light[k], 0.04f, 0.9f);
            }
        }
    }
    if (fill && sky3dLogged != mapName) {
        sky3dLogged = mapName;
        LOG_INFO("3D sky: on %s - %s, the night %.2f, the sun %s (field of view %.0f)", mapName.c_str(),
                 vault ? "a dark vault" : "the open sky", night,
                 c.sourceClamp[0] != 0.0f || c.sourceClamp[1] != 0.0f || c.sourceClamp[2] != 0.0f ? "the map's"
                                                                                                    : "none",
                 camera::fovNow());
    }
    UINT t0 = rockT != kNoSrv ? rockT : sceneDepthSrv, t1 = vaultT;
    // 3.0.22-beta.11 (core/horizon): the region's land drawn by the card - under the open sky t0 and t1 are free
    if (!vault && horizonShown) {
        for (int k = 0; k < 2; ++k)
            transition(horizonTex[k], horizonState[k], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        t1 = horizonSrv[0];
        t0 = horizonSrv[1];
        c.uiPixels[2] = float(skyPartsNow() | view3d::kPanoramaPart);
        c.hole[0] = horizonFade;
    }
    return worldPass(pso, w, h, c, t0, true, t1);
}

// 3.0.20-beta.57 (R-J3GKD9): the room the player stands in - by the place the game's camera looks at (the eye where
// that is not known), in the map's file's places; 0: no rooms on this map, the switch off, or no room there
int Renderer::Impl::roomNow() const {
    const rooms::Map* rooms = mapWorld.rooms();
    if (!rooms || rooms->count < 2 || !effects.oneRoom3d || !effects.ceiling3d || !viewToWorldValid ||
        !mapWorld.axes().known() || !cave_vault::ceilingFor(mapName, mapOutdoors))
        return 0;
    const float* m = viewToWorld;
    const float tz = cameraTargetZ > 0.0f ? cameraTargetZ : 0.0f;
    const map::Axes axes = mapWorld.axes();
    return rooms::roomAt(*rooms, (tz * m[6] + m[9]) * float(axes.x), (tz * m[8] + m[11]) * float(axes.z));
}

// 3.0.20-beta.59 (R-4Q8PHP): whether a blended draw of the game's stands in another room than the player's - a
// portal's rings and sparks, an effect, a sprite there: left out (they lay lit over the dark, with no depth of their
// own for the haze's pass to tell them by)
bool Renderer::Impl::inAnotherRoom(const FrameRecord& rec, const DrawCmd& d) {
    if (frameRoom < 0) frameRoom = camera::wideView() && !mapOutdoors ? roomNow() : 0;
    const rooms::Map* rooms = mapWorld.rooms();
    if (frameRoom <= 0 || !rooms) return false;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const size_t looked = size_t(std::min<uint32_t>(d.vertexCount, 4)) * stride;
    if (!d.vertexCount || stride < 16 || d.vertexOffset + looked > v.size()) return false;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const map::Axes axes = mapWorld.axes();
    const int room = rooms::roomOfDraw(*rooms, v.data() + d.vertexOffset, stride, d.vertexCount, viewToWorld, p.f0,
                                       p.f1, p.cx, p.cy, axes.x, axes.z);
    if (room == 0 || room == frameRoom) return false;
    if (roomDrawsLeft++ == 0)
        LOG_INFO("3D sky: on %s the game's blended draws in the other rooms are left out too - a portal's rings and "
                 "sparks, an effect, a sprite there ([view3d] one_room)", mapName.c_str());
    return true;
}

// the rooms a cell on the card - once a map
UINT Renderer::Impl::roomsTexture() {
    const rooms::Map* rooms = mapWorld.rooms();
    const uint64_t key = mapWorld.laidKey();
    if (roomsFailed || !key || !rooms || rooms->width <= 0 || rooms->height <= 0 ||
        rooms->widened.size() < size_t(rooms->width) * size_t(rooms->height))
        return kNoSrv;
    if (key == roomsKey && roomsMap) return roomsSrv;
    if (roomsMap) discard(roomsMap);
    roomsMap = nullptr;
    roomsKey = 0;
    roomsMap = createTexture2D(device, rooms->width, rooms->height, 1, DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_NONE,
                               D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
    if (roomsMap && roomsSrv == kNoSrv) roomsSrv = allocSrv();
    if (!roomsMap || roomsSrv == kNoSrv) {
        LOG_ERROR("3D sky: no memory for the map's rooms (%dx%d) - all of them drawn", rooms->width, rooms->height);
        releaseCom(roomsMap);
        roomsFailed = true;  // not tried again
        return kNoSrv;
    }
    nameResource(roomsMap, "map's rooms");
    roomsState = D3D12_RESOURCE_STATE_COPY_DEST;
    device->CreateShaderResourceView(roomsMap, nullptr, cpuSrvHandle(roomsSrv));
    const UINT row = UINT(rooms->width), pitch = (row + 255u) & ~255u;  // rows 256 bytes apart
    const Upload u = allocUpload(UINT64(pitch) * UINT(rooms->height), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!u.cpu) {  // the next frame again
        discard(roomsMap);
        roomsMap = nullptr;
        return kNoSrv;
    }
    for (int y = 0; y < rooms->height; ++y)
        std::memcpy(u.cpu + size_t(y) * pitch, rooms->widened.data() + size_t(y) * size_t(rooms->width), row);
    flushDraws();  // the draws gathered so far before the copy
    D3D12_TEXTURE_COPY_LOCATION src{}, to{};
    src.pResource = slots[slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = u.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;
    src.PlacedFootprint.Footprint.Width = UINT(rooms->width);
    src.PlacedFootprint.Footprint.Height = UINT(rooms->height);
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = pitch;
    to.pResource = roomsMap;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
    transition(roomsMap, roomsState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    roomsKey = key;
    return roomsSrv;
}

// 3.0.20-beta.54: the map's own rock's patch on the card, its levels too - once a map
UINT Renderer::Impl::vaultRockTexture() {
    const MapWorld::VaultRock& rock = mapWorld.vaultRock();
    const uint64_t key = mapWorld.laidKey();
    if (vaultRockFailed || !key || rock.patch.levels.empty() || rock.patch.width <= 0) return kNoSrv;
    if (key == vaultRockKey && vaultRock) return vaultRockSrv;
    if (vaultRock) discard(vaultRock);
    vaultRock = nullptr;
    vaultRockKey = 0;
    const int side = rock.patch.width;
    int levels = 0;  // those of the sizes the card expects (each half of the one before, 1 at least)
    while (size_t(levels) < rock.patch.levels.size() &&
           rock.patch.levels[size_t(levels)].size() ==
               size_t(std::max(side >> levels, 1)) * size_t(std::max(side >> levels, 1)))
        ++levels;
    if (levels == 0) return kNoSrv;
    vaultRock = createTexture2D(device, side, side, levels, DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_NONE,
                                D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
    if (vaultRock && vaultRockSrv == kNoSrv) vaultRockSrv = allocSrv();
    if (!vaultRock || vaultRockSrv == kNoSrv) {
        LOG_ERROR("3D sky: no memory for the vault's rock (%dx%d) - the vault's own stone", side, side);
        releaseCom(vaultRock);
        vaultRockFailed = true;  // not tried again
        return kNoSrv;
    }
    nameResource(vaultRock, "vault's rock");
    vaultRockState = D3D12_RESOURCE_STATE_COPY_DEST;
    device->CreateShaderResourceView(vaultRock, nullptr, cpuSrvHandle(vaultRockSrv));
    flushDraws();  // the draws gathered so far before the copies
    for (int l = 0; l < levels; ++l) {
        const int size = std::max(side >> l, 1);
        const UINT row = UINT(size) * 4, pitch = (row + 255u) & ~255u;  // rows 256 bytes apart
        const Upload u = allocUpload(UINT64(pitch) * UINT(size), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (!u.cpu) {  // the next frame again
            discard(vaultRock);
            vaultRock = nullptr;
            return kNoSrv;
        }
        for (int y = 0; y < size; ++y)
            std::memcpy(u.cpu + size_t(y) * pitch, rock.patch.levels[size_t(l)].data() + size_t(y) * size_t(size),
                        row);
        D3D12_TEXTURE_COPY_LOCATION src{}, to{};
        src.pResource = slots[slot].upload;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = u.offset;
        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        src.PlacedFootprint.Footprint.Width = UINT(size);
        src.PlacedFootprint.Footprint.Height = UINT(size);
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = pitch;
        to.pResource = vaultRock;
        to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        to.SubresourceIndex = UINT(l);
        list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
    }
    transition(vaultRock, vaultRockState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    vaultRockKey = key;
    vaultRockLevels = levels;
    LOG_INFO("3D sky: %s's vault in the map's own rock - a patch of %dx%d texels with %d levels, %.1f units of the "
             "world across, laid as a mosaic ([view3d] vault_rock)", mapName.c_str(), side, side, levels,
             double(float(side) / std::max(rock.texels, 1e-3f)));
    return vaultRockSrv;
}

UINT Renderer::Impl::vaultTexture() {
    // 3.0.20-beta.52: a dungeon's vault ([view3d] vault) or, in a house or a hall, a ceiling ([view3d] ceiling)
    const bool dungeon = cave_vault::forMap(mapName);
    const bool wanted = dungeon ? effects.caveVault
                                : effects.ceiling3d && cave_vault::ceilingFor(mapName, mapOutdoors);
    if (vaultFailed || !wanted) return kNoSrv;
    const uint64_t key = mapWorld.laidKey();  // the ground read and its axes known
    if (!key || !mapWorld.ground()) return kNoSrv;
    if (key != vaultKey) {
        vaultKey = key;
        vaultUploaded = false;
        // 3.0.20-beta.46 (R-3T9MZC «колоны как будто в воздухе»): down to the posts that reach its walls' height
        size_t met = 0;
        if (!dungeon) {
            vaultMade = cave_vault::buildCeiling(*mapWorld.ground(), mapWorld.wallsHeight(), mapWorld.rooms(),
                                                 vaultMap);
            if (vaultMade)
                LOG_INFO("3D sky: %s - a ceiling at %.0f over its floors %.0f (its walls' top %.0f, by its models "
                         "%.0f), %dx%d heights", mapName.c_str(), vaultMap.ceiling, vaultMap.floorLevel,
                         vaultMap.wallTop, mapWorld.wallsHeight(), vaultMap.width, vaultMap.height);
        } else {
            vaultMade = cave_vault::build(*mapWorld.ground(), mapWorld.posts(), vaultMap, &met);
        }
        if (vaultMade && dungeon)
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
