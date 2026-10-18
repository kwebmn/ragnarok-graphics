#pragma once
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <vector>

#include "core/pick.hpp"
#include "core/effect_costs.hpp"
#include "core/footsteps.hpp"
#include "core/frame.hpp"
#include "core/critters.hpp"
#include "core/key_pair.hpp"
#include "core/scale.hpp"
#include "core/gpu_timing.hpp"
#include "core/ground_cells.hpp"
#include "core/hills.hpp"
#include "core/seams.hpp"
#include "core/seasons.hpp"
#include "core/wind.hpp"
#include "core/living.hpp"
#include "core/log_once.hpp"
#include "core/retry_timer.hpp"
#include "core/map_objects.hpp"
#include "core/haze.hpp"
#include "core/outdoors.hpp"
#include "core/pipeline_key.hpp"
#include "core/weather.hpp"
#include "core/torch.hpp"
#include "win/gpu/draw_constants.hpp"
#include "win/gpu/frame_pacer.hpp"
#include "win/gpu/renderer.hpp"
#include "win/gpu/shaders.hpp"
#include "win/gpu/map_world.hpp"
#include "win/gpu/upscaler.hpp"

namespace uagfx::gpu {

class AiTextures;  // ai_textures.hpp

// A pipeline that draws one triangle over the viewport into a target of that format and samples (ao.cpp).
D3D12_GRAPHICS_PIPELINE_STATE_DESC fullScreenPipeline(ID3D12RootSignature* root, ID3DBlob* vs, ID3DBlob* ps,
                                                      DXGI_FORMAT format, UINT samples);

// 2.4.3: the resource's name for the log of a lost device (DRED names the allocation a page fault hit).
void nameResource(ID3D12Resource* r, const char* name);

// A committed 2D texture in the default heap (device.cpp); null when it cannot be made.
ID3D12Resource* createTexture2D(ID3D12Device* device, int w, int h, int levels, DXGI_FORMAT format,
                                D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear,
                                UINT samples = 1);

template <typename T>
void releaseCom(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

constexpr UINT kSlots = 2;              // submissions in flight
// 0.39.2: the effects timed one by one, for the log; 0.76: the sky, the light in the air and the floors' reflections
// their own. 2.2.2: the sun's shadows' gone with them (2.1.4); 2.4.1: the new trees' too.
enum EffectTimer : UINT {
    kTimeAo, kTimeClouds, kTimeActorShadows, kTimeLights, kTimeHaze, kTimeHeat, kTimeGrade, kTimeDof, kTimeWeather,
    kTimeGlow, kTimeWater, kTimeSky, kTimeShafts, kTimeFloor, kTimeBloom, kTimeGrass,
    kEffectTimes
};
// 2.2.2: each timed pass's names, in the enum's order (core/effect_costs.hpp kTimedPasses: the key Costs know, the
// log's and the breadcrumbs' words)
static_assert(sizeof(effect_costs::kTimedPasses) / sizeof(effect_costs::kTimedPasses[0]) == kEffectTimes,
              "a name for each timed pass, in the enum's order");
inline const effect_costs::TimedPass& passName(UINT e) { return effect_costs::kTimedPasses[e]; }
// GPU timestamps a slot: its list, the upscale, the AI textures' work, (0.39.2) a pair an effect.
constexpr UINT kTimes = 6 + 2 * kEffectTimes;
constexpr UINT kCpuSrvCount = 65536;    // one SRV per texture (the client keeps thousands), the white one, the target
constexpr UINT kNoSrv = UINT(-1);       // allocSrv: the heap is full
constexpr UINT kGpuSrvCount = 65536;    // texture pairs bound for drawing: each slot's list has its own part
constexpr UINT kRingPerSlot = kGpuSrvCount / kSlots;
constexpr UINT kSamplerCount = 1024;    // cache of sampler pairs, two descriptors each
constexpr UINT64 kUploadSize = 32ull << 20;  // per slot; grows when one submission needs more
constexpr int kUpscaleGiveUp = 30;  // part 3: DLSS failing this many frames in a row is not used any more
constexpr UINT kMaxTorches = 32;    // 0.23: torches drawn anew in a frame; the rest as the game draws them
constexpr UINT kMaxLights = 64;     // 0.25: lights of the torches' pass - the torches' and the lamps'
constexpr UINT kMaxFireflies = 96;  // 0.28: fireflies drawn anew in a frame
constexpr UINT kMaxHeat = 32;       // 0.35: spots of heat haze in a frame

// Render target views: the target, the two swap chain buffers, the game-sized helper, a new target being filled;
// part 3 (DLSS): the world, its motion, the interface layer.
constexpr UINT kRtvTarget = 0;
constexpr UINT kRtvSwap = 1;
constexpr UINT kRtvGame = 3;
constexpr UINT kRtvNew = 4;
constexpr UINT kRtvWorld = 5;
constexpr UINT kRtvMotion = 6;
constexpr UINT kRtvLayer = 7;
constexpr UINT kRtvHudless = 8;  // frame generation: the picture without the interface, the interface
constexpr UINT kRtvUi = 9;
constexpr UINT kRtvGlow = 10;      // 0.18: the light the world's additive draws add
constexpr UINT kRtvUpscaled = 11;  // the upscaler's output, for the glow to be added to it
constexpr int kGlowLevels = 8;     // the glow's chain of smaller copies, the first half the world's size
constexpr UINT kRtvGlowChain = 12;
constexpr UINT kRtvSceneDepth = kRtvGlowChain + kGlowLevels;  // 0.19: the depth before the frame's first water
constexpr UINT kRtvAo = kRtvSceneDepth + 1;  // 0.20: the soft shadows' two targets
constexpr UINT kRtvDof = kRtvAo + 2;         // 0.22: the depth of field's two
constexpr UINT kRtvDofFocus = kRtvDof + 2;   // 0.22.3: its focus, last frame's and this one's
constexpr UINT kRtvBias = kRtvDofFocus + 2;  // 0.22.3: DLSS's mask of pixels whose history it is not to take
constexpr UINT kRtvShadow = kRtvBias + 1;  // 0.32: the characters' shadows' two masks
constexpr UINT kRtvClouds = kRtvShadow + 2;  // 0.39.3: the clouds' mask
constexpr UINT kRtvBloom = kRtvClouds + 1;  // 0.78: the bloom's two quarter-size targets
constexpr UINT kRtvFloor = kRtvBloom + 2;  // 0.86: the floors' reflections at half the size
constexpr UINT kRtvMaterial = kRtvFloor + 1;  // 1.8: a level of a texture's material, made anew for each
constexpr UINT kRtvMirror = kRtvMaterial + 1;  // 1.16: the world mirrored under the water
constexpr UINT kRtvTorch = kRtvMirror + 1;  // 2.5: the torches' buffer of light
constexpr UINT kRtvCount = kRtvTorch + 1;
constexpr UINT kDsvTarget = 0;  // depth views: the target's, the world's (DLSS)
constexpr UINT kDsvWorld = 1;
constexpr UINT kDsvMirror = 3;  // 1.16: the mirrored world's depth

// What the open command list draws into (bindPass); the glow's passes add its target as the second one (0.18).
enum Pass { kPassNone = 0, kPassTarget, kPassWorld, kPassLayer, kPassTargetGlow, kPassWorldGlow };

constexpr UINT kRootConstants = 0;
constexpr UINT kRootTextures = 1;
constexpr UINT kRootSamplers = 2;
constexpr UINT kRootScene = 3;  // 0.19: t2-t4, the picture and its depth before the frame's first water

// Root constants; must match cbuffer Params in shaders.cpp.

struct Texture {
    ID3D12Resource* resource = nullptr;
    int width = 0;
    int height = 0;
    int levels = 1;
    UINT srv = 0;  // index in the CPU SRV heap
    bool leafy = false;  // 0.64: leaves (core/foliage.hpp): swayed in the world
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
    // AI textures (ai_textures.hpp): the network's version, drawn in the world instead while it is shown.
    ID3D12Resource* ai = nullptr;
    D3D12_RESOURCE_STATES aiState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT aiSrv = 0;
    // Where it stands in: the world's draws, the interface's (0.14.1: the sprite cache's copy for the cursor, the
    // loading screen and other big pictures of the interface).
    uint8_t aiKind = 0xFF;  // 2.2.1: its AI picture's kind (ai::Kind) - shown while that kind is on; 0xFF unknown
    bool aiWorld = true;
    bool aiUi = false;
    bool aiHidden = false;  // 2.4.3: uaRO Studio's A/B (Ctrl+Shift+F7) - the game's own drawn, the copy kept
    // 0.26: how much its picture looks like fire (core/effect_light.hpp) and its colour - an effect drawn from it
    // lights around it.
    float fire = 0.0f;
    float fireColour[3] = {0, 0, 0};
    float light = 0.0f;  // 0.36: how much it lights in its own colour (core/effect_light.hpp: lightOf)
    float lightColour[3] = {0, 0, 0};
    // 0.32: a sprite cache's cells and what each holds now (sprite_exe::Source) - the characters cast shadows.
    std::vector<SpriteCell> spriteCells;
    // 1.8: its material (materials.cpp) - relief, height, gloss - made from the picture matFor (the game's or the AI's)
    ID3D12Resource* mat = nullptr;
    D3D12_RESOURCE_STATES matState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT matSrv = 0;
    const void* matFor = nullptr;
};

// 0.26: a draw of a fire-coloured effect this frame: its middle in the view, how much it lights, its colour.
struct FireSpark {
    float view[3];
    float strength;
    float colour[3];
    float gameX, gameY;  // its middle on the screen, game pixels (gathered by place)
};

// 0.30: a face of one of the forest's beams of light drawn this frame - its middle and its foot (the end at the
// ground) in the view, and on the screen (game pixels) to gather them by place.
struct RayPart {
    float middle[3];
    float foot[3];
    float gameX, gameY;
};

// 0.35: a spot of heat this frame (a flame, burning skills, lava): its middle and radius in target pixels, how much
// (the player's setting in it). 0.38: a portal's swirl - strength below 0: the air turns about it instead of rising.
struct HeatSpot {
    float x, y, radius, strength;
};

// 0.39.1: a portal followed from frame to frame: its middle in the view and on the screen (game pixels), its size,
// how much it shows (fading in and out as its rings come and go).
struct PortalTrack {
    float view[3];
    float gx, gy, size, strength;
};

// 0.28: a firefly the game draws this frame (effect 45: its dot, a quad facing the screen) - drawn anew.
struct Firefly {
    float gameX, gameY;  // its middle, game pixels
    float rhw;           // the depth of its quad
    float size;          // the quad's side, game pixels
    float alpha;         // the game's fade of it
    float colour[3];     // fireflies::colourOf the game's tint
    float seed;          // 0-1, its primitive's own
};

struct Slot {
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12Resource* upload = nullptr;
    uint8_t* mapped = nullptr;
    UINT64 size = 0;
    UINT64 used = 0;
    UINT64 fence = 0;                        // signalled when this slot's last submission finished
    std::vector<IUnknown*> garbage;          // released once that fence has passed
    bool timed = false;                      // its list's start and end times are in the timer's buffer
    bool upscaleTimed = false;               // and those of an upscale in it
    bool aiTimed = false;                    // and those of the AI textures' work in it
    uint32_t effectTimed = 0;                // 0.39.2: and those of these effects (1 << EffectTimer)
};

struct Upload {
    uint8_t* cpu;
    D3D12_GPU_VIRTUAL_ADDRESS gpu;
    UINT64 offset;  // in the slot's upload buffer
};

const char* weatherName(weather::Kind k);  // 0.39: for the log

struct Renderer::Impl {
    HMODULE d3d12 = nullptr;
    HMODULE dxgi = nullptr;
    HMODULE compiler = nullptr;
    std::string shaderCacheFile;  // 0.39.5: the shaders' code kept between starts (pipeline.cpp); "" none
    Renderer::StartSink startSink = nullptr;  // 0.40: told of the shaders loaded or compiled
    void* startContext = nullptr;
    IDXGIFactory4* factory = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    // Stage 4 review: sample positions - every sample at the pixel's centre for interface draws, whose edges fall on
    // pixel centres; null when the card cannot place samples, and then MSAA stays off.
    ID3D12GraphicsCommandList1* list1 = nullptr;
    // 1.9.4: breadcrumbs - each pass writes its number into crumbs as it starts and once what came before it is done
    // (WriteBufferImmediate); after a lost device the log names the pass the GPU stopped in.
    ID3D12GraphicsCommandList2* list2 = nullptr;
    ID3D12Resource* crumbs = nullptr;
    volatile uint32_t* crumbsMapped = nullptr;
    uint32_t crumbSeq = 0;
    const char* lastCrumb = nullptr;
    std::vector<const char*> crumbNames;
    void crumb(const char* what);
    std::string crumbsReport() const;
    bool centered = false;  // the open list has them at the centre now
    ID3D12Fence* fence = nullptr;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValue = 0;
    Slot slots[kSlots];
    UINT slot = 0;
    bool open = false;
    // 2.4.3: a resource the GPU may still read goes once the list that could read it has run: the open list's slot,
    // or - no list open - the last submitted one's (its slot's begin() waits for that submission). Until now it went
    // to slots[slot] always, which between two lists is the slot about to begin: freed after a wait for the
    // submission before last only, while the last could still read it (R-TT5RVW: the device lost after an A/B).
    void discard(IUnknown* r) {
        if (r) slots[open ? slot : (slot + kSlots - 1) % kSlots].garbage.push_back(r);
    }
    bool dred = false;  // 2.4.3: D3D12's device-removed data is on (page faults): dredReport() after a loss
    void dredReport();
    std::string adapter;
    uint64_t adapterMb = 0;  // 1.8: its own memory, for the remaster's automatic choice
    LogOnce failures;
    bool lost = false;  // the device was removed (driver crash, TDR): no more GPU work, the reason is in the log

    ID3D12DescriptorHeap* cpuSrv = nullptr;
    std::vector<UINT> freeSrv;
    std::vector<uint32_t> dying;  // released by the client; the record execute() plays next may still draw with them
    UINT nextSrv = 0;
    ID3D12DescriptorHeap* gpuSrv = nullptr;
    UINT ringSrv = 0;  // the next free place in this list's part of the ring, which starts at slot * kRingPerSlot
    ID3D12DescriptorHeap* samplerHeap = nullptr;
    std::unordered_map<KeyPair, UINT, KeyPairHash> samplerPairs;
    UINT nextSampler = 0;
    ID3D12DescriptorHeap* rtvHeap = nullptr;  // kRtv*
    ID3D12DescriptorHeap* dsvHeap = nullptr;
    UINT srvStep = 0;
    UINT samplerStep = 0;
    UINT rtvStep = 0;
    UINT dsvStep = 0;

    ID3D12Resource* target = nullptr;
    ID3D12Resource* depth = nullptr;
    D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT targetSrv = 0;  // the target as one sample per pixel: itself, or `resolved` when it has several
    UINT samples = 1;    // stage 4: of the target and its depth
    int askedSamples = 0;  // the last count setScale was asked for: the msaa line once per change
    ID3D12Resource* resolved = nullptr;  // samples > 1: the target resolved, for showing and reading it
    D3D12_RESOURCE_STATES resolvedState = D3D12_RESOURCE_STATE_RESOLVE_DEST;
    int anisotropy = 0;           // stage 4: of world draws; 0 = off
    bool alphaToCoverage = false;  // stage 4
    bool xray = false;             // 2.3: uaRO Studio's X-ray - the world's and the interface's textures tinted
    // 2.3, uaRO Studio's pick (L2): the cursor's game pixel asked about - the next frame's records looked through, its
    // present writing the texture of the last draw found under it (2.3.1) - and the picked texture's picture off or
    // on (A/B).
    pick::Request pick;
    bool abWanted = false;
    // Part 3 (DLSS, uaro-render64.exe; upscaler.hpp): the world at the render size with a jitter, its depth and
    // motion, the upscaler's output at the target's size and the interface layer (premultiplied) that goes on top.
    Upscaler* upscaler = nullptr;
    AiTextures* ai = nullptr;  // AI textures, when on (startAiTextures)
    Renderer::IconSink iconSink = nullptr;  // where AI icons go (Renderer::setIconSink)
    void* iconContext = nullptr;
    bool upscale = false;  // on now
    ID3D12Resource* world = nullptr;
    ID3D12Resource* worldDepth = nullptr;  // R32 typeless: D32 to draw into, R32 to read
    ID3D12Resource* motion = nullptr;      // R16G16 float, target pixels
    ID3D12Resource* upscaled = nullptr;    // RGBA8, unordered access
    ID3D12Resource* layer = nullptr;
    // 0.22.3: R8 at the render size - the characters', monsters' and effects' coverage, which the world's draws of
    // them write into its alpha: DLSS takes no history there (they move by themselves, not with the camera).
    ID3D12Resource* biasMask = nullptr;
    D3D12_RESOURCE_STATES biasState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT biasSrv = 0;
    // 0.75, TAA (applyTaa): the motion read, the last result
    UINT motionSrv = 0, taaHistorySrv = 0;
    ID3D12Resource* taaHistory = nullptr;
    D3D12_RESOURCE_STATES taaHistoryState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    ID3DBlob* taaPs = nullptr;
    ID3D12PipelineState* taaPipeline = nullptr;
    bool applyTaa(bool reset, bool masked);  // into upscaled, then kept as the history; false: not run
    ID3DBlob* maskPs = nullptr;
    ID3D12PipelineState* maskPipeline = nullptr;
    D3D12_RESOURCE_STATES worldState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES worldDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    D3D12_RESOURCE_STATES motionState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES upscaledState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    D3D12_RESOURCE_STATES layerState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT depthSrv = 0, upscaledSrv = 0, layerSrv = 0, worldSrv = 0;
    int renderWidth = 0, renderHeight = 0;
    double worldScale = 1.0;  // game pixels -> world pixels
    D3D12_VIEWPORT worldViewport{};
    D3D12_RECT worldScissor{};
    float jitter[2] = {0, 0};  // world pixels, this frame
    uint32_t upscaleFrame = 0;
    bool worldDrawn = false;   // the world changed since the last upscale
    bool layerDrawn = false;   // the layer has something since its last clear
    bool frameEnded = true;    // presented: the next command starts a frame (the layer is cleared then)
    bool historyReset = true;  // the next upscale has no usable history
    bool upscaledValid = false;  // the last upscale ran: `upscaled` holds the world
    bool upscaledNow = false;    // ... since the last present: only then has frame generation this frame's inputs
    bool worldValid = false;     // `world` has been drawn into since the upscale targets were made
    bool generationFailed = false;  // its inputs could not be made at this size: not tried again
    UINT generationFailedW = 0, generationFailedH = 0;
    int upscaleFailures = 0;     // the upscale failed this many frames in a row
    bool dropUpscale = false;    // so many that it stops after this present
    bool cameraValid = false, prevCameraValid = false;
    float cameraTargetZ = 0;  // 1.4.2: the depth of the point the game's camera looks at (the character), 0 unknown
    float camera[12]{}, prevCamera[12]{};
    ID3DBlob* motionPs = nullptr;
    ID3DBlob* copyPs = nullptr;  // copy_ps: alpha kept
    ID3D12PipelineState* motionPipeline = nullptr;
    ID3D12PipelineState* overPipeline = nullptr;  // the layer over the picture: premultiplied
    ID3D12PipelineState* copyPipeline = nullptr;  // the layer copied as it is, alpha kept (frame generation)
    // 0.18, the glow of effects (effects.cpp): world draws write the light they add into `glow` too, from the first
    // additive draw of a frame on; once the world is drawn that light is shrunk into the chain, blurred back up and
    // added to the picture - before the interface, or (DLSS) to the upscaled world.
    EffectSettings effects;
    ID3D12Resource* glow = nullptr;          // R11G11B10 float: the world target's size and samples
    ID3D12Resource* glowResolved = nullptr;  // with several samples: `glow` resolved
    D3D12_RESOURCE_STATES glowState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES glowResolvedState = D3D12_RESOURCE_STATE_RESOLVE_DEST;
    UINT glowSrv = 0, glowResolvedSrv = 0;
    int glowWidth = 0, glowHeight = 0;
    UINT glowSamples = 0;
    bool glowFailed = false;   // not made at this size and count: not tried again every frame
    ID3D12Resource* glowChain[kGlowLevels] = {};
    D3D12_RESOURCE_STATES glowChainState[kGlowLevels] = {};
    UINT glowChainSrv[kGlowLevels] = {};
    int glowChainWidth[kGlowLevels] = {}, glowChainHeight[kGlowLevels] = {};
    int glowLevels = 0;
    bool glowActive = false;  // this frame's world draws write into `glow` too
    bool glowDrawn = false;   // `glow` holds light not yet added to the picture
    ID3DBlob* drawGlowPs = nullptr;
    ID3DBlob* glowDownPs = nullptr;
    ID3DBlob* glowUpPs = nullptr;
    ID3DBlob* glowAddPs = nullptr;
    ID3D12PipelineState* glowDownPipeline = nullptr;
    ID3D12PipelineState* glowUpPipeline = nullptr;
    ID3D12PipelineState* glowAddPipelines[5] = {};  // onto the target of 1, 2, 4, 8 samples; onto the upscaled world
    ID3D12PipelineState* glowLightPipelines[5] = {};  // 0.20: the light on the surroundings, the same five
    // 0.19, modern water (water.cpp): the frame's picture and depth as they were before its first cell of water -
    // the map, its models and the characters are drawn by then - for the water's own shader.
    D3D12_RESOURCE_STATES depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;  // the target's depth
    UINT depthTargetSrv = 0;               // ... read as R32 (with several samples a multisample view)
    ID3D12Resource* sceneColor = nullptr;  // B8G8R8A8, the world's size, one sample
    ID3D12Resource* sceneDepth = nullptr;  // R32 float: the game's depth
    D3D12_RESOURCE_STATES sceneColorState = D3D12_RESOURCE_STATE_COPY_DEST;
    D3D12_RESOURCE_STATES sceneDepthState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT sceneColorSrv = 0, sceneDepthSrv = 0;
    int sceneWidth = 0, sceneHeight = 0;
    bool sceneFailed = false;  // not made at this size: the water stays the game's
    bool waterCopied = false;  // this frame's copy is made, or could not be
    bool waterReady = false;   // ... and the water is drawn from it
    int softCopied = 0;        // 0.67: the soft effects' depth: 1 copied this picture, -1 could not be
    float viewToWorld[12] = {};  // the camera backwards: the view's x, y and z axes in the world, then its place
    bool viewToWorldValid = false;
    ID3DBlob* waterVs = nullptr;
    ID3DBlob* waterPs = nullptr;
    ID3DBlob* waterGlowPs = nullptr;
    ID3DBlob* depthCopyPs = nullptr;
    ID3DBlob* depthCopyMsPs = nullptr;
    ID3D12PipelineState* depthCopyPipeline = nullptr;
    ID3D12PipelineState* depthCopyMsPipeline = nullptr;
    // 0.20, soft shadows (ao.cpp): from the depth of the map and its models once they are drawn - at the frame's
    // first blended draw of the world after its ground - multiplied into the picture before the characters.
    ID3D12Resource* aoTarget[2] = {};  // R8, half the world's size: the shadows, then blurred back and forth
    D3D12_RESOURCE_STATES aoState[2] = {};
    UINT aoSrv[2] = {};
    int aoWidth = 0, aoHeight = 0;
    bool aoFailed = false;
    bool aoDone = false;       // this frame's shadows are on the picture, or could not be
    bool groundDrawn = false;  // this frame has drawn ground (two UV sets): the map is there
    ID3DBlob* aoPs = nullptr;
    ID3DBlob* aoBlurPs = nullptr;
    ID3DBlob* aoApplyPs = nullptr;
    ID3D12PipelineState* aoPipeline = nullptr;
    ID3D12PipelineState* aoBlurPipeline = nullptr;
    ID3D12PipelineState* aoApplyPipelines[4] = {};
    // 0.21, the picture's colours (ao.cpp): once a frame, with the shadows' timing but once the world is drawn.
    bool gradeDone = false;
    ID3DBlob* gradePs = nullptr;
    ID3D12PipelineState* gradePipelines[4] = {};  // into the target of 1, 2, 4, 8 samples (the world under DLSS: 1)
    bool nightDone = false;  // 0.63
    ID3DBlob* nightPs = nullptr;
    ID3D12PipelineState* nightPipelines[4] = {};
    // 0.22, depth of field (ao.cpp): after the colours, from a quarter-sized blurred copy.
    bool dofDone = false;
    ID3D12Resource* dofTarget[2] = {};  // B8G8R8A8, a quarter of the world's size
    D3D12_RESOURCE_STATES dofState[2] = {};
    UINT dofSrv[2] = {};
    int dofWidth = 0, dofHeight = 0;
    bool dofFailed = false;
    ID3DBlob* dofDownPs = nullptr;
    ID3DBlob* dofBlurPs = nullptr;
    ID3DBlob* dofApplyPs = nullptr;
    ID3D12PipelineState* dofDownPipeline = nullptr;
    ID3D12PipelineState* dofBlurPipeline = nullptr;
    ID3D12PipelineState* dofApplyPipelines[4] = {};
    // 0.22.3: the focus in one R32 texel, the two in turn: this frame's made from the last one's.
    ID3D12Resource* dofFocus[2] = {};
    D3D12_RESOURCE_STATES dofFocusState[2] = {};
    UINT dofFocusSrv[2] = {};
    int dofFocusNow = 0;
    LARGE_INTEGER dofFocusTime{};  // when the focus last moved (0.22.4: by time, not by frames)
    ID3DBlob* dofFocusPs = nullptr;
    ID3D12PipelineState* dofFocusPipeline = nullptr;
    // 0.23, the torches (torches.cpp): the game's torch layers are not drawn - its glow not at all, its flames noted -
    // and drawn anew once the world is: their light into the picture, then each flame and the glow around it.
    objects::Textures objectTextures;      // 0.23.1, 0.24: the textures that are a frame of a map object
    MapWorld mapWorld;                      // 0.25: the map file - its lamps and the effects the game does not show
    std::vector<torch::Flame> torchFlames;  // this frame's
    std::vector<FireSpark> fireSparks;      // 0.26: this frame's fire of skills
    std::vector<Firefly> fireflyDraws;      // 0.28: this frame's fireflies the game draws
    bool firefliesLogged = false;
    bool mapFirefliesLogged = false;
    bool mapGlowsLogged = false;
    bool torchDone = false;                 // this frame's are drawn, or could not be
    bool torchesLogged = false;
    ID3D12Resource* torchData = nullptr;  // R32G32B32A32 float, a texel a torch: its light's place and strength
    D3D12_RESOURCE_STATES torchDataState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT torchDataSrv = 0;
    // 2.5: the light in two passes (shaders.cpp torch_light_vs): each light's quad adds into a buffer of light the
    // world's size (R16G16B16A16 float: colour times amount, amount), one pass turns it into the picture.
    ID3DBlob* torchLightVs = nullptr;
    ID3DBlob* torchAccumPs = nullptr;
    ID3DBlob* torchApplyPs = nullptr;
    ID3D12PipelineState* torchAccumPipeline = nullptr;
    ID3D12Resource* torchLight = nullptr;
    D3D12_RESOURCE_STATES torchLightState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT torchLightSrv = 0;
    int torchLightW = 0, torchLightH = 0;
    bool torchLightFailed = false;
    bool ensureTorchLight(int w, int h);  // the buffer at this size; false: none (logged once)
    ID3DBlob* torchFirePs = nullptr;
    ID3DBlob* smokePs = nullptr;  // 0.24: the chimneys' smoke (draw.cpp), with the glow's second target and without
    ID3DBlob* smokeGlowPs = nullptr;
    ID3D12PipelineState* torchLightPipelines[4] = {};  // 2.5: the apply pass into the target of 1, 2, 4, 8 samples
    ID3D12PipelineState* torchFirePipelines[5] = {};   // the same, and the world under DLSS (its alpha the mask)
    ID3DBlob* fireflyPs = nullptr;                      // 0.28: a firefly's dot and halo
    ID3D12PipelineState* fireflyPipelines[5] = {};     // as torchFirePipelines
    ID3DBlob* dustPs = nullptr;                         // 0.29: a cloud of the towers' dust
    ID3D12PipelineState* dustPipelines[5] = {};
    ID3DBlob* rayPs = nullptr;  // 0.30: a beam of the forest's light (draw.cpp), with the glow's second target
    ID3DBlob* rayGlowPs = nullptr;
    std::vector<RayPart> rayParts;  // 0.30: this frame's faces of the forest's beams: dust and light where they fall
    // 0.32, the characters' shadows (actor_shadows.cpp): their layers laid on the ground into a half-sized mask,
    // softened, multiplied into the picture before the characters are drawn.
    ID3DBlob* shadowVs = nullptr;
    ID3DBlob* shadowPs = nullptr;
    ID3DBlob* shadowBlurPs = nullptr;
    ID3DBlob* shadowApplyPs = nullptr;
    ID3D12PipelineState* shadowQuadPipeline = nullptr;
    ID3D12PipelineState* shadowBlurPipeline = nullptr;
    ID3D12PipelineState* shadowApplyPipelines[4] = {};
    ID3D12Resource* shadowMask[2] = {};
    D3D12_RESOURCE_STATES shadowMaskState[2] = {};
    UINT shadowMaskSrv[2] = {};
    int shadowMaskW = 0, shadowMaskH = 0;
    bool shadowMaskFailed = false;
    bool actorShadowsDone = false;   // this frame's are in, or could not be
    bool actorShadowsDrawn = false;  // and the game's round ones with them: not drawn on their own
    bool actorShadowsLogged = false;
    bool sceneDepthFresh = false;  // 0.39.2: sceneDepth holds the depth as it is now (copySceneDepth)
    // 0.33, the clouds' shadows (clouds.cpp) over the open sky's maps; 0.39.3: worked out into a smaller mask first.
    ID3DBlob* cloudsPs = nullptr;
    ID3DBlob* cloudsApplyPs = nullptr;
    ID3D12PipelineState* cloudsMaskPipeline = nullptr;
    ID3D12PipelineState* cloudsPipelines[4] = {};  // the mask into the target of 1, 2, 4, 8 samples (the world: 1)
    ID3D12Resource* cloudsMask = nullptr;          // R8: the light they leave
    D3D12_RESOURCE_STATES cloudsMaskState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT cloudsMaskSrv = 0;
    int cloudsMaskW = 0, cloudsMaskH = 0;
    bool cloudsMaskFailed = false;
    bool cloudsDone = false;
    bool cloudsLogged = false;
    bool mapOutdoors = false;  // the map is a field or a town (core/outdoors.hpp)
    // 0.34, haze and fog (haze.cpp), the map's look (core/haze.hpp).
    ID3DBlob* hazePs = nullptr;
    ID3D12PipelineState* hazePipelines[4] = {};
    bool hazeDone = false;
    bool hazeLogged = false;
    haze::Look mapLook;
    // 0.73, light in the air (haze.cpp, shafts_ps): the torches' pass leaves its lights' count.
    ID3DBlob* shaftsPs = nullptr;
    ID3D12PipelineState* shaftsPipelines[4] = {};
    bool shaftsDone = false;
    ID3DBlob* flarePs = nullptr;  // 0.98: the sun's flare (haze.cpp applyFlare)
    ID3D12PipelineState* flarePipelines[4] = {};
    bool flareDone = false, flareLogged = false;
    bool flareWanted() const { return effects.flare && flarePs && mapOutdoors && effects.nightNow < 60; }
    bool flareDue() const { return flareWanted() && groundDrawn && !flareDone; }
    bool applyFlare();  // once a frame, the world drawn; false: lost
    UINT shaftLights = 0;
    // 0.72, reflections on the floor (haze.cpp, floor_reflect_ps).
    ID3DBlob* floorReflectPs = nullptr;
    ID3DBlob* halfOverPs = nullptr;  // 0.86: at half the size, then laid over
    ID3D12PipelineState* floorReflectPipeline = nullptr;
    ID3D12PipelineState* halfOverPipelines[4] = {};
    ID3D12Resource* floorTarget = nullptr;
    D3D12_RESOURCE_STATES floorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT floorSrv = 0;
    int floorWidth = 0, floorHeight = 0;
    bool floorReflectDone = false;
    bool floorReflectLogged = false;
    // 0.69, the sky past the map's edge (haze.cpp, sky_ps).
    ID3DBlob* skyPs = nullptr;
    ID3D12PipelineState* skyPipelines[4] = {};
    bool skyDone = false;
    bool skyLogged = false;
    // 0.35, heat haze (heat.cpp): the spots the torches' pass gathers, read aside from the picture's copy.
    ID3DBlob* heatPs = nullptr;
    ID3D12PipelineState* heatPipelines[4] = {};
    ID3D12Resource* heatData = nullptr;
    D3D12_RESOURCE_STATES heatDataState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT heatDataSrv = 0;
    std::vector<HeatSpot> heatSpots;
    std::vector<FireSpark> portalSparks;  // 0.38: this frame's portals' draws (their middles), gathered by place
    std::vector<PortalTrack> portalTracks;  // 0.39.1: the portals followed from frame to frame
    float portalTime = 0.0f;
    bool heatDone = false;
    bool heatLogged = false;
    bool mapDesert = false;  // core/outdoors.hpp
    // 0.39, the driver's own weather (weather.cpp) on the map's list (core/weather.hpp).
    ID3DBlob* weatherPs = nullptr;
    ID3D12PipelineState* weatherPipelines[4] = {};
    ID3DBlob* splashPs = nullptr;  // 0.64: the rain's splashes
    ID3DBlob* waterfallPs = nullptr;  // 0.65: the maps' waterfalls (waterfalls.cpp)
    // 0.65: [display] hdr - the back buffer HDR10 (R10G10B10A2, PQ) where the window's monitor shows HDR
    bool hdrWanted = false, hdrOn = false, hdrDecided = false, blitHdr = false;
    DXGI_FORMAT swapFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    ID3D12PipelineState* presentHdrPipeline = nullptr;
    void decideHdr();
    ID3D12PipelineState* hdrPipeline();
    ID3D12PipelineState* waterfallPipelines[4] = {};
    bool waterfallsDone = false, waterfallsLogged = false;
    ID3D12PipelineState* splashPipelines[4] = {};
    bool weatherDone = false;
    bool weatherLogged = false;
    weather::Kind mapWeather = weather::Kind::None;
    ID3DBlob* lavaPs = nullptr;     // 0.31: a cell of lava (draw.cpp), with the glow's second target
    ID3DBlob* lavaGlowPs = nullptr;
    std::string mapName;                    // the map the game shows (setMap)
    trees::Season mapSeason = trees::Season::Green;  // 1.7: its trees' (core/trees.hpp)
    seasons::Look mapYear;  // 1.20: the year on its ground (core/seasons.hpp)
    // 1.22: the shared wind (core/wind.hpp), once a frame (weather.cpp updateWind): its way in the world, its way on
    // the screen (the view's x axis), and what it has carried the weather (pixels), the clouds' shadows and the sky's
    // clouds so far
    wind::Wind windNow;
    float windScreenX = 0.0f, windClock = -1.0f;
    wind::Drift weatherDrift, cloudsDrift, skyDrift;
    bool windShared() const { return effects.sharedWind && viewToWorldValid; }
    void updateWind();
    living::Place mapPlace;                          // 1.7: its small details (core/living.hpp)
    int minuteNow() const { return effects.dayCycle && effects.dayNow >= 0 ? effects.dayNow : living::localMinute(); }
    LeafLook leafLook() const;  // 1.7: the trees' settings for leafShading (draw.cpp)
    std::vector<FireSpark> lavaCells;       // 0.31: this frame's cells of lava: their middles light around them
    bool lavaLogged = false;
    LARGE_INTEGER waterClock{};  // the waves' time starts with the first water
    uint64_t waterCells = 0;     // drawn as modern water this run, for the log
    HMODULE api = nullptr;  // setApi
    // GPU time (0.10.4): timestamps around every list and the upscale, read when the slot comes round again, and a
    // log line every half a minute - how much of each frame the card spends on the picture, and DLSS's part of it.
    ID3D12QueryHeap* timer = nullptr;  // kTimes per slot: list start and end, upscale's, AI textures'
    ID3D12Resource* timerBuffer = nullptr;
    const UINT64* timerData = nullptr;  // mapped for good
    UINT64 timerFrequency = 0;
    UINT64 gpuTicks = 0, upscaleTicks = 0, aiTicks = 0;
    uint32_t gpuFrames = 0;
    ULONGLONG gpuSince = 0;
    void readTimes(Slot& s, UINT index);  // the slot's last list has finished
    // 0.39.2: an effect's GPU time, between these two in one list (an effect that starts a new list is not timed).
    uint64_t effectTicks[kEffectTimes] = {};
    uint64_t effectSerial[kEffectTimes] = {};
    double effectMs[kEffectTimes] = {};  // 0.76: the last half minute's, ms a frame (Renderer::effectCosts)
    bool effectMsKnown = false;
    // 2.2.1: the last half second's, for the frame counter and the console (Renderer::gpuTiming)
    UINT64 recentGpu = 0, recentUpscale = 0, recentEffect[kEffectTimes] = {};
    uint32_t recentFrames = 0;
    ULONGLONG recentSince = 0;
    GpuTiming recent;
    void effectBegin(EffectTimer e);
    void effectEnd(EffectTimer e);
    void noteGpuFrame();                   // after a present
    void (*onDevice)(void*, ID3D12Device*) = nullptr;  // setOnDevice
    void* onDeviceContext = nullptr;
    // Frame generation: the back buffer's picture without the interface and the interface alone (upscaler.hpp).
    ID3D12Resource* hudless = nullptr;
    ID3D12Resource* uiCopy = nullptr;
    D3D12_RESOURCE_STATES hudlessState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES uiCopyState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT hudlessSrv = 0, uiCopySrv = 0;
    UINT generationWidth = 0, generationHeight = 0;
    bool renderStarted = false;  // renderStart() told for this frame
    int width = 0;  // the target: the game's size times scale
    int height = 0;
    int gameWidth = 0;  // the client's size
    int gameHeight = 0;
    double scale = 1.0;  // k: game pixels -> target pixels (stage 3)
    bool uiSharp = true;  // [display] ui_filter = sharp
    ViewportCmd gameViewport;  // the client's viewport, in game pixels
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissor{};
    // The game-sized helper: the target shrunk for reading back, the client's pixels before they are stretched in.
    ID3D12Resource* gameSized = nullptr;
    D3D12_RESOURCE_STATES gameState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT gameSrv = 0;
    ID3D12Resource* readback = nullptr;
    // 0.54: the screenshot - the back buffer copied before Present, written after it
    ID3D12Resource* shotBuffer = nullptr;
    UINT64 shotSize = 0;
    UINT shotPitch = 0;
    int shotWidth = 0, shotHeight = 0;
    std::string shotPath;
    UINT64 readbackSize = 0;

    std::unordered_map<uint32_t, Texture> textures;
    uint32_t nextTexture = 1;
    uint32_t white = 0;

    ID3D12RootSignature* root = nullptr;
    ID3DBlob* drawVs = nullptr;
    ID3DBlob* drawPs = nullptr;
    ID3DBlob* presentVs = nullptr;
    ID3DBlob* presentPs = nullptr;
    ID3D12PipelineState* presentPipelines[4] = {};  // copies into a target of 1, 2, 4 or 8 samples (stage 4)
    std::unordered_map<uint64_t, ID3D12PipelineState*> pipelines;
    std::string pipelineFile;  // the saved keys (1B review M8): every new pipeline is added to it

    HWND window = nullptr;
    IDXGISwapChain3* swapChain = nullptr;
    ID3D12Resource* backBuffers[2] = {nullptr, nullptr};
    D3D12_RESOURCE_STATES backStates[2] = {D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT};
    Rect picture;  // where present() last put the picture in the window, for the log
    UINT swapWidth = 0;
    UINT swapHeight = 0;
    UINT failedWidth = 0;  // a swap chain of this size could not be made: not retried every frame
    UINT failedHeight = 0;
    RetryTimer swapRetry{1000};  // that size is tried again after a second (stage 3 review M1)
    bool tearingSupported = false;
    HANDLE presentSurface = nullptr;  // part 3: a composition surface instead of the game's window
    bool surfaceNoted = false;        // the log has said frame generation stays off on it
    bool refused = false;             // the game's window took no swap chain from this process
    FramePacer pacer;  // 0.85: [frames] fps_limit
    // 0.86: the processor's time in the driver's drawing (execute, present until its wait), for the log
    LONGLONG cpuTicks = 0;
    // 2.1.1: where a frame's time goes besides the drawing - waiting for the card (a slot's fence), the frame limit's
    // wait, the swap chain's present - for the log next to cpuTicks (the game's own wait is the bridge's, remote.cpp)
    LONGLONG fenceTicks = 0, pacerTicks = 0, swapTicks = 0;
    struct CpuTime {
        LONGLONG& total;
        LARGE_INTEGER start{};
        explicit CpuTime(LONGLONG& t) : total(t) { QueryPerformanceCounter(&start); }
        ~CpuTime() { stop(); }
        void stop() {
            if (!start.QuadPart) return;
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            total += now.QuadPart - start.QuadPart;
            start.QuadPart = 0;
        }
    };

    // Geometry of the record being executed, copied into the upload buffer once per execute().
    D3D12_GPU_VIRTUAL_ADDRESS vertexBase = 0;
    D3D12_GPU_VIRTUAL_ADDRESS indexBase = 0;
    UINT indexBytes = 0;
    UINT64 listSerial = 0;    // counts the lists begin() opened
    UINT64 geometryList = 0;  // the list that geometry was uploaded for: a later list uploads it again
    // Bindings already set on the open command list (reset by begin()).
    ID3D12PipelineState* boundPipeline = nullptr;
    uint64_t boundTextures = ~0ull;
    uint64_t boundSamplers = ~0ull;
    int pass = kPassNone;

    // device.cpp
    bool begin();
    void submit();
    void waitSlot(Slot& s);
    void waitIdle();
    void retireDying();  // after execute() has submitted: the dying textures go with that submission
    void failed(uint32_t place, const char* what, HRESULT hr);
    bool createTarget(int w, int h, UINT n, ID3D12Resource*& rt, ID3D12Resource*& ds);
    bool createUpscaleTargets(int targetW, int targetH);  // part 3: false (and nothing kept) when it cannot
    void releaseUpscaleTargets();
    void stopUpscaling();  // for the rest of the run: the world at the target's size, without jitter
    void startFrame();   // the first command after a present: a clean interface layer
    bool composite();    // the world upscaled and the layer on top, into the target; false: lost
    void nextJitter();
    UINT supportedSamples(UINT want);  // the most samples up to `want` the card has for the target and its depth
    // The target with one sample per pixel, for blit() and copies: itself, or resolved into `resolved` (stage 4).
    struct Readable {
        ID3D12Resource* resource;
        D3D12_RESOURCE_STATES* state;
    };
    Readable readableTarget();
    bool scaled() const { return width != gameWidth || height != gameHeight; }
    void applyViewport();
    // false: there is no list to record into (the GPU is lost) - the caller stops too
    bool blit(ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState, UINT srcSrv, int srcW, int srcH,
              const float srcRect[4], D3D12_CPU_DESCRIPTOR_HANDLE dst, const Rect& dstRect, int dstW, int dstH,
              float sharpen = -1.0f, UINT dstSamples = 1, bool over = false, bool copy = false);
    bool ensureGenerationTargets(UINT w, UINT h);  // frame generation's two back-buffer-sized textures
    bool growUpload(Slot& s, UINT64 size);
    Upload allocUpload(UINT64 size, UINT64 alignment);
    void transition(ID3D12Resource* r, D3D12_RESOURCE_STATES& state, D3D12_RESOURCE_STATES to);
    UINT allocSrv();
    D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvHandle(UINT i) const;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(UINT i) const;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle(UINT i = kDsvTarget) const;
    bool ensureSwapChain(HWND hwnd, UINT w, UINT h, bool wantTearing);
    void releaseSwapChain();
    void clear(const ClearCmd& c);
    void setViewport(const ViewportCmd& v);
    void upload(const FrameRecord& rec, const UploadCmd& u);
    void writeTarget(const FrameRecord& rec, const WriteTargetCmd& w);
    void bindTarget() { bindPass(kPassTarget); }
    void bindPass(int p);
    void placeSamples(bool center);  // stage 4 review: at the centre for the interface, the card's default else
    void destroy();
    // pipeline.cpp
    bool createPipelines(std::string& error);
    ID3D12PipelineState* presentPipelineFor(UINT n);  // made the first time a target of n samples needs it
    // aniso: the anisotropy of the pair's samplers, 0 for interface draws and copies (stage 4).
    D3D12_GPU_DESCRIPTOR_HANDLE samplerTable(const StageSetup& s0, const StageSetup& s1, int aniso, float bias = 0.0f);
    // 2.2.4: the mip bias of the world's textures now - under DLSS lower, for the picture's pixels (upscaleMipBias)
    float worldMipBias() const { return upscale ? upscaleMipBias(worldScale, scale) : 0.0f; }
    D3D12_GPU_DESCRIPTOR_HANDLE textureTable(UINT srv0, UINT srv1);
    // Before samplerTable and textureTable (and sceneTable: `descriptors` 5); false: lost.
    bool makeRoom(const StageSetup& s0, const StageSetup& s1, int aniso, UINT descriptors = 2, float bias = 0.0f);
    D3D12_GPU_DESCRIPTOR_HANDLE sceneTable(UINT t2, UINT t3, UINT t4, UINT t5 = kNoSrv);  // 0.19, kRootScene
    ID3D12PipelineState* pipelineFor(const DrawSetup& s, uint32_t fvf, bool coverage, bool toLayer = false,
                                     bool toGlow = false, bool water = false, bool mask = false,
                                     DrawShader shader = DrawShader::Game);
    bool createUpscalePipelines();  // the motion pass and the layer composite
    ID3D12PipelineState* pipelineForKey(uint64_t key);  // null when it cannot be built; remembered either way
    // draw.cpp
    void prepareGeometry(const FrameRecord& rec);
    void draw(const FrameRecord& rec, const DrawCmd& d);
    // effects.cpp (0.18)
    bool glowWanted() const { return effects.glow && effects.glowStrength > 0; }
    // From a world draw that adds light on: the glow target for the world being drawn, cleared; false when it cannot
    // be made (the glow stays off at this size).
    bool startGlow();
    void releaseGlow();
    void dropGlow();  // what `glow` holds goes unseen: the world it belonged to was cleared away
    // The light drawn so far onto the picture: `dst` of dstW x dstH (its RTV, format and samples). The glow is
    // ready for the next frame then. False: lost.
    bool addGlow(ID3D12Resource* dst, D3D12_RESOURCE_STATES& dstState, D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, int dstW,
                 int dstH, UINT dstSamples, bool upscaledFormat);
    bool addGlowToTarget();  // without DLSS: before the interface, a present or a read of the picture
    bool createGlowPipelines();
    // water.cpp (0.19)
    bool waterWanted() const { return effects.water; }
    // Before the frame's first cell of water: its picture and depth so far copied; false (and the water the game's)
    // when the camera is not known or the copies cannot be made. Once a frame (waterCopied).
    bool copyScene();
    // The world's picture and depth copies at its size (`user` names the effect in the log); the depth copied.
    bool ensureScene(const char* user);
    bool copySceneDepth();
    void releaseScene();
    void makeDepthViews(UINT n);  // the target's depth: its D32 view and its R32 one
    // torches.cpp (0.23)
    bool torchWanted() const { return effects.torch; }
    bool smokeWanted() const { return effects.smoke && smokePs && smokeGlowPs; }  // 0.24
    bool lampsWanted() const { return effects.lamps; }  // 0.25
    bool fireWanted() const { return effects.fireLight && effects.fireLightStrength > 0; }  // 0.26
    // `amount` and `colour`: the picture's fire, or (0.36) its own light.
    void noteFire(const FrameRecord& rec, const DrawCmd& d, float amount, const float colour[3]);
    bool fireflyWanted() const { return effects.fireflies && effects.firefliesBrightness > 0 && fireflyPs; }  // 0.28
    bool takeFirefly(const FrameRecord& rec, const DrawCmd& d);  // one the game draws: true when it is not to be
    bool fireflyReady();  // its pipelines for this target
    // 0.29: the maps' hidden lights (core/map_glows.hpp), from the map's file.
    bool mapGlowsWanted() const { return effects.mapGlows && effects.mapGlowsBrightness > 0 && fireflyPs; }
    bool raysWanted() const { return effects.rays && rayPs && rayGlowPs; }  // 0.30
    bool lavaWanted() const { return effects.lava && lavaPs && lavaGlowPs; }  // 0.31
    // 0.31: the map has lava - its file says so (water type 4), or it is one of UaRO.grf's three that do.
    bool lavaMap() const {
        const rsw::World* w = mapWorld.world();
        return (w && w->waterType == 4) || mapName == "mag_dun01" || mapName == "mag_dun02" || mapName == "tak_run";
    }
    void noteLava(const FrameRecord& rec, const DrawCmd& d);
    bool actorShadowsWanted() const {  // 0.32
        return effects.actorShadows && effects.actorShadowsStrength > 0 && shadowVs && shadowPs && shadowBlurPs &&
               shadowApplyPs;
    }
    bool actorShadowsDue() const { return actorShadowsWanted() && groundDrawn && !actorShadowsDone; }
    bool applyActorShadows(const FrameRecord& rec, size_t draw);  // once a frame; false: lost
    bool ensureShadowMask(int w, int h);
    void releaseShadowMask();
    ID3D12PipelineState* shadowApplyPipeline(UINT samples);
    void noteSpriteCells(Texture& t, const SpriteCell* slots, uint32_t count);
    bool cloudsWanted() const {
        return effects.clouds && effects.cloudsStrength > 0 && cloudsPs && cloudsApplyPs && mapOutdoors;
    }
    bool cloudsDue() const { return cloudsWanted() && groundDrawn && !cloudsDone && viewToWorldValid; }
    bool applyClouds();  // once a frame; false: lost
    bool ensureCloudsMask(int w, int h);  // 0.39.3
    void releaseCloudsMask();
    ID3D12PipelineState* cloudsPipeline(UINT samples);
    bool hazeWanted() const {
        return effects.haze && effects.hazeStrength > 0 && hazePs && (mapLook.haze > 0.0f || mapLook.fog > 0.0f);
    }
    bool hazeDue() const { return hazeWanted() && groundDrawn && !hazeDone && viewToWorldValid; }
    bool applyHaze();  // once a frame, the world drawn and lit; false: lost
    ID3D12PipelineState* hazePipeline(UINT samples);
    bool skyWanted() const { return effects.sky && effects.skyStrength > 0 && skyPs && mapOutdoors; }
    bool skyDue() const { return skyWanted() && groundDrawn && !skyDone && viewToWorldValid; }
    bool applySky();  // once a frame, the world drawn; false: lost
    // 0.72: indoors (a polished or wet floor) and in the rain; a field under a dry sky does not shine
    float floorReflectAmount() const {
        if (!effects.floorReflections || !floorReflectPs || !halfOverPs) return 0.0f;
        const float user = float(std::clamp(effects.floorReflectStrength, 0, 100)) / 100.0f;
        return mapWeather == weather::Kind::Rain && effects.weather ? std::min(1.0f, user * 1.6f)
               : mapOutdoors                                         ? 0.0f
                                                                     : user;
    }
    // 0.97: the ground wet where it rains - darker, puddles mirroring - by the floor's own pass
    bool wetWanted() const {
        return effects.wetGround && effects.weather && mapWeather == weather::Kind::Rain && floorReflectPs &&
               halfOverPs;
    }
    bool floorReflectDue() const {
        return (floorReflectAmount() > 0.0f || wetWanted()) && groundDrawn && !floorReflectDone && viewToWorldValid;
    }
    bool applyFloorReflections();  // once a frame, the world drawn and lit; false: lost
    bool shaftsWanted() const { return effects.shafts && effects.shaftsStrength > 0 && shaftsPs; }
    bool shaftsDue() const {
        return shaftsWanted() && groundDrawn && !shaftsDone && viewToWorldValid &&
               (shaftLights > 0 || (mapOutdoors && effects.skyRays));  // 2.4: the sky's rays a box of their own
    }
    bool applyShafts();  // once a frame, after the haze and the sky; false: lost
    void sunOnScreen(float out[2]) const;  // 0.73: the way the sun's shadows fall on the screen (actor_shadows.cpp)
    void sunGround(float out[3]) const;  // 2.0.1: the way the shadows fall along the ground, fixed in the world
    // 2.1.3: the map's own sun (its file's light: the way its painted shadows fall), the way to it in the world (up
    // along -y); false when the file or the maps' axes are not known
    bool mapSun(float toSun[3]) const;
    float sunHeight() const;  // 2.1.3: the sun above the horizon (radians) - the day's, or the map's without the cycle
    bool heatWanted() const { return effects.heat && effects.heatStrength > 0 && heatPs; }
    bool heatDue() const {
        return heatPs && groundDrawn && !heatDone && (!heatSpots.empty() || (heatWanted() && mapDesert));
    }
    bool portalsWanted() const { return effects.portals && effects.portalsStrength > 0; }  // 0.38
    float heatUser() const { return float(std::clamp(effects.heatStrength, 0, 200)) / 100.0f; }
    bool mapWeatherWanted() const {  // 0.39
        return effects.weather && effects.weatherAmount > 0 && weatherPs && mapWeather != weather::Kind::None;
    }
    // 0.70: motes in the air where the map has no weather - pollen under the open sky, dust elsewhere
    bool motesWanted() const {  // 2.4: the pollen outdoors and the dust indoors each a box of its own
        return effects.motes && effects.motesAmount > 0 && weatherPs && !mapName.empty() &&
               (mapOutdoors ? effects.pollen : effects.indoorDust);
    }
    bool weatherWanted() const { return mapWeatherWanted() || motesWanted(); }
    bool weatherDue() const { return weatherWanted() && groundDrawn && !weatherDone; }
    bool applyWeather();  // once a frame, the world finished; false: lost
    ID3D12PipelineState* weatherPipeline(UINT samples);
    ID3D12PipelineState* alphaPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[4], UINT samples,
                                       const char* what);  // 0.64
    ID3D12PipelineState* addPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[4], UINT samples,
                                     const char* what);  // 0.73
    ID3D12PipelineState* mulPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[4], UINT samples,
                                     const char* what);  // 0.77
    // mode: 0 by alpha, 1 added, 2 multiplied, 3 premultiplied (0.86)
    ID3D12PipelineState* blendPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[4], UINT samples,
                                       const char* what, int mode);
    void notePortal(const FrameRecord& rec, const DrawCmd& d);
    bool applyHeat();  // once a frame, after the torches' pass and the haze; false: lost
    ID3D12PipelineState* heatPipeline(UINT samples);
    void noteRay(const FrameRecord& rec, const DrawCmd& d);
    // An added-light pipeline of `ps` for this target (fireflies, dust); false (and `ps` dropped) when it fails.
    bool lightPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[5], const char* what);
    bool torchDue() const {
        return !torchDone && (torchWanted() || lampsWanted() || fireflyWanted() || mapGlowsWanted() ||
                              !torchFlames.empty() || !fireSparks.empty() || !rayParts.empty() ||
                              !lavaCells.empty() || !portalSparks.empty() || !portalTracks.empty());
    }
    bool takeTorch(const FrameRecord& rec, const DrawCmd& d);  // a torch layer: true when it is not to be drawn
    bool torchReady();     // the shaders, pipelines and the picture's copies are there
    bool applyTorches();   // once a frame (torchDone), the world drawn: before its colours; false: lost
    // A new picture (cleared, presented, another target): the world's effects are due again.
    void newPicture() {
        waterCopied = false;  // 0.19: its water copies what is under it anew
        aoDone = groundDrawn = gradeDone = dofDone = torchDone = false;  // 0.20-0.23
        nightDone = false;  // 0.63
        softCopied = 0;     // 0.67
        waterfallsDone = false;  // 0.65
        torchFlames.clear();
        fireSparks.clear();
        fireflyDraws.clear();
        rayParts.clear();
        lavaCells.clear();
        actorShadowsDone = actorShadowsDrawn = false;
        cloudsDone = false;
        sceneDepthFresh = false;
        hazeDone = false;
        skyDone = false;  // 0.69
        floorReflectDone = false;  // 0.72
        bloomDone = false;  // 0.78
        stepsDone = false;  // 0.80
        if (!frameFeet.empty()) {  // 0.94: the last frame's feet (they were drawn with it)
            footTracker.update(frameFeet, waterTime(), stepLife());
            frameFeet.clear();
        }
        grassDone = false;  // 0.81
        groundLifeDone = false;  // 1.7
        crittersDone = false;
        materialsMade = 0;  // 1.8
        if (hillsWanted()) hillsGrid = hills::build(groundTris);  // 1.15: before they go
        if (grassCellsWanted()) groundCells = ground_cells::build(groundTris);  // 2.2.1
        hillsUp = hillsTried = false;
        groundCellsUp = groundCellsTried = false;
        smoothReady = false;  // 1.19
        if (windShared()) updateWind();  // 1.22
        groundTris.clear();  // 1.9
        groundGroups.clear();
        groundTextures.clear();
        seamsDone = false;
        skyLightDone = false;  // 1.11
        if (mirrorCollecting()) {  // 1.16: the last frame's casters for this one's mirror
            mirrorVerts.swap(sunVerts);
            mirrorCasters.swap(sunCasters);
        }
        mirrorDone = mirrorMade = false;
        sunVerts.clear();  // 1.12
        sunCasters.clear();
        fogLightDone = false;  // 1.13
        frameCrowns.clear();
        waterLevelNoted = false;
        shaftsDone = false;  // 0.73
        flareDone = false;   // 0.98
        shaftLights = 0;
        heatDone = false;
        heatSpots.clear();
        portalSparks.clear();
        weatherDone = false;
    }
    // ao.cpp (0.20)
    bool aoWanted() const { return effects.ao && effects.aoStrength > 0; }
    bool aoDue() const { return aoWanted() && groundDrawn && !aoDone; }
    // 0.82: the skills' sparks (torches.cpp applyEmbers) - the places the torches' pass gathered the skills' fire at
    struct Ember {
        float x, y, size, strength, colour[3];
        bool fire;
    };
    std::vector<Ember> embers;
    ID3DBlob* embersPs = nullptr;
    ID3D12PipelineState* embersPipelines[4] = {};
    ID3D12Resource* embersData = nullptr;  // R32G32B32A32, two texels an ember place
    D3D12_RESOURCE_STATES embersDataState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT embersDataSrv = 0;
    static constexpr size_t kMaxEmbers = 24;
    bool embersDue() const { return effects.skillSparks && embersPs && !embers.empty(); }
    bool applyEmbers();  // after the torches' pass; false: lost
    // 1.7: the living world's creatures and particles (critters.cpp, critters_ps)
    critters::World critterWorld;
    std::vector<critters::Crown> frameCrowns;  // the trees' leaves drawn this frame
    ID3DBlob* crittersPs = nullptr;
    ID3DBlob* crittersVs = nullptr;
    ID3D12PipelineState* crittersPipeline(UINT samples);
    ID3D12PipelineState* crittersPipelines[4] = {};
    ID3D12Resource* crittersData = nullptr;  // R32G32B32A32, four texels a sprite
    D3D12_RESOURCE_STATES crittersDataState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT crittersDataSrv = 0;
    bool crittersDone = false, crittersLogged = false;
    float crittersTime = 0.0f, crittersStepsSeen = 0.0f;
    critters::Switches critterSwitches() const;
    bool crittersWanted() const;
    bool crittersDue() const { return !crittersDone && groundDrawn && crittersWanted(); }
    bool crittersNeedFeet() const { return effects.birds || effects.breath || effects.splashes; }
    void noteCrown(const FrameRecord& rec, const DrawCmd& d);
    // 1.8: the materials (materials.cpp) and the remaster (effects.tier)
    ID3DBlob* materialPs = nullptr;
    ID3D12PipelineState* materialPso = nullptr;
    int materialsMade = 0;  // this frame
    bool materialsLogged = false;
    int materialSide = 512;
    ID3D12PipelineState* materialPipeline();
    bool ensureMaterial(uint32_t id, Texture& t, bool fromAi);  // false: not ready yet
    void dropMaterial(Texture& t);
    bool materialsWanted() const { return effects.materials && effects.materialsStrength > 0 && materialPs; }
    void materialConstants(bool ground, Constants& c) const;  // a draw with its material
    // 1.9: the ground's seams (seams.cpp) - the frame's ground cells in the world, then strips across their edges
    struct GroundGroup {
        DrawSetup state;
        uint32_t texture[2] = {0, 0};
        bool keyed[2] = {false, false};
    };
    std::vector<seams::Triangle> groundTris;
    std::vector<GroundGroup> groundGroups;
    std::vector<uint32_t> groundTextures;
    FrameRecord seamRec;
    bool seamPass = false, seamsDone = false, seamsLogged = false;
    // 2.2.1: outdoors only - a dungeon's floor is tiles (Glast Heim's church: stone and black broken ones), whose
    // edges are meant straight: the wandering line spread the black over the stones as cracks, and blinked
    bool seamsWanted() const { return effects.groundSeams && viewToWorldValid && mapOutdoors; }
    // 1.19: smooth light on the models (core/smooth_shade.hpp) - the game's record's colours changed as its vertices
    // go up (prepareGeometry), worked out once a frame
    struct ColourChange {
        size_t at;  // the colour's byte in the record's vertices
        uint32_t colour;
    };
    std::vector<ColourChange> smoothColours;
    const void* smoothFor = nullptr;
    bool smoothReady = false, smoothLogged = false;
    bool smoothModelsWanted() const { return effects.smoothModels; }
    void smoothModels(const FrameRecord& rec);
    // 1.15: smooth hills (hills.cpp, core/hills.hpp) - the last frame's ground's corners' normals, a texture of them
    // (R32_FLOAT, kMostSide a side) put up at the frame's first ground draw, read by the ground's draws (t3)
    hills::Grid hillsGrid;
    ID3D12Resource* hillsData = nullptr;
    D3D12_RESOURCE_STATES hillsDataState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT hillsDataSrv = 0;
    bool hillsUp = false, hillsTried = false, hillsLogged = false, hillsFailed = false;
    bool hillsWanted() const {
        return effects.smoothHills && effects.smoothHillsStrength > 0 && viewToWorldValid;
    }
    bool ensureHills();  // the texture holds this frame's grid; false: none this frame
    // 2.2.1: the ground's heights a cell (core/ground_cells.hpp) - the grass on the map's ground only (grass_ps, t5)
    ground_cells::Grid groundCells;
    ID3D12Resource* groundCellsData = nullptr;
    D3D12_RESOURCE_STATES groundCellsState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT groundCellsSrv = kNoSrv;
    bool groundCellsUp = false, groundCellsTried = false, groundCellsFailed = false, groundCellsLogged = false;
    bool grassCellsWanted() const {
        return effects.grass && effects.grassAmount > 0 && grassPs && mapOutdoors && viewToWorldValid;
    }
    bool ensureGroundCells();  // the texture holds this frame's cells; false: none this frame
    void hillsConstants(Constants& c) const;
    void sunInWorld(float out[3]) const;  // the way to the sun (as the materials find it), up along -y
    bool seamsDue() const { return seamsWanted() && !seamsDone && !groundTris.empty(); }
    void noteGround(const FrameRecord& rec, const DrawCmd& d);
    bool applySeams();  // once a frame, the ground drawn; false: lost
    // 1.11: light from the sky and from around (ao.cpp applySkyLight, sky_light_ps)
    ID3DBlob* skyLightPs = nullptr;
    ID3D12PipelineState* skyLightPipelines[4] = {};
    bool skyLightDone = false, skyLightLogged = false;
    bool skyLightWanted() const { return skyLightPs && (effects.skyLight || effects.bounceLight); }
    bool skyLightDue() const { return skyLightWanted() && groundDrawn && !skyLightDone && viewToWorldValid; }
    bool applySkyLight();  // once a frame, before the characters; false: lost
    // 1.12: the frame's opaque world (sun_map.cpp noteCaster) - since 2.2 for the mirror only
    struct SunCaster {
        uint32_t first = 0, count = 0;  // vertices into sunVerts (six floats each: x, y, z, u, v, colour's bits)
        uint32_t texture = 0;
        bool alphaTest = false;
        float ref = 0.5f;
    };
    std::vector<float> sunVerts;
    std::vector<SunCaster> sunCasters;
    // 1.16: the world mirrored under the water (mirror.cpp) - the last frame's casters turned over the water's level,
    // drawn at half the scene's size before the frame's first water, read by water_ps (t5)
    std::vector<float> mirrorVerts;
    std::vector<SunCaster> mirrorCasters;
    ID3D12Resource* mirror = nullptr;
    ID3D12Resource* mirrorDepth = nullptr;
    D3D12_RESOURCE_STATES mirrorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES mirrorDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    UINT mirrorSrv = kNoSrv;
    int mirrorWidth = 0, mirrorHeight = 0;
    ID3DBlob* mirrorVs = nullptr;
    ID3DBlob* mirrorPs = nullptr;
    ID3D12PipelineState* mirrorPso = nullptr;
    bool mirrorDone = false, mirrorMade = false, mirrorLogged = false, mirrorFailed = false;
    bool mirrorCollecting() const {
        return effects.worldReflections && effects.water && remaster() && mirrorVs && mirrorPs && !mirrorFailed &&
               viewToWorldValid;
    }
    bool mirrorDue() const { return mirrorCollecting() && waterLevelKnown && !mirrorDone && !mirrorCasters.empty(); }
    ID3D12PipelineState* mirrorPipeline();
    bool applyMirror();  // once a frame, before its first water; false: lost
    void noteCaster(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s);
    // 1.17, living characters (draw.cpp): a sprite lit by the sun from its silhouette's sides (shaders.cpp
    // spriteLight, stage 0 bit 28)
    bool spriteLightWanted() const { return effects.spriteLight && effects.spriteLightStrength > 0; }
    void spriteConstants(Constants& c) const;
    // 1.13: volumetric fog lit by the sun (sun_map.cpp applyFog, fog_light_ps)
    ID3DBlob* fogLightPs = nullptr;
    ID3D12PipelineState* fogLightPipelines[4] = {};
    bool fogLightDone = false, fogLightLogged = false;
    bool fogLightDue() const {
        return effects.volumetricFog && effects.volumetricFogDensity > 0 && fogLightPs && remaster() && mapOutdoors &&
               viewToWorldValid && cameraValid && cameraTargetZ > 0.0f && groundDrawn && !fogLightDone;
    }
    bool applyFog();  // once a frame, the world drawn; false: lost
    bool remaster() const {
        return effects.tier == 2 || (effects.tier == 0 && adapterMb >= 6000);  // auto: a card of 6 GB and more
    }
    bool applyCritters();  // after the torches' pass; false: lost
    // 0.81: grass on the green ground (ao.cpp applyGrass), off by default: it is costly
    ID3DBlob* grassPs = nullptr;
    ID3D12PipelineState* grassPipelines[4] = {};
    bool grassDone = false;
    bool grassLogged = false;
    bool grassDue() const {
        return effects.grass && effects.grassAmount > 0 && grassPs && mapOutdoors && groundDrawn && !grassDone &&
               viewToWorldValid;
    }
    bool applyGrass();  // once a frame, before the characters; false: lost
    // 1.7: the living world's ground (ao.cpp applyGroundLife): sand drifting, snow glittering, damp walls glistening
    ID3DBlob* groundLifePs = nullptr;
    ID3D12PipelineState* groundLifePipelines[4] = {};
    bool groundLifeDone = false, groundLifeLogged = false;
    bool groundLifeWanted() const {
        return groundLifePs && ((effects.sandDrift && mapPlace.sand) || (effects.snowSparkle && mapPlace.snow) ||
                                (effects.wetWalls && mapPlace.cave) || wetSandWanted() || seasonsShown() ||
                                rainWetShown());
    }
    // 1.20: the year on the whole map, the rain's wet on the walls and roofs (ground_life_ps)
    bool seasonsShown() const {
        return effects.seasons && ((effects.seasonSnow && mapYear.snow > 0.0f) ||
                                   (effects.seasonGrass && (mapYear.dry > 0.0f || mapYear.lush > 0.0f)));
    }
    bool rainWetShown() const { return effects.rainWet && effects.weather && mapWeather == weather::Kind::Rain; }
    bool wetSandWanted() const { return effects.shoreFoam && effects.wetSand && waterLevelKnown && mapOutdoors; }
    bool groundLifeDue() const { return groundLifeWanted() && groundDrawn && !groundLifeDone && viewToWorldValid; }
    bool applyGroundLife();  // once a frame, before the characters; false: lost
    float waterLife() const;  // 1.7: water_ps's bits (draw.cpp)
    void noteWaterLevel(const FrameRecord& rec, const DrawCmd& d);
    float waterLevel = 0.0f;  // the last water seen, its height in the world (up is -y)
    bool waterLevelKnown = false, waterLevelNoted = false;
    // 0.80: footprints and dust (ao.cpp applySteps) - the characters' feet followed (actor_shadows.cpp)
    footsteps::Tracker footTracker;
    ID3DBlob* stepsPs = nullptr;
    ID3D12PipelineState* stepsPipelines[4] = {};
    ID3D12Resource* stepsData = nullptr;  // R32G32B32A32, two texels a step
    D3D12_RESOURCE_STATES stepsDataState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT stepsDataSrv = 0;
    bool stepsDone = false;
    bool stepsLogged = false;
    int stepKind() const {  // 1 snow, 2 sand, 3 dust, 0 none
        if (!effects.footprints || !stepsPs) return 0;
        return mapWeather == weather::Kind::Snow ? 1 : mapDesert ? 2 : mapOutdoors && effects.stepDust ? 3 : 0;
    }
    float stepLife() const {
        const float prints = stepKind() == 3 ? 0.9f : stepKind() == 1 ? 25.0f : stepKind() ? 14.0f : 0.0f;
        return std::max(std::max(prints, ripplesWanted() ? kRippleLife : 0.0f), crittersNeedFeet() ? 2.0f : 0.0f);
    }
    // 0.94: rings on the water where the steps go (water_ps) - the feet followed for them too, shadows on or off
    static constexpr float kRippleLife = 1.6f;
    bool ripplesWanted() const { return effects.ripples && effects.water && waterPs; }
    bool feetWanted() const { return stepKind() || ripplesWanted() || crittersNeedFeet(); }
    std::vector<map::Point> frameFeet;  // the round shadows' middles drawn this frame, in the world
    void noteFoot(const FrameRecord& rec, const DrawCmd& d);
    bool stepsDue() const { return stepKind() && groundDrawn && !stepsDone && viewToWorldValid; }
    bool applySteps();  // once a frame, the world finished; false: lost
    float flashNow() const;  // 0.80: the thunderstorm's lightning now (0-1), where it rains
    // 0.78: bloom and the filmic curve (ao.cpp)
    ID3DBlob* bloomDownPs = nullptr;
    ID3DBlob* bloomApplyPs = nullptr;
    ID3D12PipelineState* bloomDownPipeline = nullptr;
    ID3D12PipelineState* bloomBlurPipeline = nullptr;
    ID3D12PipelineState* bloomApplyPipelines[4] = {};
    ID3D12Resource* bloomTarget[2] = {};
    D3D12_RESOURCE_STATES bloomState[2] = {D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET};
    UINT bloomSrv[2] = {};
    int bloomWidth = 0, bloomHeight = 0;
    bool bloomFailed = false;
    bool bloomDone = false;
    bool bloomWanted() const {  // 0.83: the film's grain and the new map's fade go with it
        return (effects.bloom || effects.filmic || (effects.filmGrain && effects.filmGrainAmount > 0) ||
                mapFadeNow() < 1.0f) &&
               bloomDownPs && bloomApplyPs && dofBlurPs;
    }
    float mapFadeStart = -1.0f;  // 0.83: the effects' clock when the map changed; below 0 none
    float mapFadeNow() const {  // 1 when done
        if (!effects.mapFade || mapFadeStart < 0.0f) return 1.0f;
        const float t = (const_cast<Impl*>(this)->waterTime() - mapFadeStart) / 0.7f;
        return t >= 1.0f ? 1.0f : t <= 0.0f ? 0.0f : t * t * (3.0f - 2.0f * t);
    }
    bool bloomDue() const { return bloomWanted() && groundDrawn && !bloomDone; }
    bool applyBloom();  // once a frame, the world finished, before its colours; false: lost
    void releaseBloom();
    // 0.77: the sun's shadows of the houses, walls and trees - under the open sky, by day
    bool applyAo();  // once a frame (aoDone); false: lost
    bool gradeWanted() const { return effects.grade; }
    bool nightDue() const {  // 0.74: also for the time of day's tint; 0.79: and the region's look
        return ((effects.night && effects.nightNow > 0) || (effects.dayCycle && effects.dayNow >= 0) ||
                (effects.regionLooks && mapRegion) || flashNow() > 0.0f) &&
               nightPs && groundDrawn && !nightDone;
    }
    bool mapRegion = false;  // 0.79: the map is of a region with a look of its own (core/region_look.hpp)
    bool applyNight();  // 0.63: once a frame, before the torches' light; false: lost
    bool waterfallsDue() const {
        return effects.water && effects.waterfalls && waterfallPs && groundDrawn && !waterfallsDone;
    }
    bool applyWaterfalls();  // 0.65: once a frame, after the torches; false: lost
    ID3D12PipelineState* waterfallPipeline(UINT samples);
    bool gradeDue() const { return gradeWanted() && groundDrawn && !gradeDone; }
    bool applyGrade();  // once a frame (gradeDone), the world drawn: before its glow and the interface; false: lost
    // The world's shadows if they are due, then its colours: before the interface, a present or a read of the
    // picture (and under DLSS before the upscale). False: lost.
    bool finishWorld();
    bool copySceneColor();  // the world's picture so far into sceneColor, one sample per pixel
    bool dofWanted() const { return effects.dof && effects.dofStrength > 0; }
    bool dofDue() const { return dofWanted() && groundDrawn && !dofDone; }
    bool applyDof();  // once a frame (dofDone), after the colours; false: lost
    void releaseDof();
    bool ensureAo(int w, int h);
    void releaseAo();
    ID3D12PipelineState* aoApplyPipeline(UINT samples);
    // One full-screen pass into dst (dstW x dstH): the constants, t0 = srv0 (sampled linearly, clamped) and, with
    // scene, the picture and depth copies (t2-t4). The sources are in their shader state already.
    bool effectPass(ID3D12PipelineState* pso, ID3D12Resource* dst, D3D12_RESOURCE_STATES& dstState,
                    D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, int dstW, int dstH, Constants& c, UINT srv0, bool scene,
                    UINT srv1 = kNoSrv,                 // t1: srv0 again unless given
                    const D3D12_RECT* clip = nullptr,   // 0.23: only these pixels
                    UINT vertices = 3,                  // 1.7: the vertex shader's own quads (critters_vs)
                    UINT sceneT5 = kNoSrv);             // 2.2.1: the scene table's t5 (the grass: the ground)
    // 2.2.2: an effect pass over the world's picture - the upscaler's world while it upscales (DLSS, TAA, ui_native),
    // else the target (twenty passes wrote both calls out, the same arguments twice)
    bool worldPass(ID3D12PipelineState* pso, int w, int h, Constants& c, UINT srv0, bool scene, UINT srv1 = kNoSrv,
                   const D3D12_RECT* clip = nullptr, UINT vertices = 3, UINT sceneT5 = kNoSrv) {
        return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, srv0, scene, srv1, clip,
                                    vertices, sceneT5)
                       : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, srv0, scene, srv1, clip,
                                    vertices, sceneT5);
    }
    float waterTime();            // seconds since the first water
    ID3D12PipelineState* glowAddPipeline(UINT samples, bool upscaledFormat);
    ID3D12PipelineState* glowLightPipeline(UINT samples, bool upscaledFormat);
    // One full-screen pass of the glow: src (sampled linearly, clamped) into dst; blendFactor with glow_up_ps.
    bool glowPass(ID3D12PipelineState* pso, ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState, UINT srcSrv,
                  int srcW, int srcH, ID3D12Resource* dst, D3D12_RESOURCE_STATES& dstState,
                  D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, int dstW, int dstH, float strength, bool first,
                  float blendFactor = 1.0f);
};

// 0.39.2: an effect timed for as long as it runs (Renderer::Impl::effectBegin, effectEnd).
struct EffectTime {
    Renderer::Impl& m;
    EffectTimer e;
    EffectTime(Renderer::Impl& impl, EffectTimer which) : m(impl), e(which) { m.effectBegin(e); }  // and a crumb
    ~EffectTime() { m.effectEnd(e); }
    EffectTime(const EffectTime&) = delete;
    EffectTime& operator=(const EffectTime&) = delete;
};

}  // namespace uagfx::gpu
