#pragma once
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/frame.hpp"
#include "core/key_pair.hpp"
#include "core/log_once.hpp"
#include "core/retry_timer.hpp"
#include "core/map_objects.hpp"
#include "core/outdoors.hpp"
#include "core/pipeline_key.hpp"
#include "core/torch.hpp"
#include "win/gpu/renderer.hpp"
#include "win/gpu/map_world.hpp"
#include "win/gpu/upscaler.hpp"

namespace uagfx::gpu {

class AiTextures;  // ai_textures.hpp

// A pipeline that draws one triangle over the viewport into a target of that format and samples (ao.cpp).
D3D12_GRAPHICS_PIPELINE_STATE_DESC fullScreenPipeline(ID3D12RootSignature* root, ID3DBlob* vs, ID3DBlob* ps,
                                                      DXGI_FORMAT format, UINT samples);

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
constexpr UINT kTimes = 6;              // GPU timestamps a slot: its list, the upscale, the AI textures' work
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
constexpr UINT kRtvCount = kRtvShadow + 2;
constexpr UINT kDsvTarget = 0;  // depth views: the target's, the world's (DLSS)
constexpr UINT kDsvWorld = 1;

// What the open command list draws into (bindPass); the glow's passes add its target as the second one (0.18).
enum Pass { kPassNone = 0, kPassTarget, kPassWorld, kPassLayer, kPassTargetGlow, kPassWorldGlow };

constexpr UINT kRootConstants = 0;
constexpr UINT kRootTextures = 1;
constexpr UINT kRootSamplers = 2;
constexpr UINT kRootScene = 3;  // 0.19: t2-t4, the picture and its depth before the frame's first water

// Root constants; must match cbuffer Params in shaders.cpp.
struct Constants {
    float targetSize[2];   // 0-1: the target's size in pixels
    uint32_t stage0[2];    // 2-3: colorOp | colorArg1 << 8 | colorArg2 << 16 | alphaOp << 24, alphaArg1 | alphaArg2 << 8 | texCoord << 16 | keyed << 24
    uint32_t stage1[2];    // 4-5
    uint32_t alphaTest;    // 6: func | ref << 8 | enabled << 16 | coverage << 17 | opaque in the layer << 18
    uint32_t fogEnable;    // 7
    float fogColor[4];     // 8-11
    uint32_t colorKey;     // 12
    uint32_t uiMode;       // 13: uiFilterMode() - 0 none, 1 sharp, 2 smooth (interface draws at k > 1); 3 a twin
    float scale;           // 14: k, game pixels -> target pixels (stage 3)
    float footprint;       // 15: blit only - source texels per destination pixel
    float sourceRect[4];   // 16-19: blit only, normalised u0 v0 u1 v1
    float uiPixels[4];     // 20-23: uiMode only - the quad's game pixels: first x, first y, last x, last y
    float sourceClamp[4];  // 24-27: blit only - the outermost texel centres of the source rectangle, normalised
    float sharpen;         // 28: blit only - CAS sharpness 0..1 of the picture on screen, below 0 = none
    float twin[4];         // 29-32: uiMode 3 only (stage 6) - the twin texel origin O (x, y), the twin texture's size
    float jitter[2];       // 33-34: part 3 (DLSS) - the world's geometry moved this frame, target pixels
    float reproject[12];   // 35-46: motion only - this view -> the previous one (4 x 3)
    float projection[6];   // 47-52: motion only - f0, f1, cx, cy, near, far (core/game_camera.hpp)
};
constexpr UINT kConstantCount = sizeof(Constants) / 4;
static_assert(kConstantCount == 53, "Constants must stay 53 DWORDs: the cbuffer in shaders.cpp");

struct Texture {
    ID3D12Resource* resource = nullptr;
    int width = 0;
    int height = 0;
    int levels = 1;
    UINT srv = 0;  // index in the CPU SRV heap
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
    // AI textures (ai_textures.hpp): the network's version, drawn in the world instead while it is shown.
    ID3D12Resource* ai = nullptr;
    D3D12_RESOURCE_STATES aiState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT aiSrv = 0;
    // Where it stands in: the world's draws, the interface's (0.14.1: the sprite cache's copy for the cursor, the
    // loading screen and other big pictures of the interface).
    bool aiWorld = true;
    bool aiUi = false;
    // 0.26: how much its picture looks like fire (core/effect_light.hpp) and its colour - an effect drawn from it
    // lights around it.
    float fire = 0.0f;
    float fireColour[3] = {0, 0, 0};
    // 0.32: a sprite cache's cells and what each holds now (sprite_exe::Source) - the characters cast shadows.
    std::vector<SpriteCell> spriteCells;
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
};

struct Upload {
    uint8_t* cpu;
    D3D12_GPU_VIRTUAL_ADDRESS gpu;
    UINT64 offset;  // in the slot's upload buffer
};

struct Renderer::Impl {
    HMODULE d3d12 = nullptr;
    HMODULE dxgi = nullptr;
    HMODULE compiler = nullptr;
    IDXGIFactory4* factory = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    // Stage 4 review: sample positions - every sample at the pixel's centre for interface draws, whose edges fall on
    // pixel centres; null when the card cannot place samples, and then MSAA stays off.
    ID3D12GraphicsCommandList1* list1 = nullptr;
    bool centered = false;  // the open list has them at the centre now
    ID3D12Fence* fence = nullptr;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValue = 0;
    Slot slots[kSlots];
    UINT slot = 0;
    bool open = false;
    std::string adapter;
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
    ID3DBlob* torchLightPs = nullptr;
    ID3DBlob* torchFirePs = nullptr;
    ID3DBlob* smokePs = nullptr;  // 0.24: the chimneys' smoke (draw.cpp), with the glow's second target and without
    ID3DBlob* smokeGlowPs = nullptr;
    ID3D12PipelineState* torchLightPipelines[4] = {};  // into the target of 1, 2, 4, 8 samples (the world: 1)
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
    // 0.33, the clouds' shadows (clouds.cpp) over the open sky's maps.
    ID3DBlob* cloudsPs = nullptr;
    ID3D12PipelineState* cloudsPipelines[4] = {};
    bool cloudsDone = false;
    bool cloudsLogged = false;
    bool mapOutdoors = false;  // the map is a field or a town (core/outdoors.hpp)
    ID3DBlob* lavaPs = nullptr;     // 0.31: a cell of lava (draw.cpp), with the glow's second target
    ID3DBlob* lavaGlowPs = nullptr;
    std::string mapName;                    // the map the game shows (setMap)
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
    LARGE_INTEGER lastPresent{};

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
    D3D12_GPU_DESCRIPTOR_HANDLE samplerTable(const StageSetup& s0, const StageSetup& s1, int aniso);
    D3D12_GPU_DESCRIPTOR_HANDLE textureTable(UINT srv0, UINT srv1);
    // Before samplerTable and textureTable (and sceneTable: `descriptors` 5); false: lost.
    bool makeRoom(const StageSetup& s0, const StageSetup& s1, int aniso, UINT descriptors = 2);
    D3D12_GPU_DESCRIPTOR_HANDLE sceneTable(UINT t2, UINT t3, UINT t4);  // 0.19, kRootScene
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
    void noteFire(const FrameRecord& rec, const DrawCmd& d, const Texture& t);
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
    bool cloudsWanted() const { return effects.clouds && effects.cloudsStrength > 0 && cloudsPs && mapOutdoors; }
    bool cloudsDue() const { return cloudsWanted() && groundDrawn && !cloudsDone && viewToWorldValid; }
    bool applyClouds();  // once a frame; false: lost
    ID3D12PipelineState* cloudsPipeline(UINT samples);
    void noteRay(const FrameRecord& rec, const DrawCmd& d);
    // An added-light pipeline of `ps` for this target (fireflies, dust); false (and `ps` dropped) when it fails.
    bool lightPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&pipelines)[5], const char* what);
    bool torchDue() const {
        return !torchDone && (torchWanted() || lampsWanted() || fireflyWanted() || mapGlowsWanted() ||
                              !torchFlames.empty() || !fireSparks.empty() || !rayParts.empty() ||
                              !lavaCells.empty());
    }
    bool takeTorch(const FrameRecord& rec, const DrawCmd& d);  // a torch layer: true when it is not to be drawn
    bool torchReady();     // the shaders, pipelines and the picture's copies are there
    bool applyTorches();   // once a frame (torchDone), the world drawn: before its colours; false: lost
    // A new picture (cleared, presented, another target): the world's effects are due again.
    void newPicture() {
        waterCopied = false;  // 0.19: its water copies what is under it anew
        aoDone = groundDrawn = gradeDone = dofDone = torchDone = false;  // 0.20-0.23
        torchFlames.clear();
        fireSparks.clear();
        fireflyDraws.clear();
        rayParts.clear();
        lavaCells.clear();
        actorShadowsDone = actorShadowsDrawn = false;
        cloudsDone = false;
    }
    // ao.cpp (0.20)
    bool aoWanted() const { return effects.ao && effects.aoStrength > 0; }
    bool aoDue() const { return aoWanted() && groundDrawn && !aoDone; }
    bool applyAo();  // once a frame (aoDone); false: lost
    bool gradeWanted() const { return effects.grade; }
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
                    const D3D12_RECT* clip = nullptr);  // 0.23: only these pixels
    float waterTime();            // seconds since the first water
    ID3D12PipelineState* glowAddPipeline(UINT samples, bool upscaledFormat);
    ID3D12PipelineState* glowLightPipeline(UINT samples, bool upscaledFormat);
    // One full-screen pass of the glow: src (sampled linearly, clamped) into dst; blendFactor with glow_up_ps.
    bool glowPass(ID3D12PipelineState* pso, ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState, UINT srcSrv,
                  int srcW, int srcH, ID3D12Resource* dst, D3D12_RESOURCE_STATES& dstState,
                  D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, int dstW, int dstH, float strength, bool first,
                  float blendFactor = 1.0f);
};

}  // namespace uagfx::gpu
