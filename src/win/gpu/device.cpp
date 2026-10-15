#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>

#include "core/d3d7.hpp"
#include "core/effect_light.hpp"
#include "core/game_camera.hpp"
#include "core/pipeline_key.hpp"
#include "core/scale.hpp"
#include "core/window_plan.hpp"
#include "win/gpu/ai_textures.hpp"
#include "win/gpu/impl.hpp"
#include "win/gpu/remix9.hpp"
#include "win/gpu/remote.hpp"
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

}  // namespace

ID3D12Resource* createTexture2D(ID3D12Device* device, int w, int h, int levels, DXGI_FORMAT format,
                                D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear,
                                UINT samples) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = UINT64(w);
    d.Height = UINT(h);
    d.DepthOrArraySize = 1;
    d.MipLevels = UINT16(levels);
    d.Format = format;
    d.SampleDesc.Count = samples;
    d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d.Flags = flags;
    ID3D12Resource* r = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, state, clear,
                                               __uuidof(ID3D12Resource), reinterpret_cast<void**>(&r))))
        return nullptr;
    return r;
}

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
    pass = kPassNone;
    centered = false;  // a new list starts with the default sample positions
    if (timer) {
        readTimes(s, slot);
        list->EndQuery(timer, D3D12_QUERY_TYPE_TIMESTAMP, slot * kTimes);
    }
    open = true;
    ++listSerial;
    return true;
}

void Renderer::Impl::submit() {
    if (!open) return;
    open = false;
    if (timer) {
        Slot& s = slots[slot];
        list->EndQuery(timer, D3D12_QUERY_TYPE_TIMESTAMP, slot * kTimes + 1);
        // Only what was written: the list's own pair, then the upscale's and the AI textures' when they ran.
        const auto resolve = [&](UINT first) {
            list->ResolveQueryData(timer, D3D12_QUERY_TYPE_TIMESTAMP, slot * kTimes + first, 2, timerBuffer,
                                   UINT64(slot * kTimes + first) * sizeof(UINT64));
        };
        resolve(0);
        if (s.upscaleTimed) resolve(2);
        if (s.aiTimed) resolve(4);
        for (UINT e = 0; e < kEffectTimes; ++e)
            if (s.effectTimed >> e & 1) resolve(6 + 2 * e);
        s.timed = true;
    }
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

void Renderer::Impl::readTimes(Slot& s, UINT index) {
    if (s.timed && timerData) {
        const UINT64* t = timerData + index * kTimes;
        if (t[1] > t[0]) gpuTicks += t[1] - t[0];
        if (s.upscaleTimed && t[3] > t[2]) upscaleTicks += t[3] - t[2];
        if (s.aiTimed && t[5] >= t[4]) {
            aiTicks += t[5] - t[4];
            if (ai) ai->observe(index, double(t[5] - t[4]) * 1000.0 / double(timerFrequency));
        }
        for (UINT e = 0; e < kEffectTimes; ++e)
            if ((s.effectTimed >> e & 1) && t[7 + 2 * e] > t[6 + 2 * e]) effectTicks[e] += t[7 + 2 * e] - t[6 + 2 * e];
    }
    s.timed = s.upscaleTimed = s.aiTimed = false;
    s.effectTimed = 0;
}

void Renderer::Impl::effectBegin(EffectTimer e) {
    if (!timer || !open) return;
    list->EndQuery(timer, D3D12_QUERY_TYPE_TIMESTAMP, slot * kTimes + 6 + 2 * e);
    effectSerial[e] = listSerial;
}

void Renderer::Impl::effectEnd(EffectTimer e) {
    if (!timer || !open || effectSerial[e] != listSerial) return;
    list->EndQuery(timer, D3D12_QUERY_TYPE_TIMESTAMP, slot * kTimes + 7 + 2 * e);
    slots[slot].effectTimed |= 1u << e;
}

void Renderer::Impl::noteGpuFrame() {
    if (!timer || !timerFrequency) return;
    ++gpuFrames;
    const ULONGLONG now = GetTickCount64();
    if (!gpuSince) gpuSince = now;
    if (now - gpuSince < 30000) return;
    const double ms = 1000.0 / double(timerFrequency) / double(gpuFrames);
    if (aiTicks)
        LOG_INFO("GPU: %.2f ms of work a frame, %.2f ms of it AI textures (%u frames in %.0f s)",
                 double(gpuTicks) * ms, double(aiTicks) * ms, gpuFrames, double(now - gpuSince) / 1000.0);
    if (upscaleTicks)
        LOG_INFO("GPU: %.2f ms of work a frame, %.2f ms of it %s (%u frames in %.0f s)", double(gpuTicks) * ms,
                 double(upscaleTicks) * ms, upscaler ? upscaler->name() : "upscaling", gpuFrames,
                 double(now - gpuSince) / 1000.0);
    else
        LOG_INFO("GPU: %.2f ms of work a frame (%u frames in %.0f s)", double(gpuTicks) * ms, gpuFrames,
                 double(now - gpuSince) / 1000.0);
    // 0.39.2: the effects one by one - what each of them costs a frame.
    static const char* const kEffectNames[kEffectTimes] = {
        "soft shadows", "clouds", "characters' shadows", "lights and torches", "haze", "heat haze", "colour",
        "depth of field", "weather", "glow", "water's copies"};
    std::string effects;
    for (UINT e = 0; e < kEffectTimes; ++e) {
        if (!effectTicks[e]) continue;
        char part[64];
        std::snprintf(part, sizeof part, "%s%s %.2f ms", effects.empty() ? "" : ", ", kEffectNames[e],
                      double(effectTicks[e]) * ms);
        effects += part;
        effectTicks[e] = 0;
    }
    if (!effects.empty()) LOG_INFO("GPU: effects a frame - %s", effects.c_str());
    gpuTicks = upscaleTicks = aiTicks = 0;
    gpuFrames = 0;
    gpuSince = now;
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

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::Impl::dsvHandle(UINT i) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(i) * dsvStep;
    return h;
}

// The target (with its depth), or under DLSS (part 3) the world (with its depth) or the interface layer; the glow's
// passes (0.18) with the glow target as the second.
void Renderer::Impl::bindPass(int p) {
    const bool withGlow = p == kPassTargetGlow || p == kPassWorldGlow;
    const bool toWorld = p == kPassWorld || p == kPassWorldGlow;
    if (toWorld) {
        transition(world, worldState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        transition(worldDepth, worldDepthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    } else if (p == kPassLayer) {
        transition(layer, layerState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    } else {
        transition(target, targetState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        transition(depth, depthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);  // 0.19: read by the water's copy
    }
    if (withGlow) transition(glow, glowState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (pass == p) return;
    if (toWorld) {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv[2] = {rtvHandle(kRtvWorld), rtvHandle(kRtvGlow)};
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHandle(kDsvWorld);
        list->OMSetRenderTargets(withGlow ? 2 : 1, rtv, FALSE, &dsv);
        list->RSSetViewports(1, &worldViewport);
        list->RSSetScissorRects(1, &worldScissor);
    } else if (p == kPassLayer) {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(kRtvLayer);
        list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
        const float one[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // the opaque interface's alpha (pipelineForKey)
        list->OMSetBlendFactor(one);
    } else {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv[2] = {rtvHandle(kRtvTarget), rtvHandle(kRtvGlow)};
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHandle();
        list->OMSetRenderTargets(withGlow ? 2 : 1, rtv, FALSE, &dsv);
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
    }
    pass = p;
}

void Renderer::Impl::placeSamples(bool center) {
    if (!list1 || center == centered) return;
    if (center) {
        D3D12_SAMPLE_POSITION at[8] = {};  // all eight at (0, 0): the pixel's centre
        list1->SetSamplePositions(samples, 1, at);
    } else {
        list1->SetSamplePositions(0, 0, nullptr);
    }
    centered = center;
}

// ---- Impl: frame record commands ---------------------------------------------------------------------------

void Renderer::Impl::clear(const ClearCmd& c) {
    if (c.flags & 2) sceneDepthFresh = false;  // 0.39.2: the depth cleared - its copy out of date
    // 0.18: the picture cleared - the light of a world no longer there is not added to the next one.
    if ((c.flags & 1) && c.rect.left <= 0 && c.rect.top <= 0 && c.rect.right >= gameWidth &&
        c.rect.bottom >= gameHeight) {
        dropGlow();
        newPicture();  // 0.19-0.23: its water, shadows, torches, colours, depth of field anew
    }
    if (upscale) {  // part 3: the world and its depth; a colour clear empties the interface layer too
        startFrame();
        const Rect w = scaleRect(c.rect, worldScale);
        D3D12_RECT wr{std::max<LONG>(w.left, 0), std::max<LONG>(w.top, 0), std::min<LONG>(w.right, renderWidth),
                      std::min<LONG>(w.bottom, renderHeight)};
        const Rect t = scaleRect(c.rect, scale);
        D3D12_RECT tr{std::max<LONG>(t.left, 0), std::max<LONG>(t.top, 0), std::min<LONG>(t.right, width),
                      std::min<LONG>(t.bottom, height)};
        if (wr.right > wr.left && wr.bottom > wr.top) {
            bindPass(kPassWorld);
            if (c.flags & 1) {  // alpha 0: no character or effect there yet (DLSS's mask, 0.22.3)
                const float color[4] = {float((c.argb >> 16) & 255) / 255.0f, float((c.argb >> 8) & 255) / 255.0f,
                                        float(c.argb & 255) / 255.0f, 0.0f};
                list->ClearRenderTargetView(rtvHandle(kRtvWorld), color, 1, &wr);
                worldDrawn = true;
            }
            if (c.flags & 2) list->ClearDepthStencilView(dsvHandle(kDsvWorld), D3D12_CLEAR_FLAG_DEPTH, c.z, 0, 1, &wr);
        }
        if ((c.flags & 1) && tr.right > tr.left && tr.bottom > tr.top) {
            bindPass(kPassLayer);
            const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            list->ClearRenderTargetView(rtvHandle(kRtvLayer), none, 1, &tr);
        }
        return;
    }
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
    if (upscale) {  // part 3: the world's own size
        worldViewport = D3D12_VIEWPORT{0.0f, 0.0f, float(renderWidth), float(renderHeight), gameViewport.minZ,
                                       gameViewport.maxZ};
        const Rect w = scaleRect(gameViewport.rect, worldScale);
        const LONG wl = std::max(w.left, 0), wt = std::max(w.top, 0);
        worldScissor = D3D12_RECT{wl, wt, std::max<LONG>(wl, std::min(w.right, renderWidth)),
                                  std::max<LONG>(wt, std::min(w.bottom, renderHeight))};
    }
    pass = kPassNone;  // the next draw binds its pass with these
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
    // 0.26: a whole picture - how much it looks like fire (an effect drawn from it lights around it).
    if (u.level == 0 && u.rect.left == 0 && u.rect.top == 0 && u.rect.right == t.width && u.rect.bottom == t.height) {
        t.fire = effect_light::fireOf(rec.pixels().data() + u.pixels, t.width, t.height, t.fireColour);
        t.light = effect_light::lightOf(rec.pixels().data() + u.pixels, t.width, t.height, t.lightColour);  // 0.36
    }
    const D3D12_TEXTURE_COPY_LOCATION src = stage(*this, rec.pixels().data() + u.pixels, UINT(u.rect.width()),
                                                  UINT(u.rect.height()));
    if (!src.pResource) return;
    transition(t.resource, t.state, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = t.resource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = u.level;
    list->CopyTextureRegion(&dst, UINT(u.rect.left), UINT(u.rect.top), 0, &src, nullptr);
    if (u.slotCount) noteSpriteCells(t, rec.slots().data() + u.firstSlot, u.slotCount);  // 0.32
    if (ai)
        ai->noteUpload(u.texture, t, u, rec.pixels().data() + u.pixels,
                       u.slotCount ? rec.slots().data() + u.firstSlot : nullptr, u.slotCount);  // with their sources
}

void Renderer::Impl::writeTarget(const FrameRecord& rec, const WriteTargetCmd& w) {
    if (w.rect.empty()) return;
    if (upscale) startFrame();
    if (!upscale && !finishWorld()) return;  // 0.20, 0.21: the world's shadows, colours, glow under the game's pixels
    if (!upscale && glowDrawn && !addGlowToTarget()) return;
    const D3D12_TEXTURE_COPY_LOCATION src = stage(*this, rec.pixels().data() + w.pixels, UINT(w.rect.width()),
                                                  UINT(w.rect.height()));
    if (!src.pResource) return;
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    if (!scaled() && samples == 1 && !upscale) {  // the target has the game's size: straight in
        transition(target, targetState, D3D12_RESOURCE_STATE_COPY_DEST);
        dst.pResource = target;
        list->CopyTextureRegion(&dst, UINT(w.rect.left), UINT(w.rect.top), 0, &src, nullptr);
        return;
    }
    // A larger target (stage 3 spec §5), or one with several samples that takes no copies (stage 4): the client's
    // pixels go into the game-sized helper and are stretched in.
    transition(gameSized, gameState, D3D12_RESOURCE_STATE_COPY_DEST);
    dst.pResource = gameSized;
    list->CopyTextureRegion(&dst, UINT(w.rect.left), UINT(w.rect.top), 0, &src, nullptr);
    const float from[4] = {float(w.rect.left), float(w.rect.top), float(w.rect.right), float(w.rect.bottom)};
    if (upscale) {  // part 3: the game's own pixels cover the world, so they go into the interface layer, opaque
        transition(layer, layerState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        blit(gameSized, gameState, gameSrv, gameWidth, gameHeight, from, rtvHandle(kRtvLayer), scaleRect(w.rect, scale),
             width, height);
        layerDrawn = true;
        return;
    }
    transition(target, targetState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    blit(gameSized, gameState, gameSrv, gameWidth, gameHeight, from, rtvHandle(kRtvTarget), scaleRect(w.rect, scale),
         width, height, -1.0f, samples);
}

// Draws srcRect (in the source's pixels) of a texture into dstRect of a render target with the present pipeline:
// one bilinear sample per pixel when enlarging, an average over each pixel's footprint when shrinking.
bool Renderer::Impl::blit(ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState, UINT srcSrv, int srcW, int srcH,
                          const float srcRect[4], D3D12_CPU_DESCRIPTOR_HANDLE dst, const Rect& dstRect, int dstW,
                          int dstH, float sharpen, UINT dstSamples, bool over, bool copy) {
    if (dstRect.empty() || srcW <= 0 || srcH <= 0) return true;  // nothing to copy
    ID3D12PipelineState* pso = copy ? copyPipeline : over ? overPipeline : presentPipelineFor(dstSamples);
    if (!pso) return true;  // logged when it failed: the copy is skipped
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    // First: a new list starts before this copy records anything.
    if (!makeRoom(linearClamp, linearClamp, 0)) return false;
    placeSamples(false);  // positions for one sample count must not meet a target of another
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp, 0);
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
    list->SetPipelineState(pso);
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
    pass = kPassNone;
    return true;
}

bool Renderer::Impl::createTarget(int w, int h, UINT n, ID3D12Resource*& rt, ID3D12Resource*& ds) {
    D3D12_CLEAR_VALUE black{};
    black.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    black.Color[3] = 1.0f;
    rt = createTexture2D(device, w, h, 1, DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                         D3D12_RESOURCE_STATE_RENDER_TARGET, &black, n);
    D3D12_CLEAR_VALUE farthest{};  // not "far": windef.h defines far as an empty macro
    farthest.Format = DXGI_FORMAT_D32_FLOAT;
    farthest.DepthStencil.Depth = 1.0f;
    // 0.19: typeless - D32 to draw with, R32 for the modern water to read (makeDepthViews).
    ds = createTexture2D(device, w, h, 1, DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                         D3D12_RESOURCE_STATE_DEPTH_WRITE, &farthest, n);
    if (rt && ds) return true;
    releaseCom(rt);
    releaseCom(ds);
    LOG_ERROR("cannot create a %dx%d render target with %u samples", w, h, n);
    return false;
}

UINT Renderer::Impl::supportedSamples(UINT want) {
    if (!list1) return 1;  // the interface could not stay sharp (stage 4 review): no MSAA
    for (UINT n = want >= 8 ? 8 : want >= 4 ? 4 : want >= 2 ? 2 : 1; n > 1; n /= 2) {
        bool ok = true;
        for (DXGI_FORMAT f : {DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT}) {
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS q{};
            q.Format = f;
            q.SampleCount = n;
            ok = ok && SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &q, sizeof q)) &&
                 q.NumQualityLevels > 0;
        }
        if (ok) return n;
    }
    return 1;
}

// A target with several samples is resolved into `resolved` (stage 4); bindTarget() turns it back into a target.
Renderer::Impl::Readable Renderer::Impl::readableTarget() {
    if (samples <= 1 || !resolved) return Readable{target, &targetState};
    transition(target, targetState, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    transition(resolved, resolvedState, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    list->ResolveSubresource(resolved, 0, target, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
    return Readable{resolved, &resolvedState};
}

// ---- Impl: DLSS (part 3, uaro-render64.exe) -------------------------------------------------------------------

void Renderer::Impl::releaseUpscaleTargets() {
    releaseCom(biasMask);
    releaseCom(world);
    releaseCom(worldDepth);
    releaseCom(motion);
    releaseCom(upscaled);
    releaseCom(layer);
    renderWidth = renderHeight = 0;
    upscaledValid = upscaledNow = worldValid = false;
}

bool Renderer::Impl::createUpscaleTargets(int targetW, int targetH) {
    releaseUpscaleTargets();
    int rw = 0, rh = 0;
    if (!upscaler || !createUpscalePipelines() || !upscaler->renderSize(targetW, targetH, rw, rh) || rw <= 0 || rh <= 0)
        return false;
    D3D12_CLEAR_VALUE none{};  // black, alpha 0: the world's too (its alpha is DLSS's mask, 0.22.3)
    none.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    D3D12_CLEAR_VALUE farthest{};
    farthest.Format = DXGI_FORMAT_D32_FLOAT;
    farthest.DepthStencil.Depth = 1.0f;
    D3D12_CLEAR_VALUE still{};
    still.Format = DXGI_FORMAT_R16G16_FLOAT;
    world = createTexture2D(device, rw, rh, 1, DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                            D3D12_RESOURCE_STATE_RENDER_TARGET, &none);
    worldDepth = createTexture2D(device, rw, rh, 1, DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                                 D3D12_RESOURCE_STATE_DEPTH_WRITE, &farthest);
    motion = createTexture2D(device, rw, rh, 1, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                             D3D12_RESOURCE_STATE_RENDER_TARGET, &still);
    upscaled = createTexture2D(device, targetW, targetH, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                               D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                               D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr);
    layer = createTexture2D(device, targetW, targetH, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, &none);
    D3D12_CLEAR_VALUE clearMask{};
    clearMask.Format = DXGI_FORMAT_R8_UNORM;
    biasMask = maskPs ? createTexture2D(device, rw, rh, 1, DXGI_FORMAT_R8_UNORM,
                                        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                        &clearMask)
                      : nullptr;  // without it DLSS takes the history everywhere, as before
    biasState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (biasMask) {
        device->CreateRenderTargetView(biasMask, nullptr, rtvHandle(kRtvBias));
        device->CreateShaderResourceView(biasMask, nullptr, cpuSrvHandle(biasSrv));
    }
    if (!world || !worldDepth || !motion || !upscaled || !layer) {
        LOG_ERROR("cannot create the %dx%d world and its %dx%d picture for %s", rw, rh, targetW, targetH,
                  upscaler->name());
        releaseUpscaleTargets();
        return false;
    }
    worldState = layerState = motionState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    worldDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    upscaledState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    device->CreateRenderTargetView(world, nullptr, rtvHandle(kRtvWorld));
    device->CreateShaderResourceView(world, nullptr, cpuSrvHandle(worldSrv));
    D3D12_DEPTH_STENCIL_VIEW_DESC dv{};
    dv.Format = DXGI_FORMAT_D32_FLOAT;
    dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(worldDepth, &dv, dsvHandle(kDsvWorld));
    D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = DXGI_FORMAT_R32_FLOAT;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(worldDepth, &sv, cpuSrvHandle(depthSrv));
    device->CreateRenderTargetView(motion, nullptr, rtvHandle(kRtvMotion));
    device->CreateShaderResourceView(upscaled, nullptr, cpuSrvHandle(upscaledSrv));
    device->CreateRenderTargetView(upscaled, nullptr, rtvHandle(kRtvUpscaled));  // 0.18: the glow goes onto it
    device->CreateRenderTargetView(layer, nullptr, rtvHandle(kRtvLayer));
    device->CreateShaderResourceView(layer, nullptr, cpuSrvHandle(layerSrv));
    renderWidth = rw;
    renderHeight = rh;
    worldScale = double(rw) / double(gameWidth);
    historyReset = true;
    worldDrawn = layerDrawn = false;
    frameEnded = true;
    upscaleFrame = 0;
    nextJitter();
    return true;
}

void Renderer::Impl::stopUpscaling() {
    dropUpscale = false;
    if (!upscaler) return;
    LOG_ERROR("%s failed %d frames in a row: the world is drawn without it until the game restarts",
              upscaler->name(), upscaleFailures);
    if (upscaler->generates()) upscaler->suspend();
    waitIdle();
    releaseUpscaleTargets();
    upscale = false;
    upscaledValid = upscaledNow = false;
    upscaler = nullptr;
    applyViewport();
}

// Halton (2, 3): DLSS's recommended pattern, its length growing with the upscale (8 phases at DLAA).
void Renderer::Impl::nextJitter() {
    const auto halton = [](uint32_t i, uint32_t base) {
        float f = 1.0f, r = 0.0f;
        for (; i; i /= base) {
            f /= float(base);
            r += f * float(i % base);
        }
        return r;
    };
    const double ratio = renderWidth > 0 ? double(width) / double(renderWidth) : 1.0;
    const uint32_t phases = std::max<uint32_t>(8, uint32_t(8.0 * ratio * ratio + 0.5));
    const uint32_t i = upscaleFrame % phases + 1;
    jitter[0] = halton(i, 2) - 0.5f;
    jitter[1] = halton(i, 3) - 0.5f;
}

void Renderer::Impl::startFrame() {
    if (!frameEnded || !layer) return;
    frameEnded = false;
    bindPass(kPassLayer);
    const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    list->ClearRenderTargetView(rtvHandle(kRtvLayer), none, 0, nullptr);
    layerDrawn = false;
}

bool Renderer::Impl::composite() {
    if (!upscale) return true;
    // Targets made since the frame's draws (setScale: a new size, Pause, DLSS coming on): nothing is in them yet,
    // and the target keeps the old picture setScale stretched into it - not a black frame (0.10 review 6).
    if (!worldDrawn && !layerDrawn && !worldValid) return true;
    if (!begin()) return false;
    static bool failedOnce = false;
    if (worldDrawn && !finishWorld()) return false;  // 0.20, 0.21: its shadows (if no blended draw came), colours
    bool masked = false;
    if (worldDrawn && biasMask && maskPs) {  // 0.22.3: the world's alpha into DLSS's mask
        if (!maskPipeline) {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
            p.pRootSignature = root;
            p.VS = {presentVs->GetBufferPointer(), presentVs->GetBufferSize()};
            p.PS = {maskPs->GetBufferPointer(), maskPs->GetBufferSize()};
            p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            p.SampleMask = UINT_MAX;
            p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            p.RasterizerState.DepthClipEnable = TRUE;
            p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            p.NumRenderTargets = 1;
            p.RTVFormats[0] = DXGI_FORMAT_R8_UNORM;
            p.SampleDesc.Count = 1;
            if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                           reinterpret_cast<void**>(&maskPipeline)))) {
                maskPipeline = nullptr;
                releaseCom(maskPs);  // not tried again
                LOG_ERROR("DLSS's mask of the characters failed: their history is taken as before");
            }
        }
        if (maskPipeline) {
            transition(world, worldState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            Constants mc{};
            if (!effectPass(maskPipeline, biasMask, biasState, rtvHandle(kRtvBias), renderWidth, renderHeight, mc,
                            worldSrv, false))
                return false;
            transition(biasMask, biasState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            masked = true;
        }
    }
    if (worldDrawn) {
        worldValid = true;
        // The motion of every world pixel from the game's camera (motion_ps).
        StageSetup linearClamp;
        linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
        linearClamp.magFilter = d3d7::TFG_LINEAR;
        linearClamp.minFilter = d3d7::TFN_LINEAR;
        if (!makeRoom(linearClamp, linearClamp, 0)) return false;
        const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp, 0);
        const D3D12_GPU_DESCRIPTOR_HANDLE textures = textureTable(depthSrv, depthSrv);
        transition(worldDepth, worldDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(motion, motionState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(kRtvMotion);
        list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        const D3D12_VIEWPORT vp{0.0f, 0.0f, float(renderWidth), float(renderHeight), 0.0f, 1.0f};
        const D3D12_RECT all{0, 0, renderWidth, renderHeight};
        list->RSSetViewports(1, &vp);
        list->RSSetScissorRects(1, &all);
        list->SetGraphicsRootSignature(root);
        list->SetPipelineState(motionPipeline);
        Constants c{};
        c.targetSize[0] = float(renderWidth);
        c.targetSize[1] = float(renderHeight);
        c.scale = float(worldScale);
        c.sourceRect[2] = c.sourceRect[3] = 1.0f;
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
        const bool moving = cameraValid && prevCameraValid;
        if (moving) {
            camera::reprojection(camera, prevCamera, c.reproject);
        } else {
            const float identity[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
            std::memcpy(c.reproject, identity, sizeof identity);
        }
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
        std::memcpy(c.projection, proj, sizeof proj);
        list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
        list->SetGraphicsRootDescriptorTable(kRootTextures, textures);
        list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        list->DrawInstanced(3, 1, 0, 0);

        // The upscale.
        transition(world, worldState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(motion, motionState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(upscaled, upscaledState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Upscaler::Frame f;
        f.list = list;
        f.color = world;
        f.depth = worldDepth;
        f.motion = motion;
        f.output = upscaled;
        f.bias = masked ? biasMask : nullptr;
        f.biasState = biasState;
        f.colorState = worldState;
        f.depthState = worldDepthState;
        f.motionState = motionState;
        f.outputState = upscaledState;
        f.renderW = renderWidth;
        f.renderH = renderHeight;
        f.outW = width;
        f.outH = height;
        f.jitter[0] = jitter[0];
        f.jitter[1] = jitter[1];
        camera::viewToClip(p, gameWidth, gameHeight, f.viewToClip);
        camera::inverse4(f.viewToClip, f.clipToView);
        float r4[16], m[16];
        camera::affine4(c.reproject, r4);
        camera::multiply4(f.clipToView, r4, m);
        camera::multiply4(m, f.viewToClip, f.clipToPrevClip);
        camera::inverse4(f.clipToPrevClip, f.prevClipToClip);
        f.nearZ = p.nearZ;
        f.farZ = p.farZ;
        f.fovY = camera::kFov * camera::kPi / 180.0f;
        f.aspect = float(gameWidth) / float(gameHeight);
        float view[16], eye[16];
        camera::affine4(cameraValid ? camera : c.reproject, view);
        if (camera::inverse4(view, eye)) {  // the camera's axes and place in the world (its y points down)
            for (int k = 0; k < 3; ++k) {
                f.cameraRight[k] = eye[k];
                f.cameraUp[k] = -eye[4 + k];
                f.cameraFwd[k] = eye[8 + k];
                f.cameraPos[k] = eye[12 + k];
            }
        }
        // A jump (a warp, a new map) leaves no history worth keeping.
        const float jump = std::fabs(c.reproject[9]) + std::fabs(c.reproject[10]) + std::fabs(c.reproject[11]);
        f.reset = historyReset || !moving || jump > 400.0f;
        if (timer) list->EndQuery(timer, D3D12_QUERY_TYPE_TIMESTAMP, slot * kTimes + 2);
        const bool ok = upscaler->evaluate(f);
        if (timer) {
            list->EndQuery(timer, D3D12_QUERY_TYPE_TIMESTAMP, slot * kTimes + 3);
            slots[slot].upscaleTimed = true;
        }
        ID3D12DescriptorHeap* heaps[2] = {gpuSrv, samplerHeap};  // Streamline leaves the list's state to us
        list->SetDescriptorHeaps(2, heaps);
        boundPipeline = nullptr;
        boundTextures = boundSamplers = ~0ull;
        pass = kPassNone;
        centered = false;
        if (!ok && !failedOnce) {
            failedOnce = true;
            LOG_ERROR("%s did not run: the world is stretched instead", upscaler->name());
        }
        // 0.18: the world's glow onto the upscaled world - so also onto frame generation's picture without the
        // interface - once per world drawn.
        if (ok && glowDrawn && !addGlow(upscaled, upscaledState, rtvHandle(kRtvUpscaled), width, height, 1, true))
            return false;
        dropGlow();
        upscaledValid = ok;
        upscaledNow = ok;
        // Failing frame after frame, the stretched world would also shake with the jitter: off after a while.
        upscaleFailures = ok ? 0 : upscaleFailures + 1;
        if (upscaleFailures == kUpscaleGiveUp) dropUpscale = true;
        worldDrawn = false;
        historyReset = false;
    }
    // The picture: the world at the target's size, the interface layer over it.
    const float worldAll[4] = {0.0f, 0.0f, float(renderWidth), float(renderHeight)};
    const float targetAll[4] = {0.0f, 0.0f, float(width), float(height)};
    transition(target, targetState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const bool shown = upscaledValid
                           ? blit(upscaled, upscaledState, upscaledSrv, width, height, targetAll, rtvHandle(kRtvTarget),
                                  Rect{0, 0, width, height}, width, height)
                           : blit(world, worldState, worldSrv, renderWidth, renderHeight, worldAll,
                                  rtvHandle(kRtvTarget), Rect{0, 0, width, height}, width, height);
    if (!shown) return false;
    if (layerDrawn &&
        !blit(layer, layerState, layerSrv, width, height, targetAll, rtvHandle(kRtvTarget), Rect{0, 0, width, height},
              width, height, -1.0f, 1, true))
        return false;
    return true;
}

bool Renderer::Impl::ensureGenerationTargets(UINT w, UINT h) {
    if (hudless && uiCopy && w == generationWidth && h == generationHeight) return true;
    if (generationFailed && w == generationFailedW && h == generationFailedH) return false;  // not again every frame
    Slot& s = slots[slot];  // the old ones may still be read by what this list recorded
    if (hudless) s.garbage.push_back(hudless);
    if (uiCopy) s.garbage.push_back(uiCopy);
    hudless = uiCopy = nullptr;
    generationWidth = generationHeight = 0;
    D3D12_CLEAR_VALUE black{};
    black.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    black.Color[3] = 1.0f;
    D3D12_CLEAR_VALUE none{};
    none.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    hudless = createTexture2D(device, int(w), int(h), 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                              D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, &black);
    uiCopy = createTexture2D(device, int(w), int(h), 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                             D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, &none);
    if (!hudless || !uiCopy) {
        releaseCom(hudless);
        releaseCom(uiCopy);
        LOG_ERROR("cannot create frame generation's %ux%u inputs: it stays off at this size", w, h);
        generationFailed = true;
        generationFailedW = w;
        generationFailedH = h;
        return false;
    }
    hudlessState = uiCopyState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    device->CreateRenderTargetView(hudless, nullptr, rtvHandle(kRtvHudless));
    device->CreateShaderResourceView(hudless, nullptr, cpuSrvHandle(hudlessSrv));
    device->CreateRenderTargetView(uiCopy, nullptr, rtvHandle(kRtvUi));
    device->CreateShaderResourceView(uiCopy, nullptr, cpuSrvHandle(uiCopySrv));
    generationWidth = w;
    generationHeight = h;
    return true;
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
        HRESULT hr = E_FAIL;
        if (presentSurface) {  // part 3: the surface the game's window shows (uaro-render64.exe)
            d.Scaling = DXGI_SCALING_STRETCH;
            d.Flags = 0;
            tearingSupported = false;
            IDXGIFactoryMedia* media = nullptr;
            hr = factory->QueryInterface(__uuidof(IDXGIFactoryMedia), reinterpret_cast<void**>(&media));
            if (SUCCEEDED(hr))
                hr = media->CreateSwapChainForCompositionSurfaceHandle(queue, presentSurface, &d, nullptr, &sc1);
            releaseCom(media);
        } else {
            hr = factory->CreateSwapChainForHwnd(queue, hwnd, &d, nullptr, nullptr, &sc1);
            refused = FAILED(hr);
        }
        if (SUCCEEDED(hr)) hr = sc1->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain));
        if (FAILED(hr)) {
            failed(4, presentSurface ? "swap chain on the composition surface" : "swap chain creation", hr);
            releaseCom(sc1);
            failedWidth = w;
            failedHeight = h;
            swapRetry.failed(GetTickCount64());
            return false;
        }
        sc1->Release();
        if (!presentSurface) factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        window = hwnd;
        LOG_INFO("swap chain %ux%u%s%s", w, h, tearingSupported ? ", tearing allowed" : "",
                 presentSurface ? " on a composition surface" : "");
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
    delete ai;  // its last results go into upscaled.grf
    ai = nullptr;
    releaseSwapChain();
    for (auto& t : textures) {
        releaseCom(t.second.resource);
        releaseCom(t.second.ai);
    }
    textures.clear();
    for (auto& p : pipelines) releaseCom(p.second);
    pipelines.clear();
    for (ID3D12PipelineState*& p : presentPipelines) releaseCom(p);
    releaseCom(motionPipeline);
    releaseCom(timer);
    releaseCom(timerBuffer);
    timerData = nullptr;
    releaseCom(copyPipeline);
    releaseCom(hudless);
    releaseCom(uiCopy);
    releaseCom(overPipeline);
    releaseCom(motionPs);
    releaseCom(copyPs);
    releaseCom(glow);  // 0.18
    releaseCom(glowResolved);
    for (ID3D12Resource*& r : glowChain) releaseCom(r);
    releaseCom(glowDownPipeline);
    releaseCom(glowUpPipeline);
    for (ID3D12PipelineState*& p : glowAddPipelines) releaseCom(p);
    releaseCom(drawGlowPs);
    releaseCom(glowDownPs);
    releaseCom(glowUpPs);
    releaseCom(glowAddPs);
    releaseCom(sceneColor);  // 0.19
    releaseCom(sceneDepth);
    releaseCom(depthCopyPipeline);
    releaseCom(depthCopyMsPipeline);
    releaseCom(waterVs);
    releaseCom(waterPs);
    releaseCom(waterGlowPs);
    releaseCom(depthCopyPs);
    releaseCom(depthCopyMsPs);
    for (ID3D12Resource*& r : aoTarget) releaseCom(r);  // 0.20
    releaseCom(aoPipeline);
    releaseCom(aoBlurPipeline);
    for (ID3D12PipelineState*& p : aoApplyPipelines) releaseCom(p);
    for (ID3D12PipelineState*& p : glowLightPipelines) releaseCom(p);
    for (ID3D12PipelineState*& p : gradePipelines) releaseCom(p);  // 0.21
    releaseCom(gradePs);
    for (ID3D12Resource*& r : dofTarget) releaseCom(r);  // 0.22
    for (ID3D12Resource*& r : dofFocus) releaseCom(r);
    releaseCom(dofFocusPipeline);
    releaseCom(dofFocusPs);
    releaseCom(maskPipeline);  // 0.22.3
    releaseCom(maskPs);
    for (ID3D12PipelineState*& p : torchLightPipelines) releaseCom(p);  // 0.23
    for (ID3D12PipelineState*& p : torchFirePipelines) releaseCom(p);
    for (ID3D12PipelineState*& p : fireflyPipelines) releaseCom(p);  // 0.28
    for (ID3D12PipelineState*& p : dustPipelines) releaseCom(p);      // 0.29
    releaseCom(torchLightPs);
    releaseCom(torchFirePs);
    releaseCom(fireflyPs);
    releaseCom(dustPs);
    releaseCom(rayPs);  // 0.30
    releaseCom(rayGlowPs);
    releaseCom(lavaPs);  // 0.31
    releaseCom(lavaGlowPs);
    for (ID3D12Resource*& r : shadowMask) releaseCom(r);  // 0.32
    releaseCom(shadowQuadPipeline);
    releaseCom(shadowBlurPipeline);
    for (ID3D12PipelineState*& p : shadowApplyPipelines) releaseCom(p);
    releaseCom(shadowVs);
    releaseCom(shadowPs);
    releaseCom(shadowBlurPs);
    releaseCom(shadowApplyPs);
    for (ID3D12PipelineState*& p : cloudsPipelines) releaseCom(p);  // 0.33
    releaseCom(cloudsPs);
    releaseCom(cloudsMask);  // 0.39.3
    releaseCom(cloudsMaskPipeline);
    releaseCom(cloudsApplyPs);
    for (ID3D12PipelineState*& p : hazePipelines) releaseCom(p);  // 0.34
    releaseCom(hazePs);
    for (ID3D12PipelineState*& p : weatherPipelines) releaseCom(p);  // 0.39
    releaseCom(weatherPs);
    for (ID3D12PipelineState*& p : heatPipelines) releaseCom(p);  // 0.35
    releaseCom(heatPs);
    releaseCom(heatData);
    releaseCom(torchData);
    releaseCom(smokePs);  // 0.24
    releaseCom(smokeGlowPs);
    releaseCom(dofDownPipeline);
    releaseCom(dofBlurPipeline);
    for (ID3D12PipelineState*& p : dofApplyPipelines) releaseCom(p);
    releaseCom(dofDownPs);
    releaseCom(dofBlurPs);
    releaseCom(dofApplyPs);
    releaseCom(aoPs);
    releaseCom(aoBlurPs);
    releaseCom(aoApplyPs);
    releaseUpscaleTargets();
    releaseCom(root);
    releaseCom(drawVs);
    releaseCom(drawPs);
    releaseCom(presentVs);
    releaseCom(presentPs);
    releaseCom(readback);
    releaseCom(target);
    releaseCom(depth);
    releaseCom(resolved);
    releaseCom(gameSized);
    for (Slot& s : slots) {
        for (IUnknown* g : s.garbage) g->Release();
        s.garbage.clear();
        releaseCom(s.upload);
        releaseCom(s.allocator);
    }
    releaseCom(list1);
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
    delete remote_;  // the renderer process finishes first
    delete remix_;
    impl_->destroy();
    delete impl_;
}

bool Renderer::createRemote(const BridgeOptions& o, std::string& error) {
    if (remote_) return true;
    remote_ = new Remote;
    remote_->setStartSink(impl_->startSink, impl_->startContext);  // 0.40: its start is shown as it goes
    if (remote_->start(o, error)) return true;
    delete remote_;
    remote_ = nullptr;
    return false;
}

bool Renderer::createRemix(const std::string& d3d9Path, HWND window, std::string& error) {
    if (remix_) return true;
    remix_ = new Remix9;
    if (remix_->create(d3d9Path, window, error)) return true;
    delete remix_;
    remix_ = nullptr;
    return false;
}

void Renderer::setWindow(HWND window) {
    if (remix_) remix_->setWindow(window);
}

std::string Renderer::adapterName() const {
    return remote_ ? remote_->adapterName() : remix_ ? remix_->adapterName() : impl_->adapter;
}

void Renderer::setUiSharp(bool sharp) {
    if (remix_) return;
    if (remote_) return remote_->setUiSharp(sharp);
    impl_->uiSharp = sharp;
}

bool Renderer::upscales() const { return remote_ && remote_->upscales(); }

int Renderer::samples() const { return remix_ ? 1 : remote_ ? remote_->samples() : int(impl_->samples); }

void Renderer::setUpscaler(Upscaler* u) { impl_->upscaler = u; }

void Renderer::setApi(HMODULE module) { impl_->api = module; }

void Renderer::setOnDevice(void (*fn)(void* context, ID3D12Device* device), void* context) {
    impl_->onDevice = fn;
    impl_->onDeviceContext = context;
}

bool Renderer::upgradeFactory(bool (*upgrade)(void** object)) {
    Impl& m = *impl_;
    if (remote_ || remix_ || !m.factory || !upgrade) return false;
    void* object = m.factory;
    if (!upgrade(&object) || !object || object == m.factory) return false;
    m.waitIdle();
    m.releaseSwapChain();  // made again at the next present, through Streamline's factory
    m.factory->Release();  // Streamline's factory holds the system's one itself
    m.factory = static_cast<IDXGIFactory4*>(object);
    return true;
}

bool Renderer::startAiTextures(const std::string& gameDir, const AiSettings& settings) {
    Impl& m = *impl_;
    if (remote_ || remix_ || m.ai) return m.ai != nullptr;
    m.ai = new AiTextures(m);
    std::string error;
    if (!m.ai->start(gameDir, settings, error)) {
        LOG_ERROR("AI textures: %s; they stay off", error.c_str());
        delete m.ai;
        m.ai = nullptr;
        return false;
    }
    return true;
}

void Renderer::finish() {
    if (!remote_ && impl_->device && impl_->fence) impl_->waitIdle();
}

void Renderer::requestIcon(uint64_t key, int width, int height, const uint32_t* argb) {
    if (remote_) return remote_->sendIcon(key, width, height, argb);
    if (impl_->ai) impl_->ai->noteIcon(key, width, height, argb);
}

void Renderer::setStartSink(StartSink sink, void* context) {
    impl_->startSink = sink;
    impl_->startContext = context;
    if (remote_) remote_->setStartSink(sink, context);
}

void Renderer::setIconSink(IconSink sink, void* context) {
    impl_->iconSink = sink;
    impl_->iconContext = context;
    if (remote_) remote_->setIconSink(sink, context);
}

void Renderer::setMap(const std::string& map) {
    if (remix_) return;
    if (remote_) return remote_->setMap(map);
    impl_->mapWorld.setMap(map);
    impl_->mapName = map;  // 0.31: lava's maps
    impl_->mapOutdoors = outdoors(map);  // 0.33: the clouds' shadows
    impl_->mapLook = haze::lookOf(map);  // 0.34: its haze and fog
    impl_->mapDesert = desert(map);      // 0.35: its heat haze far off
    impl_->mapWeather = weather::of(map);  // 0.39: its weather
    impl_->weatherLogged = false;
    if (impl_->ai) impl_->ai->setMap(map);
}

void Renderer::setShaderCache(const std::string& file) { impl_->shaderCacheFile = file; }

void Renderer::setGameDir(const std::string& dir) {
    if (remix_ || remote_) return;  // uaro-render64.exe is given it at its start
    impl_->mapWorld.setGameDir(dir);
}

void Renderer::setCamera(bool valid, const float view[12]) {
    if (remix_) return remix_->setCamera(valid, view);
    if (remote_) return remote_->setCamera(valid, view);
    Impl& m = *impl_;
    m.cameraValid = valid;
    if (valid) std::memcpy(m.camera, view, sizeof m.camera);
    float v4[16], inv[16];  // 0.19: view -> world for the water
    camera::affine4(m.camera, v4);
    m.viewToWorldValid = valid && camera::inverse4(v4, inv);
    if (m.viewToWorldValid)
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c) m.viewToWorld[3 * r + c] = inv[4 * r + c];
}

ID3D12Device* Renderer::device() const { return impl_->device; }

bool Renderer::upscaling() const { return impl_->upscale; }

bool Renderer::createTextureAs(uint32_t id, int width, int height, int levels) {
    Impl& m = *impl_;
    if (!id || m.textures.count(id)) return false;
    const uint32_t made = createTexture(width, height, levels);
    if (!made) return false;
    Texture t = m.textures[made];
    m.textures.erase(made);
    m.textures.emplace(id, t);
    return true;
}

void Renderer::setPresentSurface(HANDLE surface) {
    Impl& m = *impl_;
    if (m.presentSurface == surface) return;
    m.waitIdle();
    m.releaseSwapChain();
    if (m.presentSurface) CloseHandle(m.presentSurface);
    m.presentSurface = surface;
    m.failedWidth = m.failedHeight = 0;
}

bool Renderer::presentRefused() const { return impl_->refused; }

bool Renderer::lost() const { return impl_->lost; }

void Renderer::setDrawQuality(int anisotropy, bool alphaToCoverage, bool aiShown) {
    if (remix_) return;
    if (remote_) return remote_->setDrawQuality(anisotropy, alphaToCoverage, aiShown);
    Impl& m = *impl_;
    if (m.ai) m.ai->shown = aiShown;
    if (anisotropy == m.anisotropy && alphaToCoverage == m.alphaToCoverage) return;
    m.anisotropy = anisotropy;
    m.alphaToCoverage = alphaToCoverage;
    LOG_INFO("quality: anisotropy %d, alpha to coverage %s", anisotropy, alphaToCoverage ? "on" : "off");
}

void Renderer::setEffects(const EffectSettings& e) {
    if (remix_) return;
    if (remote_) return remote_->setEffects(e);
    Impl& m = *impl_;
    if (e == m.effects) return;
    const bool was = m.glowWanted();
    const bool scene =
        m.waterWanted() || m.aoWanted() || m.gradeWanted() || m.dofWanted() || m.torchWanted() || m.fireflyWanted() ||
        m.mapGlowsWanted() || m.actorShadowsWanted() || m.cloudsWanted() || m.hazeWanted() || m.heatWanted();
    const bool dof = m.dofWanted();
    const bool ao = m.aoWanted();
    const bool actorShadows = m.actorShadowsWanted();
    const bool clouds = m.cloudsWanted();
    m.effects = e;
    LOG_INFO("effects: smooth map light %s; torches %s, new flame %s, brightness %d%%, glow %d%%, light %d%%, flicker "
             "%d%%; chimney smoke %s, density %d%%; lamps %s, light %d%%; fire of skills lights %s, %d%%, "
             "the others %s; "
             "fireflies %s, %d%%; hidden lights %s, %d%%; forest beams %s, %d%%; lava %s, %d%%; "
             "characters' shadows %s, %d%%; clouds %s, %d%%; haze %s, %d%%; heat haze %s, %d%%; portals %s, %d%%; "
             "weather %s, %d%%; shadows %s, "
             "strength %d%%, size %d%%; glow %s, strength %d%%, size %d%%, light %d%%; "
             "water %s, waves %d%%, reflections %d%%, clarity %d%%; colour %s, saturation %d%%, contrast %d%%, "
             "brightness %d%%, warmth %d%%, vignette %d%%; depth of field %s, blur %d%%, sharp zone %d%%",
             e.smoothLight ? "on" : "off", e.torch ? "on" : "off", e.torchFlame ? "on" : "off", e.torchBrightness,
             e.torchGlow, e.torchLight, e.torchFlicker, e.smoke ? "on" : "off", e.smokeDensity, e.lamps ? "on" : "off",
             e.lampsLight, e.fireLight ? "on" : "off", e.fireLightStrength, e.skillColours ? "on" : "off",
             e.fireflies ? "on" : "off",
             e.firefliesBrightness, e.mapGlows ? "on" : "off", e.mapGlowsBrightness, e.rays ? "on" : "off",
             e.raysBrightness, e.lava ? "on" : "off", e.lavaBrightness, e.actorShadows ? "on" : "off",
             e.actorShadowsStrength, e.clouds ? "on" : "off", e.cloudsStrength, e.haze ? "on" : "off", e.hazeStrength,
             e.heat ? "on" : "off", e.heatStrength, e.portals ? "on" : "off", e.portalsStrength,
             e.weather ? "on" : "off", e.weatherAmount,
             e.ao ? "on" : "off", e.aoStrength, e.aoSize, e.glow ? "on" : "off",
             e.glowStrength, e.glowSize,
             e.glowLight, e.water ? "on" : "off", e.waterWaves, e.waterReflections, e.waterClarity,
             e.grade ? "on" : "off", e.gradeSaturation, e.gradeContrast, e.gradeBrightness, e.gradeWarmth,
             e.gradeVignette, e.dof ? "on" : "off", e.dofStrength, e.dofRange);
    if (was && !m.glowWanted()) m.releaseGlow();  // its video memory back
    if (ao && !m.aoWanted()) m.releaseAo();
    if (actorShadows && !m.actorShadowsWanted()) m.releaseShadowMask();  // 0.32
    if (clouds && !m.cloudsWanted()) m.releaseCloudsMask();               // 0.39.3
    if (dof && !m.dofWanted()) m.releaseDof();
    if (scene && !m.waterWanted() && !m.aoWanted() && !m.gradeWanted() && !m.dofWanted() && !m.torchWanted() &&
        !m.fireflyWanted() && !m.mapGlowsWanted() && !m.actorShadowsWanted() && !m.cloudsWanted() && !m.hazeWanted() &&
        !m.heatWanted())
        m.releaseScene();
}

void Renderer::preparePipelines(const std::string& file) {
    if (remix_) return;
    if (remote_) return remote_->preparePipelines(file);
    Impl& m = *impl_;
    std::string text;
    win::readTextFile(file, text);
    const std::vector<uint64_t> keys = parsePipelineKeys(text);
    const std::set<uint64_t> unique(keys.begin(), keys.end());
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    std::vector<uint64_t> built;
    for (const uint64_t k : unique)
        if (m.pipelineForKey(k)) built.push_back(k);
    QueryPerformanceCounter(&t1);
    LOG_INFO("pipelines: %u of %u saved ones prepared in %.0f ms", unsigned(built.size()), unsigned(unique.size()),
             double(t1.QuadPart - t0.QuadPart) * 1000.0 / double(freq.QuadPart));
    if (FAILED(m.device->GetDeviceRemovedReason())) return;  // a lost device built nothing: the file stays (0.4.2 review)
    // Written anew with the keys that built: a list from another version, broken lines or keys listed twice start
    // clean. Keys another running client added meanwhile stay (0.4.2 review).
    std::string latest;
    win::readTextFile(file, latest);
    for (const uint64_t k : parsePipelineKeys(latest))
        if (!unique.count(k)) built.push_back(k);
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    f << pipelineKeysText(built);
    f.close();
    if (f)
        m.pipelineFile = file;  // from now on every new pipeline is added
    else
        LOG_INFO("cannot write %s: pipelines are not remembered for the next start", file.c_str());
}

bool Renderer::create(std::string& error) {
    Impl& m = *impl_;
    m.d3d12 = m.api ? m.api : LoadLibraryA("d3d12.dll");  // part 3: Streamline's interposer when it is given
    m.dxgi = m.api ? m.api : LoadLibraryA("dxgi.dll");
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
    if (m.onDevice) m.onDevice(m.onDeviceContext, m.device);
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
    D3D12_FEATURE_DATA_D3D12_OPTIONS2 options2{};
    if (SUCCEEDED(m.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &options2, sizeof options2)) &&
        options2.ProgrammableSamplePositionsTier >= D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_1 &&
        SUCCEEDED(m.list->QueryInterface(__uuidof(ID3D12GraphicsCommandList1), reinterpret_cast<void**>(&m.list1)))) {
        LOG_DEBUG("msaa: sample positions tier %d", int(options2.ProgrammableSamplePositionsTier));
    } else {
        m.list1 = nullptr;
        LOG_INFO("msaa: this card cannot place samples, so MSAA stays off (the interface would lose sharpness)");
    }
    // GPU time for the log: without it nothing else changes.
    D3D12_QUERY_HEAP_DESC qh{};
    qh.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qh.Count = kSlots * kTimes;
    void* times = nullptr;
    if (SUCCEEDED(m.queue->GetTimestampFrequency(&m.timerFrequency)) && m.timerFrequency &&
        SUCCEEDED(m.device->CreateQueryHeap(&qh, __uuidof(ID3D12QueryHeap), reinterpret_cast<void**>(&m.timer))) &&
        (m.timerBuffer = createBuffer(m.device, D3D12_HEAP_TYPE_READBACK, kSlots * kTimes * sizeof(UINT64),
                                      D3D12_RESOURCE_STATE_COPY_DEST)) &&
        SUCCEEDED(m.timerBuffer->Map(0, nullptr, &times))) {
        m.timerData = static_cast<const UINT64*>(times);
    } else {
        releaseCom(m.timer);
        releaseCom(m.timerBuffer);
    }
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
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 2, false, m.dsvHeap)) {
        error = "descriptor heaps";
        return false;
    }
    m.srvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m.samplerStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    m.rtvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m.dsvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m.targetSrv = m.allocSrv();
    m.gameSrv = m.allocSrv();
    m.depthSrv = m.allocSrv();  // part 3 (DLSS): the world's depth, its picture, the interface layer, the world
    m.upscaledSrv = m.allocSrv();
    m.layerSrv = m.allocSrv();
    m.worldSrv = m.allocSrv();
    m.hudlessSrv = m.allocSrv();  // frame generation
    m.uiCopySrv = m.allocSrv();
    m.glowSrv = m.allocSrv();  // 0.18: the glow, resolved, and its chain
    m.glowResolvedSrv = m.allocSrv();
    for (UINT& srv : m.glowChainSrv) srv = m.allocSrv();
    m.depthTargetSrv = m.allocSrv();  // 0.19: the target's depth, the picture and depth under the water
    m.sceneColorSrv = m.allocSrv();
    m.sceneDepthSrv = m.allocSrv();
    for (UINT& srv : m.aoSrv) srv = m.allocSrv();  // 0.20: the soft shadows
    for (UINT& srv : m.shadowMaskSrv) srv = m.allocSrv();  // 0.32: the characters' shadows
    m.cloudsMaskSrv = m.allocSrv();  // 0.39.3: the clouds' mask
    for (UINT& srv : m.dofSrv) srv = m.allocSrv();  // 0.22: the depth of field
    for (UINT& srv : m.dofFocusSrv) srv = m.allocSrv();
    m.biasSrv = m.allocSrv();  // 0.22.3: DLSS's mask
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
    if (remix_) return remix_->setTarget(width, height);
    if (remote_) return remote_->setTarget(width, height);
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
    releaseCom(m.resolved);
    releaseCom(m.gameSized);
    m.releaseUpscaleTargets();
    m.dropGlow();
    m.newPicture();
    m.upscale = false;
    m.samples = 1;  // until the first present, like the scale: the calibration sees DX7's picture
    if (!m.createTarget(width, height, 1, m.target, m.depth)) return fail();
    m.gameSized = createTexture2D(m.device, width, height, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
    if (!m.gameSized) {
        LOG_ERROR("cannot create the %dx%d helper of the game's size", width, height);
        return fail();
    }
    m.targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m.gameState = D3D12_RESOURCE_STATE_COPY_DEST;
    m.device->CreateRenderTargetView(m.target, nullptr, m.rtvHandle(kRtvTarget));
    m.makeDepthViews(1);
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

bool Renderer::setScale(double k, int targetWidth, int targetHeight, int samples, bool upscale) {
    if (remix_) return true;  // Remix draws at its own size
    if (remote_) return remote_->setScale(k, targetWidth, targetHeight, samples, upscale);
    Impl& m = *impl_;
    if (m.lost || !m.target || targetWidth <= 0 || targetHeight <= 0) return false;
    const bool wantUpscale = upscale && m.upscaler;  // part 3: DLSS replaces MSAA
    UINT n = wantUpscale ? 1 : m.supportedSamples(UINT(std::max(samples, 1)));
    if (k == m.scale && targetWidth == m.width && targetHeight == m.height && n == m.samples &&
        wantUpscale == m.upscale)
        return true;
    ID3D12Resource* target = nullptr;
    ID3D12Resource* depth = nullptr;
    ID3D12Resource* resolved = nullptr;
    const auto make = [&](UINT count) {
        if (!m.createTarget(targetWidth, targetHeight, count, target, depth)) return false;
        if (count == 1) return true;
        resolved = createTexture2D(m.device, targetWidth, targetHeight, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                   D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_RESOLVE_DEST, nullptr);
        if (resolved && m.presentPipelineFor(count)) return true;
        LOG_ERROR("cannot resolve a %dx%d target with %u samples", targetWidth, targetHeight, count);
        releaseCom(target);
        releaseCom(depth);
        releaseCom(resolved);
        return false;
    };
    while (!make(n)) {  // stage 4: no memory for that many samples - half as many, down to one
        if (n == 1) return false;
        n /= 2;
    }
    // The back buffer keeps its picture across a present: the old target is stretched into the new one.
    const auto drop = [&]() {
        releaseCom(target);
        releaseCom(depth);
        releaseCom(resolved);
        return false;
    };
    if (!m.begin()) return drop();
    m.device->CreateRenderTargetView(target, nullptr, m.rtvHandle(kRtvNew));
    const Impl::Readable old = m.readableTarget();
    const float all[4] = {0.0f, 0.0f, float(m.width), float(m.height)};
    if (!m.blit(old.resource, *old.state, m.targetSrv, m.width, m.height, all, m.rtvHandle(kRtvNew),
                Rect{0, 0, targetWidth, targetHeight}, targetWidth, targetHeight, -1.0f, n))
        return drop();
    m.waitIdle();
    m.dropGlow();  // 0.18: drawn for the old target
    m.newPicture();
    releaseCom(m.target);
    releaseCom(m.depth);
    releaseCom(m.resolved);
    m.target = target;
    m.depth = depth;
    m.resolved = resolved;
    m.targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m.resolvedState = D3D12_RESOURCE_STATE_RESOLVE_DEST;
    const UINT was = m.samples;
    m.samples = n;
    m.device->CreateRenderTargetView(m.target, nullptr, m.rtvHandle(kRtvTarget));
    m.makeDepthViews(n);
    m.device->CreateShaderResourceView(n > 1 ? m.resolved : m.target, nullptr, m.cpuSrvHandle(m.targetSrv));
    m.width = targetWidth;
    m.height = targetHeight;
    m.scale = k;
    m.applyViewport();
    if (m.begin()) {  // the target is in place; a lost device can neither clear its depth nor draw (0.4.2 review)
        m.list->ClearDepthStencilView(m.dsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        m.submit();
    }
    LOG_DEBUG("render target %dx%d with %u samples: %.3f times the game's %dx%d", targetWidth, targetHeight, n, k,
              m.gameWidth, m.gameHeight);
    const bool wasUpscaling = m.upscale;
    m.upscale = false;
    if (wantUpscale && m.createUpscaleTargets(targetWidth, targetHeight)) {
        m.upscale = true;
        m.applyViewport();
        LOG_INFO("%s: the world at %dx%d, shown at %dx%d", m.upscaler->name(), m.renderWidth, m.renderHeight,
                 targetWidth, targetHeight);
    } else {
        if (wantUpscale) LOG_ERROR("%s cannot work at %dx%d: the world is drawn as before", m.upscaler->name(),
                                   targetWidth, targetHeight);
        else if (wasUpscaling) LOG_INFO("upscaling off");
        m.releaseUpscaleTargets();
    }
    if (n != was || (int(n) < samples && samples != m.askedSamples))
        LOG_INFO("msaa: %u sample%s per pixel%s", n, n == 1 ? "" : "s",
                 int(n) < samples ? " (fewer than asked: the card or its memory has no more)" : "");
    m.askedSamples = samples;
    return true;
}

uint32_t Renderer::createTexture(int width, int height, int levels) {
    if (remix_) return remix_->createTexture(width, height, levels);
    if (remote_) return remote_->createTexture(width, height, levels);
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

void Renderer::noteObjectTexture(uint32_t id, uint8_t kind) {
    if (remix_) return;
    if (remote_) return remote_->noteObjectTexture(id, kind);
    impl_->objectTextures.note(id, objects::Kind(kind));
}

void Renderer::releaseTexture(uint32_t id) {
    if (remix_) return remix_->releaseTexture(id);
    if (remote_) return remote_->releaseTexture(id);
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
        if (it->second.ai) {
            s.garbage.push_back(it->second.ai);
            freeSrv.push_back(it->second.aiSrv);
        }
        if (ai) ai->forget(id);
        objectTextures.forget(id);  // 0.23.1
        textures.erase(it);
    }
    dying.clear();
}

void Renderer::execute(const FrameRecord& record) {
    if (remix_) return remix_->execute(record);
    if (remote_) return remote_->execute(record);
    Impl& m = *impl_;
    if (m.lost || !m.target || record.empty() || !m.begin()) return;
    if (m.upscale && m.upscaler && !m.renderStarted) {  // part 3: latency markers
        m.renderStarted = true;
        m.upscaler->renderStart();
    }
    m.prepareGeometry(record);
    for (const Command& c : record.commands()) {
        if (!m.open) break;  // the list could not go on (the GPU is lost): nothing more is recorded
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
    if (remix_) return remix_->readTarget(argb);
    if (remote_) return remote_->readTarget(argb);
    Impl& m = *impl_;
    if (m.lost || !m.target || !m.begin()) return false;
    if (!m.composite()) return false;  // part 3: the picture as it would be shown
    if (!m.upscale && !m.finishWorld()) return false;  // 0.20, 0.21, 0.18: a frame without an interface
    if (!m.upscale && m.glowDrawn && !m.addGlowToTarget()) return false;
    const Impl::Readable r = m.readableTarget();  // resolved first when it has several samples (stage 4)
    ID3D12Resource* source = r.resource;
    D3D12_RESOURCE_STATES* sourceState = r.state;
    if (m.scaled()) {  // the client reads its own size: shrink the target into the helper first (stage 3 spec §5)
        m.transition(m.gameSized, m.gameState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float all[4] = {0.0f, 0.0f, float(m.width), float(m.height)};
        if (!m.blit(source, *sourceState, m.targetSrv, m.width, m.height, all, m.rtvHandle(kRtvGame),
                    Rect{0, 0, m.gameWidth, m.gameHeight}, m.gameWidth, m.gameHeight))
            return false;  // no stale frame for the client
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
    const UINT64 before = m.fenceValue;
    m.submit();
    if (m.fenceValue == before) return false;  // the list did not reach the GPU (Close failed, lost): no copy
    m.waitIdle();
    const HRESULT removed = m.device->GetDeviceRemovedReason();
    if (FAILED(removed)) {  // removed meanwhile: the wait ended without the copy (0.4.2 review)
        m.failed(6, "reading the frame back", removed);
        return false;
    }
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
    if (remix_) return remix_->present(window, options);
    if (remote_) return remote_->present(window, outputWidth, outputHeight, source, options);
    Impl& m = *impl_;
    if (m.lost) {  // the GPU is gone (stage 3 review M2): no Present waits for vblank, keep the game's loop from spinning
        Sleep(15);
        return;
    }
    if (!m.target || !window || source.empty()) return;
    // Part 3: frame generation must be off while the swap chain changes, for frames it has no inputs for, while
    // the game's window is not in front (NVIDIA asks for it off before the window is minimised or moved; the game in
    // full screen minimises itself when it loses the focus), and on a composition surface, whose swap chain
    // Streamline never sees (0.10 review 4). Off takes a present to apply: a new size waits a frame while frame
    // generation was on, so the swap chain never changes under it (0.10 review 3).
    UINT wantW = UINT(std::max(1, outputWidth)), wantH = UINT(std::max(1, outputHeight));
    const bool resize = m.swapChain && (wantW != m.swapWidth || wantH != m.swapHeight);
    const bool held = !m.upscale || !options.foreground || resize || m.presentSurface;
    if (m.upscaler && m.upscaler->generates() && held) {
        if (m.presentSurface && !m.surfaceNoted) {
            m.surfaceNoted = true;
            LOG_INFO("DLSS frame generation needs the game's own window: off with a composition surface");
        }
        if (m.upscaler->generating() && resize) {
            wantW = m.swapWidth;
            wantH = m.swapHeight;
        }
        m.upscaler->suspend();
    }
    if (!m.ensureSwapChain(window, wantW, wantH, options.allowTearing)) {
        Sleep(15);  // no swap chain (0.4.1 review): no Present waits for vblank, keep the game's loop from spinning
        return;
    }
    if (!m.begin() || !m.composite()) return;  // part 3: the upscaled world and the interface layer first
    if (!m.upscale && !m.finishWorld()) return;  // 0.20, 0.21, 0.18: a frame without an interface
    if (!m.upscale && m.glowDrawn && !m.addGlowToTarget()) return;
    const Impl::Readable shown = m.readableTarget();  // resolved first when it has several samples (stage 4)
    const UINT index = m.swapChain->GetCurrentBackBufferIndex();
    m.transition(m.backBuffers[index], m.backStates[index], D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m.rtvHandle(kRtvSwap + index);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m.list->ClearRenderTargetView(rtv, black, 0, nullptr);
    // Fit the source into the window, keeping its shape, on whole pixels: bars where the shapes differ.
    const Rect fit = fitPicture(source.width(), source.height(), int(m.swapWidth), int(m.swapHeight));
    if (!(fit == m.picture)) {
        m.picture = fit;
        LOG_DEBUG("picture %dx%d of the game shown as %dx%d at %d,%d in the %ux%u window", source.width(),
                 source.height(), fit.width(), fit.height(), fit.left, fit.top, m.swapWidth, m.swapHeight);
    }
    // The source is in game pixels: into target pixels by each axis' own ratio, so the whole frame is exactly the
    // whole target and at render_scale 100, where the target is the picture, it is copied pixel for pixel.
    const auto tx = [&m](int x) { return float(double(x) * m.width / m.gameWidth); };
    const auto ty = [&m](int y) { return float(double(y) * m.height / m.gameHeight); };
    const float from[4] = {tx(source.left), ty(source.top), tx(source.right), ty(source.bottom)};
    // Only a frame whose world was upscaled now has its constants, depth and motion: a frame that drew no world
    // (the last ones before the game closes) would have DLSS-G look for them in vain (0.10 log, frame 1079).
    const bool generation = m.upscale && m.upscaler && m.upscaler->generates() && m.upscaledNow && !held &&
                            m.ensureGenerationTargets(m.swapWidth, m.swapHeight);
    if (!generation && m.upscaler && m.upscaler->generates()) m.upscaler->suspend();  // no inputs for this frame
    if (generation) {
        // Part 3, frame generation: the back buffer is put together from the picture without the interface and the
        // interface alone (premultiplied), exactly as DLSS-G puts generated frames together: UI + (1 - a) * picture.
        const Rect whole{0, 0, int(m.swapWidth), int(m.swapHeight)};
        const float wholeF[4] = {0.0f, 0.0f, float(m.swapWidth), float(m.swapHeight)};
        const float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        m.transition(m.hudless, m.hudlessState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m.list->ClearRenderTargetView(m.rtvHandle(kRtvHudless), clearColor, 0, nullptr);
        m.transition(m.uiCopy, m.uiCopyState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m.list->ClearRenderTargetView(m.rtvHandle(kRtvUi), none, 0, nullptr);
        if (!m.blit(m.upscaled, m.upscaledState, m.upscaledSrv, m.width, m.height, from, m.rtvHandle(kRtvHudless), fit,
                    int(m.swapWidth), int(m.swapHeight), options.sharpen))
            return;
        if (m.layerDrawn && !m.blit(m.layer, m.layerState, m.layerSrv, m.width, m.height, from, m.rtvHandle(kRtvUi),
                                    fit, int(m.swapWidth), int(m.swapHeight), -1.0f, 1, false, true))
            return;
        if (!m.blit(m.hudless, m.hudlessState, m.hudlessSrv, int(m.swapWidth), int(m.swapHeight), wholeF, rtv, whole,
                    int(m.swapWidth), int(m.swapHeight)) ||
            !m.blit(m.uiCopy, m.uiCopyState, m.uiCopySrv, int(m.swapWidth), int(m.swapHeight), wholeF, rtv, whole,
                    int(m.swapWidth), int(m.swapHeight), -1.0f, 1, true))
            return;
        Upscaler::Generation g;
        g.list = m.list;
        g.hudless = m.hudless;
        g.ui = m.uiCopy;
        g.hudlessState = m.hudlessState;
        g.uiState = m.uiCopyState;
        g.picture = RECT{fit.left, fit.top, fit.right, fit.bottom};
        g.backWidth = int(m.swapWidth);
        g.backHeight = int(m.swapHeight);
        g.on = m.cameraValid;  // a world on screen: not the login, the character list or a loading screen
        m.upscaler->beforePresent(g);
        ID3D12DescriptorHeap* heaps[2] = {m.gpuSrv, m.samplerHeap};  // Streamline may have changed them
        m.list->SetDescriptorHeaps(2, heaps);
        m.boundPipeline = nullptr;
        m.boundTextures = m.boundSamplers = ~0ull;
        m.pass = kPassNone;
    } else if (!m.blit(shown.resource, *shown.state, m.targetSrv, m.width, m.height, from, rtv, fit,
                       int(m.swapWidth), int(m.swapHeight), options.sharpen)) {
        return;
    }
    if (m.ai) {  // a slice of the AI textures' network, in this frame's list, timed so that it keeps to its share
        if (m.timer) m.list->EndQuery(m.timer, D3D12_QUERY_TYPE_TIMESTAMP, m.slot * kTimes + 4);
        m.ai->work();
        if (m.timer) {
            m.list->EndQuery(m.timer, D3D12_QUERY_TYPE_TIMESTAMP, m.slot * kTimes + 5);
            m.slots[m.slot].aiTimed = true;
        }
    }
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
    m.noteGpuFrame();
    if (m.upscaler && m.upscaler->generates()) m.upscaler->afterPresent();  // every frame: the next frame's token
    m.renderStarted = false;
    m.upscaledNow = false;
    m.newPicture();  // 0.19-0.23: the next frame's water, shadows, torches, colours, depth of field
    if (m.upscale) {  // part 3: the next frame - a new jitter, this camera the previous one, a clean layer
        ++m.upscaleFrame;
        m.nextJitter();
        m.prevCameraValid = m.cameraValid;
        std::memcpy(m.prevCamera, m.camera, sizeof m.camera);
        m.frameEnded = true;
    }
    if (m.dropUpscale) m.stopUpscaling();
    if (FAILED(hr))
        m.failed(5, "Present", hr);
    else if (hr == DXGI_STATUS_OCCLUDED)
        Sleep(15);  // the window is covered: Present does not wait for vblank, keep the game's loop from spinning
}

}  // namespace uagfx::gpu
