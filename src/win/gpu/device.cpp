#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>

#include "core/d3d7.hpp"
#include "core/pipeline_key.hpp"
#include "core/scale.hpp"
#include "core/window_plan.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::gpu {
namespace {

UINT64 alignUp(UINT64 v, UINT64 a) { return (v + a - 1) & ~(a - 1); }

std::string narrow(const wchar_t* w) {
    char out[512];
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, sizeof out, nullptr, nullptr);
    return n > 0 ? std::string(out) : std::string();
}

ID3D12Resource* createBuffer(ID3D12Device* device, D3D12_HEAP_TYPE type, UINT64 size, D3D12_RESOURCE_STATES state) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = type;
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* r = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, state, nullptr,
                                               __uuidof(ID3D12Resource), reinterpret_cast<void**>(&r))))
        return nullptr;
    return r;
}

ID3D12Resource* createTexture2D(ID3D12Device* device, int w, int h, int levels, DXGI_FORMAT format,
                                D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = UINT64(w);
    d.Height = UINT(h);
    d.DepthOrArraySize = 1;
    d.MipLevels = UINT16(levels);
    d.Format = format;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d.Flags = flags;
    ID3D12Resource* r = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, state, clear,
                                               __uuidof(ID3D12Resource), reinterpret_cast<void**>(&r))))
        return nullptr;
    return r;
}

}  // namespace

// ---- Impl: submissions -------------------------------------------------------------------------------------

bool Renderer::Impl::begin() {
    if (open) return true;
    if (lost) return false;
    Slot& s = slots[slot];
    waitSlot(s);
    for (IUnknown* g : s.garbage) g->Release();
    s.garbage.clear();
    s.used = 0;
    ringSrv = 0;  // this list's part of the descriptor ring is free: the slot's previous list has finished
    HRESULT hr = s.allocator->Reset();
    if (SUCCEEDED(hr)) hr = list->Reset(s.allocator, nullptr);
    if (FAILED(hr)) {
        failed(1, "command list reset", hr);
        return false;
    }
    ID3D12DescriptorHeap* heaps[2] = {gpuSrv, samplerHeap};
    list->SetDescriptorHeaps(2, heaps);
    boundPipeline = nullptr;
    boundTextures = ~0ull;
    boundSamplers = ~0ull;
    targetBound = false;
    open = true;
    return true;
}

void Renderer::Impl::submit() {
    if (!open) return;
    open = false;
    const HRESULT hr = list->Close();
    if (FAILED(hr)) {
        failed(2, "command list close", hr);
        return;
    }
    ID3D12CommandList* lists[1] = {list};
    queue->ExecuteCommandLists(1, lists);
    queue->Signal(fence, ++fenceValue);
    slots[slot].fence = fenceValue;
    slot = (slot + 1) % kSlots;
}

void Renderer::Impl::waitSlot(Slot& s) {
    if (s.fence && fence->GetCompletedValue() < s.fence) {
        fence->SetEventOnCompletion(s.fence, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

void Renderer::Impl::waitIdle() {
    submit();
    if (lost) return;  // a removed device's fence never moves on
    queue->Signal(fence, ++fenceValue);
    if (fence->GetCompletedValue() < fenceValue) {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

// A failed call is logged once per place and code. A removed device (driver crash, TDR) stops all further GPU work
// with its reason in the log, instead of failing - and waiting - again every frame.
void Renderer::Impl::failed(uint32_t place, const char* what, HRESULT hr) {
    const HRESULT reason = device ? device->GetDeviceRemovedReason() : S_OK;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || FAILED(reason)) {
        if (!lost)
            LOG_ERROR("%s: the GPU device was removed (0x%08lx, reason 0x%08lx); the picture stops here, restart the game",
                      what, static_cast<unsigned long>(hr), static_cast<unsigned long>(reason));
        lost = true;
        return;
    }
    if (failures.first(place, uint32_t(hr))) LOG_ERROR("%s failed: 0x%08lx", what, static_cast<unsigned long>(hr));
}

bool Renderer::Impl::growUpload(Slot& s, UINT64 size) {
    if (s.upload) s.garbage.push_back(s.upload);  // still used by what this slot recorded so far
    s.upload = createBuffer(device, D3D12_HEAP_TYPE_UPLOAD, size, D3D12_RESOURCE_STATE_GENERIC_READ);
    s.mapped = nullptr;
    s.size = s.upload ? size : 0;
    s.used = 0;
    if (!s.upload) return false;
    D3D12_RANGE none{0, 0};
    return SUCCEEDED(s.upload->Map(0, &none, reinterpret_cast<void**>(&s.mapped)));
}

Upload Renderer::Impl::allocUpload(UINT64 size, UINT64 alignment) {
    Slot& s = slots[slot];
    UINT64 at = alignUp(s.used, alignment);
    if (at + size > s.size) {
        const UINT64 want = std::max(s.size * 2, alignUp(size, 1 << 20) + (1 << 20));
        if (!growUpload(s, want)) {  // out of address space: skip this upload or draw rather than crash
            static bool logged = false;
            if (!logged)
                LOG_ERROR("cannot map a %llu MB upload buffer: some textures and draws are skipped",
                          static_cast<unsigned long long>(want >> 20));
            logged = true;
            return Upload{nullptr, 0, 0};
        }
        at = 0;
    }
    s.used = at + size;
    return Upload{s.mapped + at, s.upload->GetGPUVirtualAddress() + at, at};
}

void Renderer::Impl::transition(ID3D12Resource* r, D3D12_RESOURCE_STATES& state, D3D12_RESOURCE_STATES to) {
    if (state == to) return;
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = state;
    b.Transition.StateAfter = to;
    list->ResourceBarrier(1, &b);
    state = to;
}

UINT Renderer::Impl::allocSrv() {
    if (!freeSrv.empty()) {
        const UINT i = freeSrv.back();
        freeSrv.pop_back();
        return i;
    }
    return nextSrv < kCpuSrvCount ? nextSrv++ : kNoSrv;
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::Impl::cpuSrvHandle(UINT i) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = cpuSrv->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(i) * srvStep;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::Impl::rtvHandle(UINT i) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(i) * rtvStep;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::Impl::dsvHandle() const { return dsvHeap->GetCPUDescriptorHandleForHeapStart(); }

void Renderer::Impl::bindTarget() {
    transition(target, targetState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (targetBound) return;
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(0);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHandle();
    list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &scissor);
    targetBound = true;
}

// ---- Impl: frame record commands ---------------------------------------------------------------------------

void Renderer::Impl::clear(const ClearCmd& c) {
    const Rect t = scaleRect(c.rect, scale);  // game pixels -> target pixels
    D3D12_RECT r{t.left, t.top, t.right, t.bottom};
    r.left = std::max<LONG>(r.left, 0);
    r.top = std::max<LONG>(r.top, 0);
    r.right = std::min<LONG>(r.right, width);
    r.bottom = std::min<LONG>(r.bottom, height);
    if (r.right <= r.left || r.bottom <= r.top) return;
    bindTarget();
    if (c.flags & 1) {  // D3DCLEAR_TARGET
        const float color[4] = {float((c.argb >> 16) & 255) / 255.0f, float((c.argb >> 8) & 255) / 255.0f,
                                float(c.argb & 255) / 255.0f, 1.0f};
        list->ClearRenderTargetView(rtvHandle(0), color, 1, &r);
    }
    if (c.flags & 2) list->ClearDepthStencilView(dsvHandle(), D3D12_CLEAR_FLAG_DEPTH, c.z, 0, 1, &r);  // ZBUFFER
}

void Renderer::Impl::setViewport(const ViewportCmd& v) {
    gameViewport = v;
    applyViewport();
}

// Pre-transformed vertices carry their screen position: the D3D7 viewport only clips them and gives the depth
// range. So the D3D12 viewport is the whole target and the client's viewport becomes the scissor rectangle.
void Renderer::Impl::applyViewport() {
    viewport = D3D12_VIEWPORT{0.0f, 0.0f, float(width), float(height), gameViewport.minZ, gameViewport.maxZ};
    const Rect r = scaleRect(gameViewport.rect, scale);
    const LONG left = std::max(r.left, 0), top = std::max(r.top, 0);
    scissor = D3D12_RECT{left, top, std::max<LONG>(left, std::min(r.right, width)),
                         std::max<LONG>(top, std::min(r.bottom, height))};
    if (open && targetBound) {  // setTarget and setScale call this between command lists
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
    }
}

namespace {

// Copies width x height ARGB values into a placed footprint (rows aligned to 256 bytes) in the upload buffer.
D3D12_TEXTURE_COPY_LOCATION stage(Renderer::Impl& m, const uint32_t* argb, UINT w, UINT h) {
    const UINT pitch = UINT(alignUp(UINT64(w) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
    const Upload u = m.allocUpload(UINT64(pitch) * h, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    D3D12_TEXTURE_COPY_LOCATION src{};
    if (!u.cpu) return src;  // pResource stays null: nothing to copy
    for (UINT y = 0; y < h; ++y) std::memcpy(u.cpu + UINT64(y) * pitch, argb + UINT64(y) * w, size_t(w) * 4);
    src.pResource = m.slots[m.slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = u.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    src.PlacedFootprint.Footprint.Width = w;
    src.PlacedFootprint.Footprint.Height = h;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = pitch;
    return src;
}

}  // namespace

void Renderer::Impl::upload(const FrameRecord& rec, const UploadCmd& u) {
    auto it = textures.find(u.texture);
    if (it == textures.end() || u.rect.empty()) return;
    Texture& t = it->second;
    const D3D12_TEXTURE_COPY_LOCATION src = stage(*this, rec.pixels().data() + u.pixels, UINT(u.rect.width()),
                                                  UINT(u.rect.height()));
    if (!src.pResource) return;
    transition(t.resource, t.state, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = t.resource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = u.level;
    list->CopyTextureRegion(&dst, UINT(u.rect.left), UINT(u.rect.top), 0, &src, nullptr);
}

void Renderer::Impl::writeTarget(const FrameRecord& rec, const WriteTargetCmd& w) {
    if (w.rect.empty()) return;
    const D3D12_TEXTURE_COPY_LOCATION src = stage(*this, rec.pixels().data() + w.pixels, UINT(w.rect.width()),
                                                  UINT(w.rect.height()));
    if (!src.pResource) return;
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    if (!scaled()) {  // the target has the game's size: straight in
        transition(target, targetState, D3D12_RESOURCE_STATE_COPY_DEST);
        dst.pResource = target;
        list->CopyTextureRegion(&dst, UINT(w.rect.left), UINT(w.rect.top), 0, &src, nullptr);
        return;
    }
    // A larger target (stage 3 spec §5): the client's pixels go into the game-sized helper and are stretched in.
    transition(gameSized, gameState, D3D12_RESOURCE_STATE_COPY_DEST);
    dst.pResource = gameSized;
    list->CopyTextureRegion(&dst, UINT(w.rect.left), UINT(w.rect.top), 0, &src, nullptr);
    transition(target, targetState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float from[4] = {float(w.rect.left), float(w.rect.top), float(w.rect.right), float(w.rect.bottom)};
    blit(gameSized, gameState, gameSrv, gameWidth, gameHeight, from, rtvHandle(kRtvTarget), scaleRect(w.rect, scale),
         width, height);
}

// Draws srcRect (in the source's pixels) of a texture into dstRect of a render target with the present pipeline:
// one bilinear sample per pixel when enlarging, an average over each pixel's footprint when shrinking.
void Renderer::Impl::blit(ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState, UINT srcSrv, int srcW, int srcH,
                          const float srcRect[4], D3D12_CPU_DESCRIPTOR_HANDLE dst, const Rect& dstRect, int dstW,
                          int dstH, float sharpen) {
    if (dstRect.empty() || srcW <= 0 || srcH <= 0) return;
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    if (!makeRoom(linearClamp, linearClamp)) return;  // first: a new list starts before this copy records anything
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp);
    const D3D12_GPU_DESCRIPTOR_HANDLE textures = textureTable(srcSrv, srcSrv);
    transition(src, srcState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const D3D12_VIEWPORT vp{float(dstRect.left), float(dstRect.top), float(dstRect.width()), float(dstRect.height()),
                            0.0f, 1.0f};
    const D3D12_RECT clip{std::max<LONG>(dstRect.left, 0), std::max<LONG>(dstRect.top, 0),
                          std::min<LONG>(dstRect.right, dstW), std::min<LONG>(dstRect.bottom, dstH)};
    list->OMSetRenderTargets(1, &dst, FALSE, nullptr);
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &clip);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(presentPipeline);
    Constants c{};
    c.sourceRect[0] = srcRect[0] / float(srcW);
    c.sourceRect[1] = srcRect[1] / float(srcH);
    c.sourceRect[2] = srcRect[2] / float(srcW);
    c.sourceRect[3] = srcRect[3] / float(srcH);
    // No tap from beyond the source rectangle, stale or not, reaches the copy's edges (stage 3 review M6).
    c.sourceClamp[0] = (srcRect[0] + 0.5f) / float(srcW);
    c.sourceClamp[1] = (srcRect[1] + 0.5f) / float(srcH);
    c.sourceClamp[2] = std::max(srcRect[0] + 0.5f, srcRect[2] - 0.5f) / float(srcW);
    c.sourceClamp[3] = std::max(srcRect[1] + 0.5f, srcRect[3] - 0.5f) / float(srcH);
    c.footprint = std::max((srcRect[2] - srcRect[0]) / float(dstRect.width()),
                           (srcRect[3] - srcRect[1]) / float(dstRect.height()));
    c.sharpen = sharpen;  // only the picture on screen: reads and writes of the frame stay as the game drew it
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootTextures, textures);
    list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    targetBound = false;
}

bool Renderer::Impl::createTarget(int w, int h, ID3D12Resource*& rt, ID3D12Resource*& ds) {
    D3D12_CLEAR_VALUE black{};
    black.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    black.Color[3] = 1.0f;
    rt = createTexture2D(device, w, h, 1, DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                         D3D12_RESOURCE_STATE_RENDER_TARGET, &black);
    D3D12_CLEAR_VALUE farthest{};  // not "far": windef.h defines far as an empty macro
    farthest.Format = DXGI_FORMAT_D32_FLOAT;
    farthest.DepthStencil.Depth = 1.0f;
    ds = createTexture2D(device, w, h, 1, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                         D3D12_RESOURCE_STATE_DEPTH_WRITE, &farthest);
    if (rt && ds) return true;
    releaseCom(rt);
    releaseCom(ds);
    LOG_ERROR("cannot create a %dx%d render target", w, h);
    return false;
}

// ---- Impl: swap chain --------------------------------------------------------------------------------------

void Renderer::Impl::releaseSwapChain() {
    for (ID3D12Resource*& b : backBuffers) releaseCom(b);
    releaseCom(swapChain);
    window = nullptr;
    swapWidth = swapHeight = 0;
}

bool Renderer::Impl::ensureSwapChain(HWND hwnd, UINT w, UINT h, bool wantTearing) {
    if (swapChain && hwnd == window && w == swapWidth && h == swapHeight) return true;
    if (!swapChain && w == failedWidth && h == failedHeight && !swapRetry.mayTry(GetTickCount64())) return false;
    waitIdle();
    const UINT flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    if (swapChain && hwnd == window) {  // the window changed size
        for (ID3D12Resource*& b : backBuffers) releaseCom(b);
        const HRESULT hr = swapChain->ResizeBuffers(2, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, flags);
        if (FAILED(hr)) {
            failed(3, "swap chain resize", hr);
            releaseSwapChain();
            failedWidth = w;
            failedHeight = h;
            swapRetry.failed(GetTickCount64());
            return false;
        }
    } else {
        releaseSwapChain();
        IDXGIFactory5* f5 = nullptr;
        BOOL allow = FALSE;
        if (wantTearing && SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory5), reinterpret_cast<void**>(&f5)))) {
            if (FAILED(f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof allow))) allow = FALSE;
            f5->Release();
        }
        tearingSupported = allow == TRUE;
        DXGI_SWAP_CHAIN_DESC1 d{};
        d.Width = w;
        d.Height = h;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d.BufferCount = 2;
        d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        d.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        d.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        IDXGISwapChain1* sc1 = nullptr;
        HRESULT hr = factory->CreateSwapChainForHwnd(queue, hwnd, &d, nullptr, nullptr, &sc1);
        if (SUCCEEDED(hr)) hr = sc1->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain));
        if (FAILED(hr)) {
            failed(4, "swap chain creation", hr);
            releaseCom(sc1);
            failedWidth = w;
            failedHeight = h;
            swapRetry.failed(GetTickCount64());
            return false;
        }
        sc1->Release();
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        window = hwnd;
        LOG_INFO("swap chain %ux%u%s", w, h, tearingSupported ? ", tearing allowed" : "");
    }
    for (UINT i = 0; i < 2; ++i) {
        if (FAILED(swapChain->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&backBuffers[i])))) {
            releaseSwapChain();
            return false;
        }
        device->CreateRenderTargetView(backBuffers[i], nullptr, rtvHandle(1 + i));
        backStates[i] = D3D12_RESOURCE_STATE_PRESENT;
    }
    swapWidth = w;
    swapHeight = h;
    failedWidth = failedHeight = 0;
    swapRetry.succeeded();
    return true;
}

void Renderer::Impl::destroy() {
    if (device && fence) waitIdle();
    releaseSwapChain();
    for (auto& t : textures) releaseCom(t.second.resource);
    textures.clear();
    for (auto& p : pipelines) releaseCom(p.second);
    pipelines.clear();
    releaseCom(presentPipeline);
    releaseCom(root);
    releaseCom(drawVs);
    releaseCom(drawPs);
    releaseCom(presentVs);
    releaseCom(presentPs);
    releaseCom(readback);
    releaseCom(target);
    releaseCom(depth);
    releaseCom(gameSized);
    for (Slot& s : slots) {
        for (IUnknown* g : s.garbage) g->Release();
        s.garbage.clear();
        releaseCom(s.upload);
        releaseCom(s.allocator);
    }
    releaseCom(list);
    releaseCom(cpuSrv);
    releaseCom(gpuSrv);
    releaseCom(samplerHeap);
    releaseCom(rtvHeap);
    releaseCom(dsvHeap);
    releaseCom(fence);
    if (fenceEvent) CloseHandle(fenceEvent);
    fenceEvent = nullptr;
    releaseCom(queue);
    releaseCom(device);
    releaseCom(factory);
}

// ---- Renderer -------------------------------------------------------------------------------------------------

Renderer::Renderer() : impl_(new Impl) {}

Renderer::~Renderer() {
    impl_->destroy();
    delete impl_;
}

std::string Renderer::adapterName() const { return impl_->adapter; }

void Renderer::setUiSharp(bool sharp) { impl_->uiSharp = sharp; }

void Renderer::preparePipelines(const std::string& file) {
    Impl& m = *impl_;
    std::string text;
    win::readTextFile(file, text);
    const std::vector<uint64_t> keys = parsePipelineKeys(text);
    // Written anew: a list from another version or with broken lines starts clean, keys listed twice once.
    const std::set<uint64_t> unique(keys.begin(), keys.end());
    std::string clean = pipelineKeysHeader();
    for (const uint64_t k : unique) clean += pipelineKeyLine(k);
    std::ofstream(file, std::ios::binary | std::ios::trunc) << clean;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    unsigned ready = 0;
    for (const uint64_t k : unique)
        if (m.pipelineForKey(k)) ++ready;
    QueryPerformanceCounter(&t1);
    m.pipelineFile = file;  // from now on every new pipeline is added
    LOG_INFO("pipelines: %u of %u saved ones prepared in %.0f ms", ready, unsigned(unique.size()),
             double(t1.QuadPart - t0.QuadPart) * 1000.0 / double(freq.QuadPart));
}

bool Renderer::create(std::string& error) {
    Impl& m = *impl_;
    m.d3d12 = LoadLibraryA("d3d12.dll");
    m.dxgi = LoadLibraryA("dxgi.dll");
    if (!m.d3d12 || !m.dxgi) {
        error = "d3d12.dll or dxgi.dll is missing";
        return false;
    }
    using CreateFactory2Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);
    const auto createFactory = reinterpret_cast<CreateFactory2Fn>(
        reinterpret_cast<void*>(GetProcAddress(m.dxgi, "CreateDXGIFactory2")));
    const auto createDevice = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
        reinterpret_cast<void*>(GetProcAddress(m.d3d12, "D3D12CreateDevice")));
    if (!createFactory || !createDevice ||
        FAILED(createFactory(0, __uuidof(IDXGIFactory4), reinterpret_cast<void**>(&m.factory)))) {
        error = "no DXGI 1.4 factory";
        return false;
    }
    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; !m.device && m.factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 d{};
        adapter->GetDesc1(&d);
        if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&  // WARP / Basic Render: not worth it
            SUCCEEDED(createDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                   reinterpret_cast<void**>(&m.device))))
            m.adapter = narrow(d.Description);
        adapter->Release();
    }
    if (!m.device) {
        error = "no hardware adapter with Direct3D 12";
        return false;
    }
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(m.device->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&m.queue)))) {
        error = "command queue";
        return false;
    }
    for (Slot& s : m.slots) {
        if (FAILED(m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                    reinterpret_cast<void**>(&s.allocator))) ||
            !m.growUpload(s, kUploadSize)) {
            error = "command allocators / upload buffers";
            return false;
        }
    }
    if (FAILED(m.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m.slots[0].allocator, nullptr,
                                           __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&m.list)))) {
        error = "command list";
        return false;
    }
    m.list->Close();
    if (FAILED(m.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&m.fence))) ||
        !(m.fenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr))) {
        error = "fence";
        return false;
    }
    auto heap = [&](D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool visible, ID3D12DescriptorHeap*& out) {
        D3D12_DESCRIPTOR_HEAP_DESC d{};
        d.Type = type;
        d.NumDescriptors = count;
        d.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        return SUCCEEDED(m.device->CreateDescriptorHeap(&d, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&out)));
    };
    if (!heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kCpuSrvCount, false, m.cpuSrv) ||
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kGpuSrvCount, true, m.gpuSrv) ||
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, kSamplerCount, true, m.samplerHeap) ||
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kRtvCount, false, m.rtvHeap) ||
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false, m.dsvHeap)) {
        error = "descriptor heaps";
        return false;
    }
    m.srvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m.samplerStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    m.rtvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m.targetSrv = m.allocSrv();
    m.gameSrv = m.allocSrv();
    if (!m.createPipelines(error)) return false;

    // The texture bound wherever the client binds none: D3D7 reads it as white (states.md §5).
    m.white = createTexture(1, 1, 1);
    const uint32_t whitePixel = 0xFFFFFFFF;
    FrameRecord rec;
    rec.upload(m.white, 0, Rect{0, 0, 1, 1}, &whitePixel);
    if (!m.white || !m.begin()) {
        error = "white texture";
        return false;
    }
    m.upload(rec, rec.uploads()[0]);
    m.waitIdle();
    return true;
}

bool Renderer::setTarget(int width, int height) {
    Impl& m = *impl_;
    if (width <= 0 || height <= 0) return false;
    if (width == m.gameWidth && height == m.gameHeight && m.target) return true;
    m.waitIdle();
    // On a failure nothing half made is left (stage 3 review M3): no target, sizes zero, so the next call starts over
    // and no view describes a released resource.
    const auto fail = [&m]() {
        releaseCom(m.target);
        releaseCom(m.depth);
        releaseCom(m.gameSized);
        m.width = m.height = m.gameWidth = m.gameHeight = 0;
        return false;
    };
    releaseCom(m.target);
    releaseCom(m.depth);
    releaseCom(m.gameSized);
    if (!m.createTarget(width, height, m.target, m.depth)) return fail();
    m.gameSized = createTexture2D(m.device, width, height, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
    if (!m.gameSized) {
        LOG_ERROR("cannot create the %dx%d helper of the game's size", width, height);
        return fail();
    }
    m.targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m.gameState = D3D12_RESOURCE_STATE_COPY_DEST;
    m.device->CreateRenderTargetView(m.target, nullptr, m.rtvHandle(kRtvTarget));
    m.device->CreateDepthStencilView(m.depth, nullptr, m.dsvHandle());
    m.device->CreateShaderResourceView(m.target, nullptr, m.cpuSrvHandle(m.targetSrv));
    m.device->CreateRenderTargetView(m.gameSized, nullptr, m.rtvHandle(kRtvGame));
    m.device->CreateShaderResourceView(m.gameSized, nullptr, m.cpuSrvHandle(m.gameSrv));
    m.width = m.gameWidth = width;
    m.height = m.gameHeight = height;
    m.scale = 1.0;  // until the first present (stage 3 spec §3): the start-up calibration sees DX7's picture
    m.gameViewport = ViewportCmd{Rect{0, 0, width, height}, 0.0f, 1.0f};
    m.applyViewport();
    if (!m.begin()) return false;
    const float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m.list->ClearRenderTargetView(m.rtvHandle(kRtvTarget), color, 0, nullptr);
    m.list->ClearDepthStencilView(m.dsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m.submit();
    LOG_INFO("render target %dx%d", width, height);
    return true;
}

bool Renderer::setScale(double k, int targetWidth, int targetHeight) {
    Impl& m = *impl_;
    if (m.lost || !m.target || targetWidth <= 0 || targetHeight <= 0) return false;
    if (k == m.scale && targetWidth == m.width && targetHeight == m.height) return true;
    ID3D12Resource* target = nullptr;
    ID3D12Resource* depth = nullptr;
    if (!m.createTarget(targetWidth, targetHeight, target, depth)) return false;
    // The back buffer keeps its picture across a present: the old target is stretched into the new one.
    if (!m.begin()) {
        releaseCom(target);
        releaseCom(depth);
        return false;
    }
    m.device->CreateRenderTargetView(target, nullptr, m.rtvHandle(kRtvNew));
    const float all[4] = {0.0f, 0.0f, float(m.width), float(m.height)};
    m.blit(m.target, m.targetState, m.targetSrv, m.width, m.height, all, m.rtvHandle(kRtvNew),
           Rect{0, 0, targetWidth, targetHeight}, targetWidth, targetHeight);
    m.waitIdle();
    releaseCom(m.target);
    releaseCom(m.depth);
    m.target = target;
    m.depth = depth;
    m.targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m.device->CreateRenderTargetView(m.target, nullptr, m.rtvHandle(kRtvTarget));
    m.device->CreateDepthStencilView(m.depth, nullptr, m.dsvHandle());
    m.device->CreateShaderResourceView(m.target, nullptr, m.cpuSrvHandle(m.targetSrv));
    m.width = targetWidth;
    m.height = targetHeight;
    m.scale = k;
    m.applyViewport();
    if (!m.begin()) return false;
    m.list->ClearDepthStencilView(m.dsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m.submit();
    LOG_INFO("render target %dx%d: %.3f times the game's %dx%d", targetWidth, targetHeight, k, m.gameWidth,
             m.gameHeight);
    return true;
}

uint32_t Renderer::createTexture(int width, int height, int levels) {
    Impl& m = *impl_;
    Texture t;
    t.width = width;
    t.height = height;
    t.levels = std::max(1, levels);
    t.srv = m.allocSrv();
    if (t.srv == kNoSrv) {  // index 0 is the target's: never hand out a slot that is in use
        LOG_ERROR("no texture descriptor left for a %dx%d texture (%u textures alive)", width, height,
                  static_cast<unsigned>(m.textures.size()));
        return 0;
    }
    t.resource = createTexture2D(m.device, width, height, t.levels, DXGI_FORMAT_B8G8R8A8_UNORM,
                                 D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
    if (!t.resource) {
        m.freeSrv.push_back(t.srv);
        return 0;
    }
    m.device->CreateShaderResourceView(t.resource, nullptr, m.cpuSrvHandle(t.srv));
    const uint32_t id = m.nextTexture++;
    m.textures.emplace(id, t);
    return id;
}

void Renderer::releaseTexture(uint32_t id) {
    Impl& m = *impl_;
    // Draws recorded before the client let go of the texture have not been played yet: it stays until they have.
    if (m.textures.count(id)) m.dying.push_back(id);
}

void Renderer::Impl::retireDying() {
    // The record that could still draw with them has just been submitted. Its slot's begin() frees them once that
    // submission's fence has passed, and with it every earlier submission on the queue.
    Slot& s = slots[(slot + kSlots - 1) % kSlots];
    for (uint32_t id : dying) {
        auto it = textures.find(id);
        if (it == textures.end()) continue;
        s.garbage.push_back(it->second.resource);
        freeSrv.push_back(it->second.srv);
        textures.erase(it);
    }
    dying.clear();
}

void Renderer::execute(const FrameRecord& record) {
    Impl& m = *impl_;
    if (m.lost || !m.target || record.empty() || !m.begin()) return;
    m.prepareGeometry(record);
    for (const Command& c : record.commands()) {
        switch (c.type) {
            case Cmd::Clear: m.clear(record.clears()[c.index]); break;
            case Cmd::Viewport: m.setViewport(record.viewports()[c.index]); break;
            case Cmd::Upload: m.upload(record, record.uploads()[c.index]); break;
            case Cmd::WriteTarget: m.writeTarget(record, record.writes()[c.index]); break;
            case Cmd::Draw: m.draw(record, record.draws()[c.index]); break;
        }
    }
    const UINT64 before = m.fenceValue;
    m.submit();
    if (m.fenceValue != before) m.retireDying();  // only once the record really went to the GPU
}

bool Renderer::readTarget(std::vector<uint32_t>& argb) {
    Impl& m = *impl_;
    if (m.lost || !m.target || !m.begin()) return false;
    ID3D12Resource* source = m.target;
    D3D12_RESOURCE_STATES* sourceState = &m.targetState;
    if (m.scaled()) {  // the client reads its own size: shrink the target into the helper first (stage 3 spec §5)
        m.transition(m.gameSized, m.gameState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float all[4] = {0.0f, 0.0f, float(m.width), float(m.height)};
        m.blit(m.target, m.targetState, m.targetSrv, m.width, m.height, all, m.rtvHandle(kRtvGame),
               Rect{0, 0, m.gameWidth, m.gameHeight}, m.gameWidth, m.gameHeight);
        source = m.gameSized;
        sourceState = &m.gameState;
    }
    const int w = m.gameWidth;
    const int h = m.gameHeight;
    const UINT pitch = UINT(alignUp(UINT64(w) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
    const UINT64 size = UINT64(pitch) * UINT64(h);
    if (m.readbackSize < size) {
        releaseCom(m.readback);
        m.readback = createBuffer(m.device, D3D12_HEAP_TYPE_READBACK, size, D3D12_RESOURCE_STATE_COPY_DEST);
        m.readbackSize = m.readback ? size : 0;
        if (!m.readback) return false;
    }
    m.transition(source, *sourceState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = m.readback;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Offset = 0;
    dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    dst.PlacedFootprint.Footprint.Width = UINT(w);
    dst.PlacedFootprint.Footprint.Height = UINT(h);
    dst.PlacedFootprint.Footprint.Depth = 1;
    dst.PlacedFootprint.Footprint.RowPitch = pitch;
    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = source;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;
    m.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    m.waitIdle();
    uint8_t* data = nullptr;
    D3D12_RANGE all{0, SIZE_T(size)};
    const HRESULT hr = m.readback->Map(0, &all, reinterpret_cast<void**>(&data));
    if (FAILED(hr)) {
        m.failed(6, "reading the frame back", hr);
        return false;
    }
    argb.resize(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        std::memcpy(argb.data() + size_t(y) * size_t(w), data + UINT64(y) * pitch, size_t(w) * 4);
    D3D12_RANGE none{0, 0};
    m.readback->Unmap(0, &none);
    return true;
}

void Renderer::present(HWND window, int outputWidth, int outputHeight, const Rect& source,
                       const PresentOptions& options) {
    Impl& m = *impl_;
    if (m.lost) {  // the GPU is gone (stage 3 review M2): no Present waits for vblank, keep the game's loop from spinning
        Sleep(15);
        return;
    }
    if (!m.target || !window || source.empty()) return;
    if (!m.ensureSwapChain(window, UINT(std::max(1, outputWidth)), UINT(std::max(1, outputHeight)),
                           options.allowTearing) ||
        !m.begin())
        return;
    const UINT index = m.swapChain->GetCurrentBackBufferIndex();
    m.transition(m.backBuffers[index], m.backStates[index], D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m.rtvHandle(kRtvSwap + index);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m.list->ClearRenderTargetView(rtv, black, 0, nullptr);
    // Fit the source into the window, keeping its shape, on whole pixels: bars where the shapes differ.
    const Rect fit = fitPicture(source.width(), source.height(), int(m.swapWidth), int(m.swapHeight));
    if (!(fit == m.picture)) {
        m.picture = fit;
        LOG_INFO("picture %dx%d of the game shown as %dx%d at %d,%d in the %ux%u window", source.width(),
                 source.height(), fit.width(), fit.height(), fit.left, fit.top, m.swapWidth, m.swapHeight);
    }
    // The source is in game pixels: into target pixels by each axis' own ratio, so the whole frame is exactly the
    // whole target and at render_scale 100, where the target is the picture, it is copied pixel for pixel.
    const auto tx = [&m](int x) { return float(double(x) * m.width / m.gameWidth); };
    const auto ty = [&m](int y) { return float(double(y) * m.height / m.gameHeight); };
    const float from[4] = {tx(source.left), ty(source.top), tx(source.right), ty(source.bottom)};
    m.blit(m.target, m.targetState, m.targetSrv, m.width, m.height, from, rtv, fit, int(m.swapWidth),
           int(m.swapHeight), options.sharpen);
    m.transition(m.backBuffers[index], m.backStates[index], D3D12_RESOURCE_STATE_PRESENT);
    m.submit();

    if (options.fpsLimit > 0) {  // simple pacing: wait until 1/limit seconds after the previous present
        LARGE_INTEGER now, freq;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&now);
        const LONGLONG period = freq.QuadPart / options.fpsLimit;
        if (m.lastPresent.QuadPart) {
            const LONGLONG wait = m.lastPresent.QuadPart + period - now.QuadPart;
            if (wait > 0) Sleep(DWORD(wait * 1000 / freq.QuadPart));
        }
        QueryPerformanceCounter(&m.lastPresent);
    }
    const UINT sync = options.vsync ? 1 : 0;
    const UINT flags = !options.vsync && options.allowTearing && m.tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
    const HRESULT hr = m.swapChain->Present(sync, flags);
    if (FAILED(hr))
        m.failed(5, "Present", hr);
    else if (hr == DXGI_STATUS_OCCLUDED)
        Sleep(15);  // the window is covered: Present does not wait for vblank, keep the game's loop from spinning
}

}  // namespace uagfx::gpu
