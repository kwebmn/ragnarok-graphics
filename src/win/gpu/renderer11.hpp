#pragma once
#include <windows.h>
#include <dxgiformat.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/critters.hpp"
#include "core/draw_batch.hpp"
#include "core/footsteps.hpp"
#include "core/ground_cells.hpp"
#include "core/living.hpp"
#include "core/seams.hpp"
#include "core/seasons.hpp"
#include "core/smooth_shade.hpp"
#include "core/haze.hpp"
#include "core/outdoors.hpp"
#include "core/weather.hpp"
#include "core/wind.hpp"
#include "win/gpu/map_world.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/trees.hpp"
#include "core/map_objects.hpp"
#include "core/torch.hpp"
#include "core/retry_timer.hpp"
#include "win/gpu/frame_pacer.hpp"
#include "win/gpu/renderer.hpp"
#include "win/gpu/shaders.hpp"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11Buffer;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;
struct ID3D11Query;
struct IDXGIFactory1;
struct IDXGISwapChain;

namespace uagfx::gpu {

class Ai11;

// 0.46: the game drawn with Direct3D 11 - for graphics cards without Direct3D 12 (Radeon HD 5000-7000, GeForce GTX
// 400-600 with old drivers, Intel HD 4000) and Windows 7 and 8. The same shaders as Direct3D 12's (shaders.cpp): the
// game's fixed function, the target at the window's size with the interface's filter, crisp text's twins, the
// picture fitted into the window with its sharpening; no DLSS or effects yet. Feature level 11_0 takes shader model 5,
// 10_0 and 10_1 shader model 4 (when the shaders compile for it). 0.47: the AI textures' ready pictures (ai11.hpp).
class Renderer11 {
public:
    Renderer11() = default;
    ~Renderer11();
    Renderer11(const Renderer11&) = delete;
    Renderer11& operator=(const Renderer11&) = delete;

    // The device and its shaders - read from shaderCache when a run kept them, else compiled and kept there; the
    // start screen is told of them.
    bool create(const std::string& shaderCache, Renderer::StartSink sink, void* sinkContext, std::string& error);
    // 2.2.1: the card's work a frame, the last half second; false: not measured
    bool timing(GpuTiming& out) const {
        out = recent_;
        return recent_.gpuMs >= 0.0;
    }
    std::string adapterName() const { return adapter_; }
    bool setTarget(int width, int height);
    bool setScale(double k, int targetWidth, int targetHeight);
    uint32_t createTexture(int width, int height, int levels);
    void releaseTexture(uint32_t id);
    void execute(const FrameRecord& record);
    bool readTarget(std::vector<uint32_t>& argb);
    void present(HWND window, int outputWidth, int outputHeight, const Rect& source, const PresentOptions& options);
    void saveScreenshot(const std::string& path);  // 0.54: the back buffer as it is now, to a PNG
    void setEffects(const EffectSettings& e) { effects_ = e; }  // 0.59: the colours (grade) are drawn here
    void setMap(const std::string& map) {  // 0.79: the region's look; 1.7: the trees' season
        map_ = map;
        season_ = trees::seasonOf(map, trees::monthNow());
        look_ = haze::lookOf(map);  // 2.5: its haze and fog, the open sky, its weather (as Direct3D 12's setMap)
        outdoors_ = outdoors(map);
        desert_ = desert(map);  // 2.5.2: the prints in the sand
        footTracker_.clear();
        lavaLogged_ = false;
        place_ = living::placeOf(map, outdoors_, season_ == trees::Season::Autumn);  // 2.5.4: as Direct3D 12's
        year_ = seasons::of(map, trees::monthNow(), outdoors_, place_);
        waterLevelKnown_ = false;
        groundLifeLogged_ = grassLogged_ = false;
        critterWorld_.setPlace(place_);  // 2.5.4: a new map - everything goes
        crittersTime_ = 0.0f;
        weather_ = weather::of(map);
        hazeLogged_ = skyLogged_ = weatherLogged_ = cloudsLogged_ = false;
        mapWorld_.setMap(map);  // 2.5.1: its file - the lamps' lights
        if (!map.empty()) fadeStart_ = waterTime();  // 2.5.2: the world comes in from black (0.83)
    }
    void setGameDir(const std::string& dir) { mapWorld_.setGameDir(dir); }  // 2.5.1: the map files
    void noteObjectTexture(uint32_t id, uint8_t kind) { objectTextures_.note(id, objects::Kind(kind)); }  // 0.61
    void setUiSharp(bool sharp) { uiSharp_ = sharp; }
    void setStateCache(bool on) { stateCache_ = on; }  // 0.88: [debug] state_cache
    void setMergeDraws(bool on) { mergeDraws_ = on; }  // 0.90: [debug] merge_draws
    void setCamera(bool valid, const float view[12], float targetZ = 0);  // 0.99: the game's camera (the modern water)
    void setAnisotropy(int anisotropy) { anisotropy_ = anisotropy; }
    // 0.47: the AI textures from the texture pack (none made); false: off, the reason in the log.
    bool startAi(const std::string& gameDir, const AiSettings& settings);
    // Pause: the game's own textures (0); 2.2.1: the kinds shown; 2.2.2: the sprites' kinds shown
    void setAiShown(uint8_t kinds, uint8_t sprites);
    void requestIcon(uint64_t key, int width, int height, const uint32_t* argb);
    void setIconSink(Renderer::IconSink sink, void* context);
    void finish();
    bool lost() const { return lost_; }

private:
    struct Texture {
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0, height = 0, levels = 1;
        std::vector<SpriteCell> cells;  // 0.60.1: a sprite cache's cells and what each holds (the shadows)
        bool leafy = false;             // 0.64: leaves (core/foliage.hpp): swayed in the world
        // 2.5.1: how much its picture looks like fire and its colour, or lights in its own colour (core/effect_light)
        float fire = 0.0f, fireColour[3] = {0, 0, 0};
        float light = 0.0f, lightColour[3] = {0, 0, 0};
        // 1.14: its material (materials11.cpp) and the picture it was made from (the game's or the AI's)
        ID3D11Texture2D* mat = nullptr;
        ID3D11ShaderResourceView* matSrv = nullptr;
        const void* matFor = nullptr;
    };
    struct Target {  // a picture drawn into and read from
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0, height = 0;
    };
    // 1.0: the glow of the effects (glow11.cpp, as Direct3D 12's effects.cpp): from the world's first draw that adds
    // light the draws write that light into a second target too; at the interface it is shrunk level by level,
    // blurred back up and added onto the picture.
    static constexpr int kGlowLevels11 = 8;
    bool glowWanted() const {
        return effects_.glow && effects_.glowStrength > 0 && drawGlowPs_ && glowDownPs_ && glowUpPs_ && glowAddPs_;
    }
    bool startGlow();
    void addGlow();
    void glowPass(ID3D11PixelShader* ps, ID3D11ShaderResourceView* src, int srcW, int srcH, ID3D11RenderTargetView* dst,
                  int dstW, int dstH, ID3D11BlendState* blend, float factor, float strength, bool first);
    ID3D11PixelShader* drawGlowPs_ = nullptr;
    ID3D11PixelShader* drawLeanPs_ = nullptr;  // 2.5: draw_ps / draw_glow_ps without the world's extras
    ID3D11PixelShader* drawGlowLeanPs_ = nullptr;
    ID3D11PixelShader* waterGlowPs_ = nullptr;
    ID3D11PixelShader* glowDownPs_ = nullptr;
    ID3D11PixelShader* glowUpPs_ = nullptr;
    ID3D11PixelShader* glowAddPs_ = nullptr;
    Target glow_;
    Target glowChain_[kGlowLevels11];
    int glowLevels_ = 0;
    bool glowActive_ = false;  // the world's draws write into glow_ too
    bool glowDrawn_ = false;   // ... and some did this frame
    bool glowFailed_ = false;  // not made at this size
    bool makeTarget(Target& t, int width, int height, DXGI_FORMAT format);
    // 1.0.1: the game's fireflies drawn anew (torches11.cpp, firefly_ps as Direct3D 12's): taken from its draws, each
    // a soft blinking light added at the interface, behind what stands in front of it
    struct Fly11 {
        float x, y, rhw, size, alpha, colour[3], seed;
    };
    std::vector<Fly11> flies_;
    ID3D11PixelShader* fireflyPs_ = nullptr;
    // 1.14, the materials (materials11.cpp, as Direct3D 12's materials.cpp): made from a texture's picture by
    // material_ps, a few a frame, then read by the ground's and the models' draws (t2, shade() bit 28)
    ID3D11PixelShader* materialPs_ = nullptr;
    int materialsMade_ = 0;
    bool materialsLogged_ = false;
    bool materialsWanted() const { return effects_.materials && effects_.materialsStrength > 0 && materialPs_; }
    bool ensureMaterial(Texture& t, ID3D11ShaderResourceView* source);
    static void dropMaterial(Texture& t);
    void materialConstants(bool ground, Constants& c) const;
    bool firefliesWanted() const { return effects_.fireflies && effects_.firefliesBrightness > 0 && fireflyPs_; }
    bool takeFirefly(const FrameRecord& rec, const DrawCmd& d);
    void applyFireflies();
    void applyGrade();  // 0.59
    void applyNight();  // 0.63
    // 2.5: the haze and the low fog, the sky past the map's edge, the map's weather (or the motes) - Direct3D 12's
    // haze.cpp and weather.cpp passes by the same shaders, with the game's camera (setCamera) and the depth (t3)
    void applyHaze();
    void applySky();
    void applyWeather();
    // 2.5.1: the soft shadows where surfaces meet (ao.cpp: a mask at a half or a quarter of the size, blurred twice,
    // multiplied in) and the clouds' shadows (clouds.cpp: their mask, multiplied in) - before the characters
    void applyAo();
    void applyClouds();
    // 2.5.2: the bloom, the filmic curve, the film's grain and the new map's fade (ao.cpp applyBloom: the bright
    // parts at a quarter of the size, blurred twice each way, added) - before the colours
    void applyBloom();
    float mapFadeNow();  // 1 when done
    // 2.5.2: the steps the characters left - prints in the snow and the sand, dust on dry ground (ao.cpp applySteps)
    // - and the rings on the water where they go (water_ps), the feet followed from the game's round shadows
    void applySteps();
    void noteFoot(const FrameRecord& rec, const DrawCmd& d);
    int stepKind() const {  // 1 snow, 2 sand, 3 dust, 0 none
        if (!effects_.footprints || !stepsPs_) return 0;
        return weather_ == weather::Kind::Snow ? 1 : desert_ ? 2 : outdoors_ && effects_.stepDust ? 3 : 0;
    }
    static constexpr float kRippleLife = 1.6f;
    bool ripplesWanted() const { return effects_.ripples && effects_.water && waterPs_; }
    float stepLife() const {
        const float prints = stepKind() == 3 ? 0.9f : stepKind() == 1 ? 25.0f : stepKind() ? 14.0f : 0.0f;
        return std::max(std::max(prints, ripplesWanted() ? kRippleLife : 0.0f),
                        effects_.birds || effects_.breath || effects_.splashes ? 2.0f : 0.0f);
    }
    // 2.5.2: the chimneys' smoke (smoke_ps) and the lava (lava_ps) drawn by their own shaders, the lava lighting
    // around it in the torches' pass (draw.cpp, torches.cpp)
    bool smokeWanted() const { return effects_.smoke && smokePs_ && smokeGlowPs_; }
    bool lavaWanted() const { return effects_.lava && lavaPs_ && lavaGlowPs_; }
    bool lavaMap() const {
        const rsw::World* w = mapWorld_.world();
        return (w && w->waterType == 4) || map_ == "mag_dun01" || map_ == "mag_dun02" || map_ == "tak_run";
    }
    void noteLava(const FrameRecord& rec, const DrawCmd& d);
    // 2.5.3: the hot air shimmering (heat_ps, heat.cpp) over the flames, the burning skills and the lava, the
    // portals' air turning, the desert far off - its spots gathered by the torches' pass
    struct HeatSpot11 {
        float x, y, radius, strength;  // target pixels; below 0 a portal's swirl
    };
    std::vector<HeatSpot11> heatSpots_;
    void applyHeat();
    bool heatWanted() const { return effects_.heat && effects_.heatStrength > 0 && heatPs_; }
    float heatUser() const { return float(std::clamp(effects_.heatStrength, 0, 200)) / 100.0f; }
    // 2.5.3: the portals (torches.cpp notePortal): their draws gathered into what stands there, followed from frame
    // to frame - a soft blue light about each and the air turning in it
    bool portalsWanted() const { return effects_.portals && effects_.portalsStrength > 0; }
    void notePortal(const FrameRecord& rec, const DrawCmd& d);
    struct PortalTrack11 {
        float view[3];
        float gx, gy, size, strength;
    };
    std::vector<PortalTrack11> portalTracks_;
    float portalTime_ = 0.0f;
    // 2.5.3: the forest's beams of light (ray_ps, draw.cpp 0.30) - their dust and light where they fall
    bool raysWanted() const { return effects_.rays && rayPs_ && rayGlowPs_; }
    struct RayPart11 {
        float middle[3], foot[3];
        float gameX, gameY;
    };
    std::vector<RayPart11> rayParts_;
    void noteRay(const FrameRecord& rec, const DrawCmd& d);
    // 2.5.3: the skills' sparks about the places the torches' pass gathered the skills' fire at (torches.cpp
    // applyEmbers), added; the floors' reflections indoors and in the rain (haze.cpp applyFloorReflections: at half
    // the size, then laid over)
    struct Ember11 {
        float x, y, size, strength, colour[3];
        bool fire;
    };
    std::vector<Ember11> embers_;
    void applyEmbers();
    void applyFloorReflections();
    float floorReflectAmount() const {
        if (!effects_.floorReflections || !floorReflectPs_ || !halfOverPs_) return 0.0f;
        const float user = float(std::clamp(effects_.floorReflectStrength, 0, 100)) / 100.0f;
        return weather_ == weather::Kind::Rain && effects_.weather ? std::min(1.0f, user * 1.6f)
               : outdoors_                                          ? 0.0f
                                                                    : user;
    }
    // 2.5.4: the living ground (ao.cpp applyGroundLife: sand drifting over the desert, snow glittering, a damp cave's
    // walls glistening, the wet sand along the shore, the season's snow and grass, wet in the rain) and the grass
    // (applyGrass: on the map's ground only - its cells from the frame's ground triangles), before the characters
    void applyGroundLife();
    void applyGrass();
    // 2.5.4: the living world's creatures and particles (critters.cpp: core/critters.hpp moved on, turned to the
    // screen, drawn over the world by critters_vs and critters_ps after the torches)
    void applyCritters();
    void noteCrown(const FrameRecord& rec, const DrawCmd& d);
    critters::Switches critterSwitches() const {
        critters::Switches on;
        on.butterflies = effects_.butterflies;
        on.dragonflies = effects_.butterflies && effects_.dragonflies;
        on.birds = effects_.birds;
        on.leaves = effects_.leafFall;
        on.embers = effects_.embers;
        on.breath = effects_.breath;
        on.splashes = effects_.splashes;
        on.drips = effects_.caveDrips;
        on.underwater = effects_.underwater;
        return on;
    }
    bool crittersNeedFeet() const { return effects_.birds || effects_.breath || effects_.splashes; }
    // 2.5.5: the sun (actor_shadows.cpp): the way to it from the map's file, else the day's; on the screen
    bool mapSun(float toSun[3]) const;
    float sunHeight() const;
    void sunGround(float out[3]) const;
    void sunOnScreen(float out[2]) const;
    // 2.5.5: the light from the sky and from around (ao.cpp applySkyLight), before the soft shadows; the light in
    // the air about the lights and from the sky (haze.cpp applyShafts), the sun's flare (applyFlare); the depth of
    // field (ao.cpp applyDof), after the colours; the characters lit by the sun from their sides (sun_map.cpp
    // spriteConstants); the shared wind (weather.cpp updateWind) the weather, the clouds, the sky, the grass, the
    // sand and the leaves go with
    void applySkyLight();
    void applyShafts();
    void applyFlare();
    void applyDof();
    void applyFog();  // 2.5.5: the volumetric fog low over the fields (sun_map.cpp applyFog) - the remaster's
    void spriteConstants(Constants& c) const;
    void updateWind();
    bool windShared() const { return effects_.sharedWind && cameraValid_; }
    void noteGround(const FrameRecord& rec, const DrawCmd& d);
    void noteWaterLevel(const FrameRecord& rec, const DrawCmd& d);
    bool seasonsShown() const {
        return effects_.seasons && ((effects_.seasonSnow && year_.snow > 0.0f) ||
                                    (effects_.seasonGrass && (year_.dry > 0.0f || year_.lush > 0.0f)));
    }
    bool rainWetShown() const { return effects_.rainWet && effects_.weather && weather_ == weather::Kind::Rain; }
    bool wetSandWanted() const { return effects_.shoreFoam && effects_.wetSand && waterLevelKnown_ && outdoors_; }
    bool groundLifeWanted() const {
        return groundLifePs_ && ((effects_.sandDrift && place_.sand) || (effects_.snowSparkle && place_.snow) ||
                                 (effects_.wetWalls && place_.cave) || wetSandWanted() || seasonsShown() ||
                                 rainWetShown());
    }
    bool grassWanted() const {
        return effects_.grass && effects_.grassAmount > 0 && grassPs_ && outdoors_ && cameraValid_;
    }
    bool wetWanted() const {
        return effects_.wetGround && effects_.weather && weather_ == weather::Kind::Rain && floorReflectPs_ &&
               halfOverPs_;
    }
    bool bloomWanted() {
        return (effects_.bloom || effects_.filmic || (effects_.filmGrain && effects_.filmGrainAmount > 0) ||
                mapFadeNow() < 1.0f) &&
               bloomDownPs_ && dofBlurPs_ && bloomApplyPs_;
    }
    bool aoWanted() const { return effects_.ao && effects_.aoStrength > 0 && aoPs_ && aoBlurPs_ && aoApplyPs_; }
    bool cloudsWanted() const {
        return effects_.clouds && effects_.cloudsStrength > 0 && cloudsPs_ && cloudsApplyPs_ && outdoors_;
    }
    // A pass into a small target of its own: t0 and the depth (t3) read, the whole target written
    void maskPass(ID3D11PixelShader* ps, const Target& into, const Constants& c, ID3D11ShaderResourceView* t0,
                  ID3D11ShaderResourceView* depth, ID3D11ShaderResourceView* t2 = nullptr,
                  ID3D11ShaderResourceView* t1 = nullptr);
    void passSamplers();  // linear, clamped, at s0 and s1 - the passes' own
    // A pass over the whole picture: copy - 1 the picture copied now into t2 (sceneColor), 2 its copy as it is, 0
    // none; blend - 0 written whole, 1 blended over by its alpha, 2 multiplied in, 3 added, 4 premultiplied over;
    // depth - bound as t3 (sceneDepth); t0, t1 - bound as the first two textures
    void worldPass(ID3D11PixelShader* ps, const Constants& c, int copy = 1, int blend = 0,
                   ID3D11ShaderResourceView* depth = nullptr, ID3D11ShaderResourceView* t0 = nullptr,
                   ID3D11ShaderResourceView* t1 = nullptr, const RECT* clip = nullptr,  // D3D11_RECT
                   ID3D11ShaderResourceView* t5 = nullptr);  // 2.5.4: the grass's ground cells
    // 0.60.1: the characters' shadows (actor_shadows11.cpp, as Direct3D 12's actor_shadows.cpp - the sun's light
    // alone: no map's lamps without the camera)
    bool shadowsWanted() const {
        return effects_.actorShadows && effects_.actorShadowsStrength > 0 && shadowVs_ && shadowPs_ && shadowBlurPs_ &&
               shadowApplyPs_ && shadowLayout_;
    }
    void applyActorShadows(const FrameRecord& rec, size_t from);
    bool ensureShadowMask(int w, int h);
    // 0.61: the torches' light (torches11.cpp, as Direct3D 12's torch_light_ps): the flames the game draws noted,
    // their light onto the ground before the colours; the flames and their glow stay the game's.
    void noteTorch(const FrameRecord& rec, const DrawCmd& d);
    void applyTorchLight();
    bool torchesWanted() const { return effects_.torch && effects_.torchLight > 0 && torchLightPs_; }
    // 2.5.2: the new flame and its glow (torch_fire_ps) in place of the game's glow - and of its flame with
    // torch_flame on (torches.cpp takeTorch); true: the game's draw is not made
    bool flamesWanted() const { return effects_.torch && torchFirePs_; }
    bool takeTorch(const FrameRecord& rec, const DrawCmd& d);
    ID3D11PixelShader* torchFirePs_ = nullptr;
    ID3D11PixelShader* dustPs_ = nullptr;  // 2.5.2: the maps' clouds of dust (core/map_glows.hpp)
    // 2.5.1: the fire of skills lights around it (torches.cpp noteFire): a draw that adds a fire-coloured picture,
    // or (skill_colours) one that lights in its own colour - its middle, depth, strength and colour, gathered by
    // place in the torches' light pass
    bool fireWanted() const { return effects_.fireLight && effects_.fireLightStrength > 0 && torchLightPs_; }
    struct FireSpark11 {
        float view[3];
        float strength;
        float colour[3];
        float gameX, gameY;
    };
    std::vector<FireSpark11> fireSparks_;  // this frame's
    void noteFire(const FrameRecord& rec, const DrawCmd& d, float amount, const float colour[3]);
    bool compileShaders(const std::string& cacheFile, Renderer::StartSink sink, void* sinkContext, std::string& error);
    bool makeTarget(Target& t, int width, int height) {
        return makeTarget(t, width, height, DXGI_FORMAT_B8G8R8A8_UNORM);
    }
    void dropTarget(Target& t);
    bool makeDepth(int width, int height, ID3D11Texture2D*& depth, ID3D11DepthStencilView*& dsv);
    bool makeStaging(int width, int height);
    bool uploadGeometry(const FrameRecord& rec);
    void clear(const ClearCmd& c);
    void applyViewport();
    void upload(const FrameRecord& rec, const UploadCmd& u);
    void writeTarget(const FrameRecord& rec, const WriteTargetCmd& w);
    void draw(const FrameRecord& rec, const DrawCmd& d);
    // srcRect of a picture (its pixels) into dstRect of a target: bilinear when enlarged, the footprint's average
    // when shrunk; sharpen 0-1 sharpens (CAS), below 0 does not.
    void blit(ID3D11ShaderResourceView* src, int srcWidth, int srcHeight, const float srcRect[4],
              ID3D11RenderTargetView* dst, const Rect& dstRect, int dstWidth, int dstHeight, float sharpen);
    void bindTarget();  // the target, its depth, the game's viewport: for the game's draws
    void setConstants(const Constants& c);
    ID3D11BlendState* blendState(bool enable, uint32_t src, uint32_t dst, uint8_t writeMask);
    ID3D11DepthStencilState* depthState(bool enable, bool write, uint32_t func);
    ID3D11RasterizerState* rasterState(uint32_t cull);
    ID3D11SamplerState* samplerState(const StageSetup& s, int aniso);
    bool ensureSwapChain(HWND window, UINT width, UINT height, bool wantTearing);
    void releaseSwapChain();
    void failed(const char* what, long hr);
    // 0.46.1: the GPU's time a frame, as Direct3D 12 logs it - timestamps around each record's work and the
    // picture's, summed (the GPU waits for the game between them); read 4 frames on, never waited for.
    void timeBegin();
    void timeEnd();
    void timeFrame();
    bool readTimes(UINT slot, bool wait);
    void logGpu(ULONGLONG now);

    HMODULE dll_ = nullptr;
    Ai11* ai_ = nullptr;  // 0.47
    Renderer::IconSink iconSink_ = nullptr;
    void* iconContext_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* ctx_ = nullptr;
    unsigned level_ = 0;  // D3D_FEATURE_LEVEL
    IDXGIFactory1* factory_ = nullptr;
    std::string adapter_;
    bool bgraVertices_ = true;  // B8G8R8A8 vertex colours: else they are turned into R8G8B8A8 on the way
    bool lost_ = false;

    ID3D11VertexShader* drawVs_ = nullptr;
    ID3D11PixelShader* drawPs_ = nullptr;
    // 0.99: the modern water (water_vs/water_ps as Direct3D 12's): the picture and the depth as they were before the
    // frame's first water (scene_, depthCopy_), the game's camera backwards
    ID3D11VertexShader* waterVs_ = nullptr;
    ID3D11PixelShader* waterPs_ = nullptr;
    ID3D11Texture2D* depthCopy_ = nullptr;
    ID3D11DepthStencilView* depthCopyDsv_ = nullptr;
    ID3D11ShaderResourceView* depthCopySrv_ = nullptr;
    bool waterCopied_ = false;  // this frame's copies made (or could not be)
    bool waterReady_ = false;   // ... and the water is drawn from them
    bool cameraValid_ = false;
    float viewToWorld_[12] = {};
    LARGE_INTEGER waterClock_{};
    unsigned long long waterCells_ = 0;
    float waterTime();
    bool copyForWater();
    ID3D11VertexShader* presentVs_ = nullptr;
    ID3D11PixelShader* presentPs_ = nullptr;
    ID3D11PixelShader* gradePs_ = nullptr;  // 0.59
    ID3D11VertexShader* shadowVs_ = nullptr;  // 0.60.1
    ID3D11PixelShader* shadowPs_ = nullptr;
    ID3D11PixelShader* shadowBlurPs_ = nullptr;
    ID3D11PixelShader* shadowApplyPs_ = nullptr;
    ID3D11InputLayout* shadowLayout_ = nullptr;
    ID3D11Buffer* shadowVb_ = nullptr;
    size_t shadowVbBytes_ = 0;
    ID3D11ShaderResourceView* depthSrv_ = nullptr;
    ID3D11Texture2D* depthSrvOf_ = nullptr;  // the depth buffer depthSrv_ reads
    // 0.67, soft particles: the depth tested but not written, while the effect reads it (t3)
    ID3D11DepthStencilView* readOnlyDsv_ = nullptr;
    ID3D11Texture2D* readOnlyDsvOf_ = nullptr;
    bool depthViews();  // depthSrv_ and readOnlyDsv_ for depth_; false when they cannot be made
    ID3D11BlendState* maxBlend_ = nullptr;   // the mask: the darkest shadow
    ID3D11BlendState* mulBlend_ = nullptr;   // onto the picture: multiplied
    Target shadowMask_[2];
    bool groundDrawn_ = false, shadowsDone_ = false, shadowsLogged_ = false;
    // 0.61: the torches' light
    ID3D11PixelShader* torchLightPs_ = nullptr;
    ID3D11PixelShader* nightPs_ = nullptr;  // 0.63
    bool nightDone_ = false;
    ID3D11Texture2D* lightData_ = nullptr;  // two texels a light (torch_light_ps's t1)
    ID3D11ShaderResourceView* lightDataSrv_ = nullptr;
    objects::Textures objectTextures_;
    std::vector<torch::Flame> flames_;  // this frame's
    bool lightDone_ = false, lightLogged_ = false;
    // 2.5: the haze, the sky and the weather (applyHaze, applySky, applyWeather)
    ID3D11PixelShader* hazePs_ = nullptr;
    ID3D11PixelShader* skyPs_ = nullptr;
    ID3D11PixelShader* weatherPs_ = nullptr;
    bool hazeDone_ = false, skyDone_ = false, weatherDone_ = false;
    bool hazeLogged_ = false, skyLogged_ = false, weatherLogged_ = false;
    haze::Look look_;  // the map's (setMap)
    bool outdoors_ = false;
    weather::Kind weather_ = weather::Kind::None;
    float cameraTargetZ_ = 0.0f;  // the view depth of what the camera looks at (setCamera), 0 unknown
    // 2.5.1: the soft shadows and the clouds' shadows; the map's file (the lamps' lights, torches11.cpp)
    ID3D11PixelShader* aoPs_ = nullptr;
    ID3D11PixelShader* aoBlurPs_ = nullptr;
    ID3D11PixelShader* aoApplyPs_ = nullptr;
    ID3D11PixelShader* cloudsPs_ = nullptr;
    ID3D11PixelShader* cloudsApplyPs_ = nullptr;
    Target aoTarget_[2];
    Target cloudsMask_;
    bool aoDone_ = false, cloudsDone_ = false, aoLogged_ = false, cloudsLogged_ = false;
    MapWorld mapWorld_;
    // 2.5.2: the bloom's two quarter-size targets and shaders; the new map's fade
    ID3D11PixelShader* bloomDownPs_ = nullptr;
    ID3D11PixelShader* dofBlurPs_ = nullptr;
    ID3D11PixelShader* bloomApplyPs_ = nullptr;
    Target bloom_[2];
    bool bloomDone_ = false, bloomLogged_ = false;
    float fadeStart_ = -1.0f;  // the effects' clock when the map changed; below 0 none
    // 2.5.2: the footprints and the ripples; the chimneys' smoke and the lava
    ID3D11PixelShader* stepsPs_ = nullptr;
    ID3D11Texture2D* stepsData_ = nullptr;  // two texels a step (steps_ps's t1)
    ID3D11ShaderResourceView* stepsDataSrv_ = nullptr;
    footsteps::Tracker footTracker_;
    std::vector<map::Point> frameFeet_;  // the round shadows' middles drawn this frame, in the world
    bool stepsDone_ = false, stepsLogged_ = false, desert_ = false;
    ID3D11PixelShader* smokePs_ = nullptr;
    ID3D11PixelShader* smokeGlowPs_ = nullptr;
    ID3D11PixelShader* lavaPs_ = nullptr;
    ID3D11PixelShader* lavaGlowPs_ = nullptr;
    std::vector<FireSpark11> lavaCells_;  // this frame's cells of lava: their middles light around them
    std::vector<FireSpark11> portalSparks_;  // 2.5.3: this frame's portal draws (strength: their size, game pixels)
    bool smokeLogged_ = false, lavaLogged_ = false;
    // 2.5.3: the heat haze, the forest's beams
    ID3D11PixelShader* heatPs_ = nullptr;
    ID3D11Texture2D* heatData_ = nullptr;  // a texel a spot (heat_ps's t1)
    ID3D11ShaderResourceView* heatDataSrv_ = nullptr;
    bool heatDone_ = false, heatLogged_ = false, raysLogged_ = false;
    ID3D11PixelShader* rayPs_ = nullptr;
    ID3D11PixelShader* rayGlowPs_ = nullptr;
    // 2.5.3: the skills' sparks, the floors' reflections, the rain's splashes
    ID3D11PixelShader* embersPs_ = nullptr;
    ID3D11Texture2D* embersData_ = nullptr;  // two texels a place (embers_ps's t1)
    ID3D11ShaderResourceView* embersDataSrv_ = nullptr;
    ID3D11PixelShader* floorReflectPs_ = nullptr;
    ID3D11PixelShader* halfOverPs_ = nullptr;
    Target floor_;
    bool floorDone_ = false, floorLogged_ = false;
    ID3D11PixelShader* splashPs_ = nullptr;
    // 2.5.4: the living ground, the grass
    ID3D11PixelShader* groundLifePs_ = nullptr;
    ID3D11PixelShader* grassPs_ = nullptr;
    living::Place place_;
    seasons::Look year_;
    float waterLevel_ = 0.0f;  // the last water seen, its height in the world (up is -y)
    bool waterLevelKnown_ = false, waterLevelNoted_ = false;
    std::vector<seams::Triangle> groundTris_;  // this frame's ground in the world, for the next frame's cells
    ground_cells::Grid groundCells_;
    ID3D11Texture2D* groundCellsData_ = nullptr;  // R32G32: each cell's lowest and highest y
    ID3D11ShaderResourceView* groundCellsSrv_ = nullptr;
    bool groundCellsUp_ = false;  // this frame's grid in the texture
    bool groundLifeDone_ = false, grassDone_ = false, groundLifeLogged_ = false, grassLogged_ = false;
    // 2.5.4: the living world
    ID3D11VertexShader* crittersVs_ = nullptr;
    ID3D11PixelShader* crittersPs_ = nullptr;
    ID3D11Texture2D* crittersData_ = nullptr;  // four texels a sprite (critters_vs's and critters_ps's t1)
    ID3D11ShaderResourceView* crittersDataSrv_ = nullptr;
    critters::World critterWorld_;
    std::vector<critters::Crown> frameCrowns_;   // this frame's trees in sight (noteCrown)
    std::vector<critters::Point> critterFires_;  // this frame's flames' tips (the torches' pass)
    float crittersTime_ = 0.0f, crittersStepsSeen_ = 0.0f;
    bool crittersLogged_ = false;
    // 2.5.5: the sky's light, the light in the air, the flare, the depth of field, the shared wind
    ID3D11PixelShader* skyLightPs_ = nullptr;
    ID3D11PixelShader* shaftsPs_ = nullptr;
    ID3D11PixelShader* flarePs_ = nullptr;
    ID3D11PixelShader* dofDownPs_ = nullptr;
    ID3D11PixelShader* dofFocusPs_ = nullptr;
    ID3D11PixelShader* dofApplyPs_ = nullptr;
    ID3D11PixelShader* fogLightPs_ = nullptr;
    bool fogDone_ = false, fogLogged_ = false;
    Target dof_[2];
    Target dofFocus_[2];  // 1x1 R32: the focus, last frame's and this one's
    int dofFocusNow_ = 0;
    LARGE_INTEGER dofFocusTime_{};
    int shaftLights_ = 0;  // the lights of this frame's torches' pass (lightData_)
    Target shafts_;        // 2.5.7: the light in the air at half the size
    bool skyLightDone_ = false, shaftsDone_ = false, flareDone_ = false, dofDone_ = false;
    bool skyLightLogged_ = false, dofLogged_ = false;
    float windClock_ = -1.0f;
    wind::Wind windNow_;
    float windScreenX_ = 0.0f;
    wind::Drift weatherDrift_, cloudsDrift_, skyDrift_;
    std::vector<smooth_shade::RecordChange> smoothColours_;  // 2.5.5: this record's models' corners spread
    bool smoothLogged_ = false;
    unsigned long long clockStart_ = 0;
    ID3D11VertexShader* clearVs_ = nullptr;
    ID3D11PixelShader* clearPs_ = nullptr;
    ID3D11InputLayout* layout_[2] = {nullptr, nullptr};  // one UV set, two
    ID3D11Buffer* constants_ = nullptr;
    ID3D11Buffer* clearConstants_ = nullptr;
    ID3D11Buffer* vertices_ = nullptr;
    ID3D11Buffer* indices_ = nullptr;
    UINT vertexBytes_ = 0, indexBytes_ = 0;
    bool geometry_ = false;  // this record's vertices are in vertices_
    Texture white_;
    std::unordered_map<uint32_t, Texture> textures_;
    uint32_t nextTexture_ = 1;
    std::unordered_map<uint64_t, ID3D11BlendState*> blends_;
    std::unordered_map<uint64_t, ID3D11DepthStencilState*> depths_;
    std::unordered_map<uint64_t, ID3D11RasterizerState*> rasters_;
    std::unordered_map<uint64_t, ID3D11SamplerState*> samplers_;

    Target target_;  // the game's picture at the window's size (k times the game's)
    Target scene_;   // 0.59: its copy the colours are read from
    EffectSettings effects_;
    std::string map_;
    trees::Season season_ = trees::Season::Green;
    bool worldDrawn_ = false, gradeDone_ = false;  // this frame
    ID3D11Texture2D* depth_ = nullptr;
    ID3D11DepthStencilView* dsv_ = nullptr;
    Target game_;  // the game's size: its own pixels on the way in, the picture on the way out
    ID3D11Texture2D* staging_ = nullptr;
    int gameWidth_ = 0, gameHeight_ = 0;
    double scale_ = 1.0;
    ViewportCmd gameViewport_;
    bool targetBound_ = false;
    // 0.88: what the game's draws bound last - a draw calls only for what differs (an old graphics driver spends much
    // of the processor on each call, and on Direct3D 11 that is the game's own thread). Forgotten when the target is
    // bound anew: every other pass resets targetBound_ before it binds anything of its own.
    struct Bound {
        bool valid = false;
        ID3D11BlendState* blend = nullptr;
        ID3D11DepthStencilState* depth = nullptr;
        ID3D11RasterizerState* raster = nullptr;
        ID3D11SamplerState* samplers[2] = {nullptr, nullptr};
        ID3D11ShaderResourceView* srvs[2] = {nullptr, nullptr};
        ID3D11InputLayout* layout = nullptr;
        unsigned topology = 0;  // a D3D11_PRIMITIVE_TOPOLOGY (0 none)
        ID3D11Buffer* vertexBuffer = nullptr;  // bound at an offset of `phase` bytes with `stride`
        UINT stride = 0, phase = 0;
        ID3D11Buffer* indexBuffer = nullptr;
        ID3D11VertexShader* vs = nullptr;  // 0.99
        ID3D11PixelShader* ps = nullptr;
    } bound_;
    // 0.90: a draw call's state - the game's draws with the same one and their vertices one after another go to the
    // driver as one draw call (core/draw_batch.hpp); the views it reads are held while it is gathered
    struct Issue {
        Constants c{};
        ID3D11BlendState* blend = nullptr;
        ID3D11DepthStencilState* depth = nullptr;
        ID3D11RasterizerState* raster = nullptr;
        ID3D11SamplerState* samplers[2] = {nullptr, nullptr};
        ID3D11ShaderResourceView* srvs[3] = {nullptr, nullptr, nullptr};  // 1.14: [2] the material (t2)
        ID3D11InputLayout* layout = nullptr;
        ID3D11VertexShader* vs = nullptr;  // 0.99: the game's draw's, or the water's
        ID3D11PixelShader* ps = nullptr;
        uint32_t primitive = 0, firstIndex = 0, indexCount = 0;  // a draw by itself: its own
        bool sameState(const Issue& o) const;
    };
    void flushBatch();  // the gathered draw call to the driver
    Issue issue_;
    batch::Batch batch_;
    ID3D11Buffer* quadIndices_ = nullptr;  // batch::quadIndices: the quads as lists of triangles
    bool mergeDraws_ = true;
    // 0.88: what the constant buffer holds - the same constants are not written again
    Constants constantsNow_{};
    bool constantsKnown_ = false;
    bool stateCache_ = true;
    bool uiSharp_ = true;
    int anisotropy_ = 0;

    IDXGISwapChain* swap_ = nullptr;
    ID3D11RenderTargetView* backRtv_ = nullptr;
    HWND swapWindow_ = nullptr;
    UINT swapWidth_ = 0, swapHeight_ = 0, swapFlags_ = 0;
    UINT failedWidth_ = 0, failedHeight_ = 0;
    RetryTimer swapRetry_{2000};
    bool tearing_ = false;
    Rect picture_;
    FramePacer pacer_;  // 0.85: [frames] fps_limit

    static constexpr UINT kTimedFrames = 4;
    static constexpr UINT kStamps = 16;  // 8 spans a frame; more of them stretch the last
    struct FrameTimes {
        ID3D11Query* disjoint = nullptr;
        ID3D11Query* stamps[kStamps] = {};
        ID3D11Query* effect[15][2] = {};  // 0.89: the effects' own spans (EffectTime11; 2.5.5: fifteen)
        unsigned effectBegun = 0, effectEnded = 0;  // bits by EffectTime11
        UINT used = 0;          // stamps ended
        bool open = false;      // its disjoint begun: this frame is timed
        bool inSpan = false;    // a span begun, its end not yet
        bool pending = false;   // ended, its data not read yet
    };
    FrameTimes times_[kTimedFrames];
    // 0.89: the effects timed one by one on the card (the log's "GPU: effects a frame" line)
    enum EffectTime11 {
        kTimeShadows11, kTimeLight11, kTimeNight11, kTimeGrade11, kTimeHaze11, kTimeSky11, kTimeWeather11,
        kTimeAo11, kTimeClouds11, kTimeBloom11, kTimeSteps11, kTimeHeat11, kTimeSkyLight11, kTimeShafts11,
        kTimeDof11, kEffectTimes11
    };
    static_assert(kEffectTimes11 == 15, "FrameTimes::effect holds fifteen");
    void effectBegin(EffectTime11 e);  // once a frame each, in an open frame
    void effectEnd(EffectTime11 e);
    struct EffectSpan {  // the scope of an effect's pass
        Renderer11& r;
        EffectTime11 e;
        EffectSpan(Renderer11& renderer, EffectTime11 effect) : r(renderer), e(effect) { r.effectBegin(e); }
        ~EffectSpan() { r.effectEnd(e); }
        EffectSpan(const EffectSpan&) = delete;
        EffectSpan& operator=(const EffectSpan&) = delete;
    };
    double effectMs_[kEffectTimes11] = {};
    double recentMs_ = 0.0, recentEffectMs_[kEffectTimes11] = {};  // 2.2.1: the half second's, for timing()
    uint32_t recentFrames_ = 0;
    ULONGLONG recentSince_ = 0;
    GpuTiming recent_;
    // 0.89: what the frames ask of the processor and the driver, the log every half minute - draws, the driver's
    // calls made and spared, the processor's time in drawing; the game's textures made, reused, freed
    struct Stats {
        uint64_t frames = 0, draws = 0, calls = 0, spared = 0, made = 0, reused = 0, freed = 0, uploaded = 0;
        uint64_t batches = 0;  // 0.90: the draw calls the game's draws went in
        LONGLONG ticks = 0;
    } stats_;
    // 0.89: textures the game let go, kept for its next one of the same size and levels - it makes and frees small
    // ones again and again, and an old driver takes long to make each
    struct Pooled {
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0, height = 0, levels = 1;
    };
    std::vector<Pooled> pool_;
    size_t pooledBytes_ = 0;
    UINT timeSlot_ = 0;
    bool timing_ = false;   // the queries were made
    bool untimed_ = false;  // this frame's slot was still busy: the frame is not timed
    double gpuMs_ = 0.0;
    uint32_t gpuFrames_ = 0;
    ULONGLONG gpuSince_ = 0;
    std::map<std::pair<std::string, long>, int> failures_;  // a failure of each place and code logged once
};

}  // namespace uagfx::gpu
