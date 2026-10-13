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
#include "win/gpu/renderer.hpp"

namespace uagfx::gpu {

template <typename T>
void releaseCom(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

constexpr UINT kSlots = 2;              // submissions in flight
constexpr UINT kCpuSrvCount = 65536;    // one SRV per texture (the client keeps thousands), the white one, the target
constexpr UINT kNoSrv = UINT(-1);       // allocSrv: the heap is full
constexpr UINT kGpuSrvCount = 65536;    // texture pairs bound for drawing: each slot's list has its own part
constexpr UINT kRingPerSlot = kGpuSrvCount / kSlots;
constexpr UINT kSamplerCount = 1024;    // cache of sampler pairs, two descriptors each
constexpr UINT64 kUploadSize = 32ull << 20;  // per slot; grows when one submission needs more

// Render target views: the target, the two swap chain buffers, the game-sized helper, a new target being filled.
constexpr UINT kRtvTarget = 0;
constexpr UINT kRtvSwap = 1;
constexpr UINT kRtvGame = 3;
constexpr UINT kRtvNew = 4;
constexpr UINT kRtvCount = 5;

constexpr UINT kRootConstants = 0;
constexpr UINT kRootTextures = 1;
constexpr UINT kRootSamplers = 2;

// Root constants; must match cbuffer Params in shaders.cpp.
struct Constants {
    float targetSize[2];   // 0-1: the target's size in pixels
    uint32_t stage0[2];    // 2-3: colorOp | colorArg1 << 8 | colorArg2 << 16 | alphaOp << 24, alphaArg1 | alphaArg2 << 8 | texCoord << 16 | keyed << 24
    uint32_t stage1[2];    // 4-5
    uint32_t alphaTest;    // 6: func | ref << 8 | enabled << 16
    uint32_t fogEnable;    // 7
    float fogColor[4];     // 8-11
    uint32_t colorKey;     // 12
    uint32_t uiMode;       // 13: uiFilterMode() - 0 none, 1 sharp, 2 smooth (interface draws at k > 1)
    float scale;           // 14: k, game pixels -> target pixels (stage 3)
    float footprint;       // 15: blit only - source texels per destination pixel
    float sourceRect[4];   // 16-19: blit only, normalised u0 v0 u1 v1
    float uiPixels[4];     // 20-23: uiMode only - the quad's game pixels: first x, first y, last x, last y
    float sourceClamp[4];  // 24-27: blit only - the outermost texel centres of the source rectangle, normalised
    float sharpen;         // 28: blit only - CAS sharpness 0..1 of the picture on screen, below 0 = none
};
constexpr UINT kConstantCount = sizeof(Constants) / 4;
static_assert(kConstantCount == 29, "Constants must stay 29 DWORDs");

struct Texture {
    ID3D12Resource* resource = nullptr;
    int width = 0;
    int height = 0;
    int levels = 1;
    UINT srv = 0;  // index in the CPU SRV heap
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
};

struct Slot {
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12Resource* upload = nullptr;
    uint8_t* mapped = nullptr;
    UINT64 size = 0;
    UINT64 used = 0;
    UINT64 fence = 0;                        // signalled when this slot's last submission finished
    std::vector<IUnknown*> garbage;          // released once that fence has passed
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

    ID3D12Resource* target = nullptr;
    ID3D12Resource* depth = nullptr;
    D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT targetSrv = 0;
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
    ID3D12PipelineState* presentPipeline = nullptr;
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
    LARGE_INTEGER lastPresent{};

    // Geometry of the record being executed, copied into the upload buffer once per execute().
    D3D12_GPU_VIRTUAL_ADDRESS vertexBase = 0;
    D3D12_GPU_VIRTUAL_ADDRESS indexBase = 0;
    UINT indexBytes = 0;
    UINT64 geometryFence = 0;  // fenceValue when that geometry was uploaded: a later list uploads it again
    // Bindings already set on the open command list (reset by begin()).
    ID3D12PipelineState* boundPipeline = nullptr;
    uint64_t boundTextures = ~0ull;
    uint64_t boundSamplers = ~0ull;
    bool targetBound = false;

    // device.cpp
    bool begin();
    void submit();
    void waitSlot(Slot& s);
    void waitIdle();
    void retireDying();  // after execute() has submitted: the dying textures go with that submission
    void failed(uint32_t place, const char* what, HRESULT hr);
    bool createTarget(int w, int h, ID3D12Resource*& rt, ID3D12Resource*& ds);
    bool scaled() const { return width != gameWidth || height != gameHeight; }
    void applyViewport();
    void blit(ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState, UINT srcSrv, int srcW, int srcH,
              const float srcRect[4], D3D12_CPU_DESCRIPTOR_HANDLE dst, const Rect& dstRect, int dstW, int dstH,
              float sharpen = -1.0f);
    bool growUpload(Slot& s, UINT64 size);
    Upload allocUpload(UINT64 size, UINT64 alignment);
    void transition(ID3D12Resource* r, D3D12_RESOURCE_STATES& state, D3D12_RESOURCE_STATES to);
    UINT allocSrv();
    D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvHandle(UINT i) const;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(UINT i) const;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle() const;
    bool ensureSwapChain(HWND hwnd, UINT w, UINT h, bool wantTearing);
    void releaseSwapChain();
    void clear(const ClearCmd& c);
    void setViewport(const ViewportCmd& v);
    void upload(const FrameRecord& rec, const UploadCmd& u);
    void writeTarget(const FrameRecord& rec, const WriteTargetCmd& w);
    void bindTarget();
    void destroy();
    // pipeline.cpp
    bool createPipelines(std::string& error);
    D3D12_GPU_DESCRIPTOR_HANDLE samplerTable(const StageSetup& s0, const StageSetup& s1);
    D3D12_GPU_DESCRIPTOR_HANDLE textureTable(UINT srv0, UINT srv1);
    bool makeRoom(const StageSetup& s0, const StageSetup& s1);  // before samplerTable and textureTable; false: lost
    ID3D12PipelineState* pipelineFor(const DrawSetup& s, uint32_t fvf);
    ID3D12PipelineState* pipelineForKey(uint64_t key);  // null when it cannot be built; remembered either way
    // draw.cpp
    void prepareGeometry(const FrameRecord& rec);
    void draw(const FrameRecord& rec, const DrawCmd& d);
};

}  // namespace uagfx::gpu
