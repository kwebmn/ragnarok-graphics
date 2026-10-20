#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>

#include "core/d3d7.hpp"
#include "core/foliage.hpp"
#include "core/frame_fit.hpp"
#include "core/full_colour.hpp"
#include "core/png_write.hpp"
#include "core/region_look.hpp"
#include "core/effect_light.hpp"
#include "core/game_camera.hpp"
#include "core/pipeline_key.hpp"
#include "core/scale.hpp"
#include "core/window_plan.hpp"
#include "win/gpu/adapters.hpp"
#include "win/gpu/ai_textures.hpp"
#include "core/pick.hpp"
#include "win/gpu/impl.hpp"
#include "win/gpu/remix9.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/gpu/remote.hpp"
#include "win/dpi.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::gpu {

// 2.4.3: D3D12's device-removed extended data (DRED, Windows 10 1903 and later; the MinGW headers lack it): what
// the runtime knows after a lost device - a page fault's address, the allocations freed lately and those still
// there at it. The two interfaces and the structures as d3d12.h declares them. 2.5.8: outside the anonymous
// namespace - there GCC knows a class's every derived class, finds none implementing these and makes each call of
// their methods a call of a pure virtual function: address 0 here (tools/check_pure_calls.py). Every start of
// Direct3D 12 whose runtime gave DRED's settings crashed in 2.5.0-2.5.7, the 64-bit renderer too.
constexpr int kDredOn = 2;  // D3D12_DRED_ENABLEMENT_FORCED_ON
struct DredAllocationNode {
    const char* nameA;
    const wchar_t* nameW;
    UINT type;  // D3D12_DRED_ALLOCATION_TYPE: 34 a resource, 24 a descriptor heap, 21 a pipeline state...
    const DredAllocationNode* next;
};
struct DredPageFault {
    D3D12_GPU_VIRTUAL_ADDRESS va;
    const DredAllocationNode* existing;
    const DredAllocationNode* freed;
};
struct DredSettings : IUnknown {  // ID3D12DeviceRemovedExtendedDataSettings
    virtual void STDMETHODCALLTYPE SetAutoBreadcrumbsEnablement(int) = 0;
    virtual void STDMETHODCALLTYPE SetPageFaultEnablement(int) = 0;
    virtual void STDMETHODCALLTYPE SetWatsonDumpEnablement(int) = 0;
};
struct DredData : IUnknown {  // ID3D12DeviceRemovedExtendedData
    virtual HRESULT STDMETHODCALLTYPE GetAutoBreadcrumbsOutput(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPageFaultAllocationOutput(DredPageFault*) = 0;
};

namespace {

UINT64 alignUp(UINT64 v, UINT64 a) { return (v + a - 1) & ~(a - 1); }

std::string narrow(const wchar_t* w) {
    char out[512];
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, sizeof out, nullptr, nullptr);
    return n > 0 ? std::string(out) : std::string();
}

const GUID kDredSettingsIid = {0x82BC481C, 0x6B9B, 0x4030, {0xAE, 0xDB, 0x7E, 0xE3, 0xD1, 0xDF, 0x1E, 0x63}};
const GUID kDredDataIid = {0x98931D33, 0x5AE8, 0x4791, {0xAA, 0x3C, 0x1A, 0x73, 0xA2, 0x93, 0x4E, 0x71}};

// The names of an allocation list, up to a dozen: the resource's own (nameResource) or its kind's number.
std::string dredNames(const DredAllocationNode* n) {
    std::string s;
    int count = 0;
    for (; n && count < 12; n = n->next, ++count) {
        if (!s.empty()) s += ", ";
        s += n->nameA ? std::string(n->nameA) : n->nameW ? narrow(n->nameW) : std::string("?");
        char kind[16];
        std::snprintf(kind, sizeof kind, " (%u)", n->type);
        s += kind;
    }
    if (n) s += ", ...";
    return s.empty() ? std::string("none") : s;
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
    char name[64];
    std::snprintf(name, sizeof name, "buffer %llu KB%s", static_cast<unsigned long long>(size >> 10),
                  type == D3D12_HEAP_TYPE_UPLOAD ? " (upload)" : type == D3D12_HEAP_TYPE_READBACK ? " (readback)" : "");
    nameResource(r, name);
    return r;
}

}  // namespace

void nameResource(ID3D12Resource* r, const char* name) {
    if (!r || !name) return;
    wchar_t w[128];
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, w, 128) > 0) r->SetName(w);
}

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
    char name[64];
    std::snprintf(name, sizeof name, "texture %dx%d, %d levels, format %u%s", w, h, levels, unsigned(format),
                  samples > 1 ? ", multisampled" : "");
    nameResource(r, name);
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
    lastTextureKey = ~0ull;  // 2.5: the ring starts anew
    pendingBatch.close();
    pass = kPassNone;
    centered = false;  // a new list starts with the default sample positions
    if (timer) {
        readTimes(s, slot);
        list->EndQuery(timer, D3D12_QUERY_TYPE_TIMESTAMP, slot * kTimes);
    }
    open = true;
    lastCrumb = nullptr;  // 1.9.4: each list its crumbs
    ++listSerial;
    return true;
}

void Renderer::Impl::submit() {
    if (!open) return;
    flushDraws();  // 2.5
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
        if (t[1] > t[0]) {
            gpuTicks += t[1] - t[0];
            recentGpu += t[1] - t[0];
        }
        if (s.upscaleTimed && t[3] > t[2]) {
            upscaleTicks += t[3] - t[2];
            recentUpscale += t[3] - t[2];
        }
        if (s.aiTimed && t[5] >= t[4]) {
            aiTicks += t[5] - t[4];
            if (ai) ai->observe(index, double(t[5] - t[4]) * 1000.0 / double(timerFrequency));
        }
        for (UINT e = 0; e < kEffectTimes; ++e)
            if ((s.effectTimed >> e & 1) && t[7 + 2 * e] > t[6 + 2 * e]) {
                effectTicks[e] += t[7 + 2 * e] - t[6 + 2 * e];
                recentEffect[e] += t[7 + 2 * e] - t[6 + 2 * e];
            }
    }
    s.timed = s.upscaleTimed = s.aiTimed = false;
    s.effectTimed = 0;
}

void Renderer::Impl::effectBegin(EffectTimer e) {
    crumb(passName(e).words);  // the table's own text: crumb() tells the passes apart by the pointer
    flushDraws();  // 2.5: timed from here on, the draws before it done
    if (!timer || !open) return;
    list->EndQuery(timer, D3D12_QUERY_TYPE_TIMESTAMP, slot * kTimes + 6 + 2 * e);
    effectSerial[e] = listSerial;
}

// 1.9.4: a breadcrumb - the pass's number (its name's place in crumbNames) and a count, written by the GPU as it
// starts the pass (word 0) and once all the work before it is done (word 1).
void Renderer::Impl::crumb(const char* what) {
    if (!list2 || !crumbsMapped || !open || what == lastCrumb) return;
    flushDraws();  // 2.5: the pass before this one ends with its draws
    lastCrumb = what;
    uint32_t id = 0;
    while (id < crumbNames.size() && crumbNames[id] != what) ++id;
    if (id == crumbNames.size()) {
        if (crumbNames.size() >= 255) return;
        crumbNames.push_back(what);
    }
    const uint32_t value = (++crumbSeq << 8) | id;
    const D3D12_GPU_VIRTUAL_ADDRESS at = crumbs->GetGPUVirtualAddress();
    D3D12_WRITEBUFFERIMMEDIATE_PARAMETER p[2] = {{at, value}, {at + 4, value}};
    D3D12_WRITEBUFFERIMMEDIATE_MODE modes[2] = {D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_IN,
                                                D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_OUT};
    list2->WriteBufferImmediate(2, p, modes);
}

std::string Renderer::Impl::crumbsReport() const {
    if (!crumbsMapped) return "";
    const uint32_t started = crumbsMapped[0], done = crumbsMapped[1];
    const auto name = [&](uint32_t v) {
        const uint32_t id = v & 255;
        return id < crumbNames.size() ? std::string(crumbNames[id]) : std::string("?");
    };
    char line[256];
    std::snprintf(line, sizeof line, "in \"%s\" (pass %u started; everything before \"%s\", pass %u, done; %u passes "
                  "recorded)", name(started).c_str(), started >> 8, name(done).c_str(), done >> 8, crumbSeq);
    return line;
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
    // 2.2.1: twice a second the frame's work and the timed passes' (effect_costs' names) for the frame counter
    ++recentFrames;
    if (!recentSince) recentSince = now;
    if (now - recentSince >= 500) {
        const double ms = 1000.0 / double(timerFrequency) / double(recentFrames);
        recent.gpuMs = double(recentGpu) * ms;
        recent.upscaleMs = double(recentUpscale) * ms;
        recent.upscaler = upscaler && recentUpscale ? upscaler->name() : "";
        recent.shown = upscaler ? upscaler->shownPerFrame() : 0.0;  // 2.5.24: frame generation's
        recent.reflex = upscaler && upscaler->latencyMarkers();       // 2.5.26
        recent.effects.clear();
        for (UINT e = 0; e < kEffectTimes; ++e) {
            if (recentEffect[e]) recent.effects[passName(e).key] = double(recentEffect[e]) * ms;
            recentEffect[e] = 0;
        }
        ++recent.serial;
        recentGpu = recentUpscale = 0;
        recentFrames = 0;
        recentSince = now;
    }
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
    std::string effects;
    for (UINT e = 0; e < kEffectTimes; ++e) {
        effectMs[e] = double(effectTicks[e]) * ms;  // 0.76: for keep_fps and the settings app
        if (!effectTicks[e]) continue;
        char part[64];
        std::snprintf(part, sizeof part, "%s%s %.2f ms", effects.empty() ? "" : ", ", passName(e).words,
                      double(effectTicks[e]) * ms);
        effects += part;
        effectTicks[e] = 0;
    }
    if (!effects.empty()) LOG_INFO("GPU: effects a frame - %s", effects.c_str());
    LARGE_INTEGER qpf;  // 0.86: and the processor's part
    QueryPerformanceFrequency(&qpf);
    const double perFrame = 1000.0 / double(qpf.QuadPart) / double(std::max<uint32_t>(gpuFrames, 1));
    char reflexPart[48] = "";  // 2.5.26
    if (reflexTicks)
        std::snprintf(reflexPart, sizeof reflexPart, ", %.2f ms Reflex's wait", double(reflexTicks) * perFrame);
    LOG_INFO("CPU: %.2f ms a frame in the driver's drawing (%.2f of it waiting for the card); %.2f ms the frame "
             "limit's wait, %.2f ms the swap chain's present%s", double(cpuTicks) * perFrame,
             double(fenceTicks) * perFrame, double(pacerTicks) * perFrame, double(swapTicks) * perFrame, reflexPart);
    cpuTicks = fenceTicks = pacerTicks = swapTicks = reflexTicks = 0;
    if (drawsGathered)  // 2.5: how many of the game's draws went as one call
        LOG_INFO("draws: %u a frame in %u calls to the GPU", drawsGathered / std::max<uint32_t>(gpuFrames, 1),
                 drawCalls / std::max<uint32_t>(gpuFrames, 1));
    drawsGathered = drawCalls = 0;
    effectMsKnown = true;
    gpuTicks = upscaleTicks = aiTicks = 0;
    gpuFrames = 0;
    gpuSince = now;
}

// 2.5.25: with frame generation the game's frames are held so that the shown ones stay under the refresh of the
// window's monitor (R-Q7RBYV: 4x made 156 a second for a 144 Hz monitor - the extra ones dropped unevenly, and the
// card spent on them the time the game's own frames needed). One present is one frame of the game.
int Renderer::Impl::pacedLimit(int fpsLimit) {
    const int shown = upscaler ? upscaler->shownPerPresent() : 1;
    if (shown <= 1) return fpsLimit;
    const ULONGLONG now = GetTickCount64();
    if (!refreshAsked || now - refreshAsked >= 5000) {
        refreshAsked = now;
        refreshHz = 0;
        MONITORINFOEXW monitor{};
        monitor.cbSize = sizeof monitor;
        DEVMODEW mode{};
        mode.dmSize = sizeof mode;
        if (window &&
            GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                            reinterpret_cast<MONITORINFO*>(&monitor)) &&
            EnumDisplaySettingsW(monitor.szDevice, ENUM_CURRENT_SETTINGS, &mode))
            refreshHz = int(mode.dmDisplayFrequency);
    }
    const int fit = frame_fit::gameFps(refreshHz, shown);
    if (fit != fitLogged) {
        fitLogged = fit;
        if (fit > 0)
            LOG_INFO("frame generation %dx on a %d Hz monitor: the game's frames at most %d a second, %d shown with "
                     "the generated ones - more the monitor would drop unevenly", shown, refreshHz, fit, fit * shown);
        else
            LOG_INFO("frame generation %dx: the monitor's refresh unknown, the game's frames not held for it", shown);
    }
    return frame_fit::pacedFps(fpsLimit, refreshHz, shown);
}

// 2.5.25: whether Windows can flip the swap chain straight to the monitor (independent flip) or composes it with the
// rest of the screen: frame generation paces its frames, and G-SYNC works, only flipped straight - and that takes a
// window covering its monitor pixel for pixel with nothing over it. Windows scaling of the DPI-unaware game makes it a
// pixel short (175 %: 3839x2159 of 3840x2160, R-Q7RBYV). Logged when it changes. 2.5.26: told by the window's place
// - IDXGISwapChainMedia, which tells the mode itself, is not there for a window's swap chain (R-FABEAA).
void Renderer::Impl::notePresentation() {
    const ULONGLONG now = GetTickCount64();
    if (!window || presentSurface || now - presentationAsked < 5000) return;
    presentationAsked = now;
    RECT client{};
    POINT origin{0, 0};
    MONITORINFO monitor{};
    monitor.cbSize = sizeof monitor;
    {
        win::RealPixels real;  // the screen's own pixels, whatever Windows scaling
        if (!GetClientRect(window, &client) || !ClientToScreen(window, &origin) ||
            !GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
            return;
    }
    const RECT on{origin.x, origin.y, origin.x + client.right, origin.y + client.bottom};
    if (EqualRect(&on, &presentationWindow) && EqualRect(&monitor.rcMonitor, &presentationMonitor)) return;
    presentationWindow = on;
    presentationMonitor = monitor.rcMonitor;
    const RECT& m = monitor.rcMonitor;
    if (EqualRect(&on, &m))
        LOG_INFO("presentation: the window covers its monitor (%ldx%ld) - Windows can show the picture straight on "
                 "it while no other window is over the game", m.right - m.left, m.bottom - m.top);
    else
        LOG_INFO("presentation: the window %ldx%ld at %ld,%ld does not cover its monitor's %ldx%ld - Windows "
                 "composes the picture with the desktop, frame generation shows its frames later and less evenly",
                 on.right - on.left, on.bottom - on.top, on.left - m.left, on.top - m.top, m.right - m.left,
                 m.bottom - m.top);
}

// 2.5: the quads' index pattern (core/draw_batch.hpp) in video memory, once, through this list.
bool Renderer::Impl::ensureQuadIndices() {
    if (quadIndices) return true;
    if (quadIndicesFailed || !open) return false;
    const std::vector<uint16_t> pattern = batch::quadIndices(batch::kMaxQuads);
    const UINT64 bytes = pattern.size() * sizeof(uint16_t);
    quadIndices = createBuffer(device, D3D12_HEAP_TYPE_DEFAULT, bytes, D3D12_RESOURCE_STATE_COPY_DEST);
    const Upload u = quadIndices ? allocUpload(bytes, 4) : Upload{nullptr, 0, 0};
    if (!quadIndices || !u.cpu) {
        releaseCom(quadIndices);
        quadIndicesFailed = true;
        LOG_ERROR("draws: no memory for the quads' index pattern: each quad is drawn by itself");
        return false;
    }
    nameResource(quadIndices, "quad index pattern");
    std::memcpy(u.cpu, pattern.data(), size_t(bytes));
    flushDraws();
    list->CopyBufferRegion(quadIndices, 0, slots[slot].upload, u.offset, bytes);
    quadIndicesState = D3D12_RESOURCE_STATE_COPY_DEST;
    transition(quadIndices, quadIndicesState, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    return true;
}

// 2.5: the gathered draw call to the list - its pipeline, tables and constants where they differ from the bound
// ones, the quads through the index pattern, a list as it is, a draw by itself as the game made it.
void Renderer::Impl::flushDraws() {
    if (!pendingBatch.open()) return;
    const PendingDraw& p = pendingDraw;
    if (!boundPipeline) {  // a fresh list or after a pass: the root signature and its tables are gone
        list->SetGraphicsRootSignature(root);
        boundTextures = ~0ull;
        boundSamplers = ~0ull;
    }
    if (p.pso != boundPipeline) {
        list->SetPipelineState(p.pso);
        boundPipeline = p.pso;
    }
    if (p.textureKey != boundTextures) {
        list->SetGraphicsRootDescriptorTable(kRootTextures, p.textures);
        boundTextures = p.textureKey;
    }
    if (p.samplers.ptr != boundSamplers) {
        list->SetGraphicsRootDescriptorTable(kRootSamplers, p.samplers);
        boundSamplers = p.samplers.ptr;
    }
    if (p.hasScene) list->SetGraphicsRootDescriptorTable(kRootScene, p.scene);
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &p.c, 0);
    const batch::Kind kind = pendingBatch.kind();
    list->IASetPrimitiveTopology(kind == batch::Kind::Quads ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST : p.topology);
    const UINT stride = pendingBatch.stride();
    const D3D12_VERTEX_BUFFER_VIEW vb{vertexBase + pendingBatch.firstByte(), pendingBatch.vertices() * stride, stride};
    list->IASetVertexBuffers(0, 1, &vb);
    if (kind == batch::Kind::Quads) {
        const D3D12_INDEX_BUFFER_VIEW ib{quadIndices->GetGPUVirtualAddress(),
                                         UINT(batch::kMaxQuads * 6 * sizeof(uint16_t)), DXGI_FORMAT_R16_UINT};
        list->IASetIndexBuffer(&ib);
        list->DrawIndexedInstanced(pendingBatch.vertices() / 4 * 6, 1, 0, 0, 0);
    } else if (p.indexCount) {
        const D3D12_INDEX_BUFFER_VIEW ib{indexBase, indexBytes, DXGI_FORMAT_R16_UINT};
        list->IASetIndexBuffer(&ib);
        list->DrawIndexedInstanced(p.indexCount, 1, p.firstIndex, 0, 0);
    } else {
        list->DrawInstanced(pendingBatch.vertices(), 1, 0, 0);
    }
    ++drawCalls;
    pendingBatch.close();
}

void Renderer::Impl::waitSlot(Slot& s) {
    if (s.fence && fence->GetCompletedValue() < s.fence) {
        CpuTime waited(fenceTicks);  // 2.1.1
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
        if (!lost) {
            LOG_ERROR("%s: the GPU device was removed (0x%08lx, reason 0x%08lx); the picture stops here, restart the game",
                      what, static_cast<unsigned long>(hr), static_cast<unsigned long>(reason));
            const std::string where = crumbsReport();  // 1.9.4
            if (!where.empty()) LOG_ERROR("the GPU stopped %s", where.c_str());
            dredReport();  // 2.4.3
        }
        lost = true;
        return;
    }
    if (failures.first(place, uint32_t(hr))) LOG_ERROR("%s failed: 0x%08lx", what, static_cast<unsigned long>(hr));
}

// 2.4.3: what D3D12's device-removed data knows of the loss - a page fault (a resource read or written after it was
// freed, or past its end): its address, the allocations freed lately and those still there at it; or no fault at
// all: the GPU stopped on its own - a hang in a shader, or a reset another program caused.
void Renderer::Impl::dredReport() {
    if (!dred || !device) return;
    DredData* data = nullptr;
    if (FAILED(device->QueryInterface(kDredDataIid, reinterpret_cast<void**>(&data))) || !data) {
        LOG_ERROR("DRED: this Windows or driver gives no data of the loss");
        return;
    }
    DredPageFault fault{};
    const HRESULT hr = data->GetPageFaultAllocationOutput(&fault);
    data->Release();
    if (FAILED(hr)) {
        LOG_ERROR("DRED: no page fault data (0x%08lx)", static_cast<unsigned long>(hr));
        return;
    }
    if (!fault.va && !fault.existing && !fault.freed) {
        LOG_ERROR("DRED: no page fault - the GPU stopped on its own (a hang, or a reset another program caused)");
        return;
    }
    LOG_ERROR("DRED: page fault at 0x%016llx; freed lately: %s; there: %s", static_cast<unsigned long long>(fault.va),
              dredNames(fault.freed).c_str(), dredNames(fault.existing).c_str());
}

bool Renderer::Impl::growUpload(Slot& s, UINT64 size) {
    if (s.upload) s.garbage.push_back(s.upload);  // still used by what this slot recorded so far
    s.upload = createBuffer(device, D3D12_HEAP_TYPE_UPLOAD, size, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (s.upload) nameResource(s.upload, (std::string("upload ring of slot ") + std::to_string(&s - slots)).c_str());
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
    flushDraws();  // 2.5: the draws gathered so far go before the barrier
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
    flushDraws();  // 2.5: with the targets they were drawn to
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
    flushDraws();  // 2.5: with their own sample positions
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
    flushDraws();  // 2.5: before the clear
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
    flushDraws();  // 2.5: with the viewport they were drawn in
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
        t.leafy = foliage::leafyTexture(rec.pixels().data() + u.pixels, t.width, t.height,  // 0.64
                                        u.slotCount > 0 || !t.spriteCells.empty());  // 2.6.19: no sprite cache
    }
    const D3D12_TEXTURE_COPY_LOCATION src = stage(*this, rec.pixels().data() + u.pixels, UINT(u.rect.width()),
                                                  UINT(u.rect.height()));
    if (!src.pResource) return;
    if (u.level == 0 && t.mat && t.matFor == t.resource) dropMaterial(t);  // 1.8: new pixels, a new material
    transition(t.resource, t.state, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = t.resource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = u.level;
    list->CopyTextureRegion(&dst, UINT(u.rect.left), UINT(u.rect.top), 0, &src, nullptr);
    if (u.slotCount) noteSpriteCells(t, rec.slots().data() + u.firstSlot, u.slotCount);  // 0.32
    if (ai) {
        std::vector<uint32_t> game;  // 2.5.18: the texels the pack knows it by
        ai->noteUpload(u.texture, t, u, full_colour::aiTexels(u, rec.pixels().data() + u.pixels, game),
                       u.slotCount ? rec.slots().data() + u.firstSlot : nullptr, u.slotCount,  // with their sources
                       rec.pixels().data());  // 2.5.23: a dyed frame's own colours
    }
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
    ID3D12PipelineState* pso = copy       ? copyPipeline
                               : over     ? overPipeline
                               : blitHdr  ? hdrPipeline()  // 0.65
                                          : presentPipelineFor(dstSamples);
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
    if (blitHdr && !copy && !over) c.twin[0] = 200.0f;  // 0.65: white's nits on the HDR10 back buffer
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
    // 1.5.1: the card's own word, when it has stopped (removed, hung) - the reason the rest fails with it.
    LOG_ERROR("cannot create a %dx%d render target with %u samples (the graphics card says 0x%08lx)", w, h, n,
              static_cast<unsigned long>(device->GetDeviceRemovedReason()));
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
    releaseCom(taaHistory);  // 0.75
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
    device->CreateShaderResourceView(motion, nullptr, cpuSrvHandle(motionSrv));  // 0.75: TAA reads it
    if (upscaler->inRenderer()) {  // 0.75: TAA's last result
        taaHistory = createTexture2D(device, targetW, targetH, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE,
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr);
        taaHistoryState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (!taaHistory) {
            LOG_ERROR("cannot create TAA's %dx%d history", targetW, targetH);
            releaseUpscaleTargets();
            return false;
        }
        device->CreateShaderResourceView(taaHistory, nullptr, cpuSrvHandle(taaHistorySrv));
    }
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

// 0.75, TAA: the world blended into the last result along its motion (taa_ps) into the upscaled picture, which is
// then kept as the next frame's history.
bool Renderer::Impl::applyTaa(bool reset, bool masked) {
    if (!taaPs || !taaHistory || !world || !motion || !upscaled) return false;
    if (!taaPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, taaPs, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
        p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&taaPipeline)))) {
            taaPipeline = nullptr;
            releaseCom(taaPs);  // not tried again
            LOG_ERROR("TAA's pipeline failed: the world is shown without it");
            return false;
        }
    }
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    if (!makeRoom(linearClamp, linearClamp, 0, 5)) return false;
    transition(world, worldState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(motion, motionState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(taaHistory, taaHistoryState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (masked) transition(biasMask, biasState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(upscaled, upscaledState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp, 0);
    const D3D12_GPU_DESCRIPTOR_HANDLE textures = textureTable(worldSrv, motionSrv);
    const D3D12_GPU_DESCRIPTOR_HANDLE scene =
        sceneTable(taaHistorySrv, masked ? biasSrv : taaHistorySrv, taaHistorySrv);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(kRtvUpscaled);
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const D3D12_VIEWPORT vp{0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f};
    const D3D12_RECT all{0, 0, width, height};
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &all);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(taaPipeline);
    Constants c{};
    c.targetSize[0] = float(width);
    c.targetSize[1] = float(height);
    c.sourceRect[2] = c.sourceRect[3] = 1.0f;
    c.uiPixels[0] = float(width);
    c.uiPixels[1] = float(height);
    c.twin[0] = reset ? 1.0f : 0.0f;
    c.twin[1] = masked ? 1.0f : 0.0f;
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootTextures, textures);
    list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
    list->SetGraphicsRootDescriptorTable(kRootScene, scene);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    transition(upscaled, upscaledState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    transition(taaHistory, taaHistoryState, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(taaHistory, upscaled);
    transition(taaHistory, taaHistoryState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    static bool logged = false;
    if (!logged) LOG_INFO("TAA: on, %dx%d", width, height);
    logged = true;
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
    if (upscaler && upscaler->passthrough()) {  // 0.62: supersampling - the world where it is
        jitter[0] = jitter[1] = 0.0f;
        return;
    }
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
    flushDraws();  // 2.5
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
    const bool passthrough = upscaler && upscaler->passthrough();  // 0.62: only made smaller
    if (worldDrawn && passthrough) {
        worldValid = true;
        upscaledValid = upscaledNow = false;  // the world shrunk into the picture below, its glow onto that
        worldDrawn = false;
        historyReset = false;
    }
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
        // 0.75: TAA keeps its history while the camera is not known (no motion: it stands still) - else the jitter
        // would show.
        const bool ok =
            upscaler->inRenderer() ? applyTaa(historyReset || jump > 400.0f, masked) : upscaler->evaluate(f);
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
    if (passthrough && glowDrawn && !addGlow(target, targetState, rtvHandle(kRtvTarget), width, height, 1, false))
        return false;  // 0.62: the world's glow onto the shrunk world, under the interface
    if (passthrough) dropGlow();
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
    swapFormat = DXGI_FORMAT_B8G8R8A8_UNORM;  // 0.65: a new one starts as SDR
    hdrOn = hdrDecided = false;
}

// 0.65: HDR10 where it is asked for and the monitor shows HDR (Windows' HDR on): the back buffers become 10 bits a
// channel and the swap chain PQ / BT.2020; else (again) the 8-bit sRGB ones. Not with frame generation or a
// composition surface.
void Renderer::Impl::decideHdr() {
    hdrDecided = true;
    if (!swapChain) return;
    bool can = false;
    if (hdrWanted && !presentSurface && !(upscaler && upscaler->generates())) {
        IDXGIOutput* out = nullptr;
        IDXGIOutput6* out6 = nullptr;
        DXGI_OUTPUT_DESC1 d{};
        if (SUCCEEDED(swapChain->GetContainingOutput(&out)) &&
            SUCCEEDED(out->QueryInterface(__uuidof(IDXGIOutput6), reinterpret_cast<void**>(&out6))) &&
            SUCCEEDED(out6->GetDesc1(&d)))
            can = d.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
        releaseCom(out6);
        releaseCom(out);
        UINT support = 0;
        can = can && SUCCEEDED(swapChain->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,
                                                                 &support)) &&
              (support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
        if (!can) LOG_INFO("HDR: the monitor does not show HDR now (Windows' HDR off?) - the picture stays SDR");
    }
    const DXGI_FORMAT want = can ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
    if (want != swapFormat) {
        waitIdle();
        for (ID3D12Resource*& b : backBuffers) releaseCom(b);
        const UINT flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        if (FAILED(swapChain->ResizeBuffers(2, swapWidth, swapHeight, want, flags))) {
            LOG_ERROR("HDR: the swap chain would not change its format");
            releaseSwapChain();
            return;
        }
        for (UINT i = 0; i < 2; ++i) {
            if (FAILED(swapChain->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&backBuffers[i])))) {
                releaseSwapChain();
                return;
            }
            device->CreateRenderTargetView(backBuffers[i], nullptr, rtvHandle(1 + i));
            backStates[i] = D3D12_RESOURCE_STATE_PRESENT;
        }
        swapFormat = want;
    }
    swapChain->SetColorSpace1(can ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
                                  : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
    if (can != hdrOn)
        LOG_INFO("HDR: %s", can ? "on - HDR10, white at 200 nits, the brightest a little over it" : "off");
    hdrOn = can;
}

bool Renderer::Impl::ensureSwapChain(HWND hwnd, UINT w, UINT h, bool wantTearing) {
    if (swapChain && hwnd == window && w == swapWidth && h == swapHeight) {
        if (!hdrDecided) decideHdr();  // 0.65
        return swapChain != nullptr;
    }
    if (!swapChain && w == failedWidth && h == failedHeight && !swapRetry.mayTry(GetTickCount64())) return false;
    waitIdle();
    const UINT flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    if (swapChain && hwnd == window) {  // the window changed size
        for (ID3D12Resource*& b : backBuffers) releaseCom(b);
        const HRESULT hr = swapChain->ResizeBuffers(2, w, h, swapFormat, flags);  // 0.65: HDR keeps its format
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
    if (!hdrDecided) decideHdr();  // 0.65
    return swapChain != nullptr;
}

void Renderer::Impl::destroy() {
    if (device && fence) waitIdle();
    delete ai;  // its last results go into the texture pack
    ai = nullptr;
    releaseSwapChain();
    for (auto& t : textures) {
        releaseCom(t.second.resource);
        releaseCom(t.second.ai);
        releaseCom(t.second.mat);  // 1.8
    }
    textures.clear();
    for (auto& p : pipelines) releaseCom(p.second);
    pipelines.clear();
    for (ID3D12PipelineState*& p : presentPipelines) releaseCom(p);
    releaseCom(presentHdrPipeline);  // 0.65
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
    releaseCom(drawLeanPs);  // 2.5
    releaseCom(drawGlowLeanPs);
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
    for (ID3D12PipelineState*& p : nightPipelines) releaseCom(p);  // 0.63
    releaseCom(nightPs);
    releaseCom(gradePs);
    releaseCom(maskPipeline);  // 0.22.3
    releaseCom(maskPs);
    releaseCom(taaPipeline);  // 0.75
    releaseCom(taaPs);
    for (ID3D12PipelineState*& p : torchLightPipelines) releaseCom(p);  // 0.23
    for (ID3D12PipelineState*& p : torchFirePipelines) releaseCom(p);
    for (ID3D12PipelineState*& p : fireflyPipelines) releaseCom(p);  // 0.28
    for (ID3D12PipelineState*& p : dustPipelines) releaseCom(p);      // 0.29
    releaseCom(torchLightVs);  // 2.5
    releaseCom(torchAccumPs);
    releaseCom(torchApplyPs);
    releaseCom(torchAccumPipeline);
    releaseCom(torchLight);
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
    for (ID3D12PipelineState*& p : mistPipelines) releaseCom(p);  // 0.34
    releaseCom(mistPs);
    for (ID3D12PipelineState*& p : skyPipelines) releaseCom(p);  // 0.69
    releaseCom(floorReflectPipeline);  // 0.72, 0.86
    for (ID3D12PipelineState*& p : halfOverPipelines) releaseCom(p);
    releaseCom(halfOverPs);
    releaseCom(floorTarget);
    releaseCom(shaftsHalfPipeline);  // 0.73; 2.5.7 at half the size
    for (ID3D12PipelineState*& p : halfAddPipelines) releaseCom(p);
    releaseCom(shaftsTarget);
    releaseCom(shaftsPs);
    releaseCom(flarePs);  // 0.98
    for (ID3D12PipelineState*& f : flarePipelines) releaseCom(f);
    for (ID3D12PipelineState*& p : embersPipelines) releaseCom(p);  // 0.82
    releaseCom(embersPs);
    releaseCom(embersData);
    for (ID3D12PipelineState*& p : grassPipelines) releaseCom(p);  // 0.81
    for (ID3D12PipelineState*& p : groundLifePipelines) releaseCom(p);  // 1.7
    for (ID3D12PipelineState*& p : crittersPipelines) releaseCom(p);
    releaseCom(materialPso);  // 1.8
    releaseCom(materialPs);
    releaseCom(crittersPs);
    releaseCom(crittersVs);
    releaseCom(crittersData);
    releaseCom(hillsData);  // 1.15
    releaseCom(groundCellsData);  // 2.2.1
    releaseCom(mirror);  // 1.16
    releaseCom(mirrorDepth);
    releaseCom(mirrorPso);
    releaseCom(mirrorVs);
    releaseCom(mirrorPs);
    releaseCom(groundLifePs);
    releaseCom(grassPs);
    for (ID3D12PipelineState*& p : stepsPipelines) releaseCom(p);  // 0.80
    releaseCom(stepsPs);
    releaseCom(stepsData);
    releaseCom(trampleData);  // 2.6.8
    releaseBloom();  // 0.78
    releaseCom(bloomDownPipeline);
    releaseCom(bloomBlurPipeline);
    for (ID3D12PipelineState*& p : bloomApplyPipelines) releaseCom(p);
    releaseCom(bloomDownPs);
    releaseCom(bloomApplyPs);
    releaseCom(floorReflectPs);
    releaseCom(skyPs);
    for (ID3D12PipelineState*& p : weatherPipelines) releaseCom(p);  // 0.39
    for (ID3D12PipelineState*& p : splashPipelines) releaseCom(p);  // 0.64
    for (ID3D12PipelineState*& p : waterfallPipelines) releaseCom(p);  // 0.65
    releaseCom(waterfallPs);
    releaseCom(splashPs);
    releaseCom(weatherPs);
    for (ID3D12PipelineState*& p : heatPipelines) releaseCom(p);  // 0.35
    releaseCom(heatPs);
    releaseCom(heatData);
    releaseCom(torchData);
    releaseCom(quadIndices);  // 2.5
    releaseCom(smokePs);  // 0.24
    releaseCom(smokeGlowPs);
    releaseCom(dofBlurPs);
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
    releaseCom(shotBuffer);
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
    releaseCom(list2);  // 1.9.4
    crumbsMapped = nullptr;
    releaseCom(crumbs);
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
    delete d3d11_;
    impl_->destroy();
    delete impl_;
    delete supersample_;  // 0.62: after the renderer that used it
    delete taa_;          // 0.75
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

bool Renderer::createD3D11(const std::string& shaderCache, std::string& error) {
    if (d3d11_) return true;
    d3d11_ = new Renderer11;
    if (d3d11_->create(shaderCache, impl_->startSink, impl_->startContext, error)) return true;
    delete d3d11_;
    d3d11_ = nullptr;
    return false;
}

void Renderer::setWindow(HWND window) {
    if (remix_) remix_->setWindow(window);
}

std::string Renderer::adapterName() const {
    return remote_ ? remote_->adapterName()
           : remix_ ? remix_->adapterName()
           : d3d11_ ? d3d11_->adapterName()
                    : impl_->adapter;
}

void Renderer::setMergeDraws(bool on) {
    if (d3d11_) d3d11_->setMergeDraws(on);
    if (impl_) impl_->mergeDraws = on;  // 2.5: Direct3D 12 gathers them too
}

void Renderer::setStateCache(bool on) {
    if (d3d11_) d3d11_->setStateCache(on);
}

void Renderer::setUiSharp(bool sharp) {
    if (remix_) return;
    if (d3d11_) return d3d11_->setUiSharp(sharp);
    if (remote_) return remote_->setUiSharp(sharp);
    impl_->uiSharp = sharp;
}

bool Renderer::upscales() const { return remote_ && remote_->upscales(); }

int Renderer::samples() const {
    return remix_ || d3d11_ ? 1 : remote_ ? remote_->samples() : int(impl_->samples);
}

void Renderer::setUpscaler(Upscaler* u) { impl_->upscaler = u; }

bool Renderer::setSupersample(double ratio) {
    if (remote_ || d3d11_ || remix_) return false;  // 0.62: Direct3D 12 in the game's process only
    if (ratio <= 1.0) {
        if (supersample_ && impl_->upscaler == supersample_) impl_->upscaler = nullptr;
        return false;
    }
    if (!supersample_) supersample_ = new Supersample();
    supersample_->setRatio(ratio);
    impl_->upscaler = supersample_;
    return true;
}

bool Renderer::effectCosts(effect_costs::Costs& out) const {
    out.clear();  // 0.76: by the names effect_costs.cpp knows the passes by (passName)
    if (remote_) {  // 2.2.1: the 64-bit renderer's, as it sends them (the last half second's)
        GpuTiming t;
        if (!remote_->timing(t) || t.effects.empty()) return false;
        out = t.effects;
        return true;
    }
    if (d3d11_) {  // 2.5.9: Direct3D 11's own passes as it times them (the last half second's): keep_fps had none
        GpuTiming t;
        if (!d3d11_->timing(t) || t.effects.empty()) return false;
        out = t.effects;
        return true;
    }
    if (remix_ || !impl_->effectMsKnown) return false;
    for (UINT e = 0; e < kEffectTimes; ++e)
        if (impl_->effectMs[e] > 0.0) out[passName(e).key] = impl_->effectMs[e];
    return true;
}

bool Renderer::gpuTiming(GpuTiming& out) const {
    if (remote_) return remote_->timing(out);  // the 64-bit renderer's own count of safe shaders with it
    if (d3d11_) {
        const bool known = d3d11_->timing(out);
        out.safeShaders = safeShaderCount();  // 2.2.2
        return known;
    }
    if (remix_ || !impl_) return false;
    out = impl_->recent;
    out.safeShaders = safeShaderCount();
    return out.gpuMs >= 0.0;
}

bool Renderer::setTaa(bool on) {
    if (remote_ || d3d11_ || remix_) return false;  // 0.75: Direct3D 12 in the game's process only
    if (!on) {
        if (taa_ && impl_->upscaler == taa_) impl_->upscaler = nullptr;
        return false;
    }
    if (!taa_) taa_ = new Taa();
    impl_->upscaler = taa_;
    return true;
}

void Renderer::setApi(HMODULE module) { impl_->api = module; }

void Renderer::setOnDevice(void (*fn)(void* context, ID3D12Device* device), void* context) {
    impl_->onDevice = fn;
    impl_->onDeviceContext = context;
}

bool Renderer::upgradeFactory(bool (*upgrade)(void** object)) {
    Impl& m = *impl_;
    if (remote_ || remix_ || d3d11_ || !m.factory || !upgrade) return false;
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
    if (d3d11_) return d3d11_->startAi(gameDir, settings);  // 0.47: the ready pictures alone
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

bool Renderer::startAiForPack(const std::string& gameDir, const AiSettings& settings, std::string& error) {
    Impl& m = *impl_;
    if (remote_ || remix_ || m.ai || !m.device) {
        error = "the renderer is not for it";
        return false;
    }
    m.ai = new AiTextures(m);
    if (!m.ai->startForPack(gameDir, settings, error)) {
        delete m.ai;
        m.ai = nullptr;
        return false;
    }
    return true;
}

bool Renderer::enlargeForPack(int group, const uint32_t* argb, int width, int height, uint32_t border,
                              std::vector<uint32_t>& out, std::string& error) {
    Impl& m = *impl_;
    if (!m.ai) {
        error = "no networks";
        return false;
    }
    return m.ai->enlarge(m.ai->netForPicture(ai::Group(group), width, height), argb, width, height, border, out,
                         error);  // 1.6, 1.6.3
}

bool Renderer::startAiForStudio(const std::string& gameDir, std::string& error) {
    Impl& m = *impl_;
    if (remote_ || remix_ || m.ai || !m.device) {
        error = "the renderer is not for it";
        return false;
    }
    m.ai = new AiTextures(m);
    if (!m.ai->startForStudio(gameDir, error)) {
        delete m.ai;
        m.ai = nullptr;
        return false;
    }
    return true;
}

bool Renderer::enlargeByModel(const std::string& model, int denoise, const uint32_t* argb, int width, int height,
                              uint32_t border, std::vector<uint32_t>& out, std::string& error) {
    Impl& m = *impl_;
    if (!m.ai) {
        error = "no networks";
        return false;
    }
    const int net = m.ai->netForModel(model, denoise, error);
    return net >= 0 && m.ai->enlarge(net, argb, width, height, border, out, error);
}

std::string Renderer::packNetName(int group) const {
    const Impl& m = *impl_;
    const int net = m.ai ? m.ai->netForGroup(ai::Group(group)) : -1;
    return net < 0 ? std::string() : m.ai->netName(net);
}

void Renderer::finish() {
    if (d3d11_) return d3d11_->finish();
    if (!remote_ && impl_->device && impl_->fence) impl_->waitIdle();
}

void Renderer::requestIcon(uint64_t key, int width, int height, const uint32_t* argb) {
    if (d3d11_) return d3d11_->requestIcon(key, width, height, argb);
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
    if (d3d11_) d3d11_->setIconSink(sink, context);
    if (remote_) remote_->setIconSink(sink, context);
}

void Renderer::setMap(const std::string& map) {
    if (remix_) return;
    if (d3d11_) return d3d11_->setMap(map);  // 0.79: the region's look
    if (remote_) return remote_->setMap(map);
    impl_->mapWorld.setMap(map);
    impl_->mapName = map;  // 0.31: lava's maps
    impl_->mapSeason = trees::seasonOf(map, trees::monthNow());  // 1.7
    impl_->mapPlace = living::placeOf(map, outdoors(map), impl_->mapSeason == trees::Season::Autumn);
    impl_->mapYear = seasons::of(map, trees::monthNow(), outdoors(map), impl_->mapPlace);  // 1.20
    impl_->mapOutdoors = outdoors(map);  // 0.33: the open sky's effects
    impl_->torchKeeper.clear();  // 2.6.17: a new map's torches
    impl_->mapShine = floorShine(map);   // 2.5.17: the floor reflections on smooth floors alone
    impl_->mapDesert = desert(map);      // 0.35: its heat haze far off
    impl_->mapWeather = weather::of(map);  // 0.39: its weather
    impl_->footTracker.clear();  // 0.80
    impl_->waterLevelKnown = false;  // 1.7
    impl_->critterWorld.setPlace(impl_->mapPlace);
    impl_->crittersTime = 0.0f;
    if (!map.empty()) impl_->mapFadeStart = impl_->waterTime();  // 0.83: the world comes in from black
    impl_->mapRegion = region_look::of(map, 100).region[0] != 0;  // 0.79: its look
    if (impl_->mapRegion) LOG_INFO("region: %s (%s)", region_look::of(map, 100).region, map.c_str());
    impl_->weatherLogged = false;
    if (impl_->ai) impl_->ai->setMap(map);
}

void Renderer::setShaderCache(const std::string& file) { impl_->shaderCacheFile = file; }

void Renderer::setGameDir(const std::string& dir) {
    if (d3d11_) return d3d11_->setGameDir(dir);  // 2.5.1: the map files, for the lamps
    if (remix_ || remote_) return;  // uaro-render64.exe is given it at its start
    impl_->mapWorld.setGameDir(dir);
}

void Renderer::setCamera(bool valid, const float view[12], float targetZ) {
    if (remix_) return remix_->setCamera(valid, view);
    if (d3d11_) return d3d11_->setCamera(valid, view);  // 0.99: the modern water
    if (remote_) return remote_->setCamera(valid, view, targetZ);
    Impl& m = *impl_;
    m.cameraValid = valid;
    m.cameraTargetZ = valid ? targetZ : 0.0f;
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
    // uaro-render64.exe's and (2.5.13) Direct3D 11's own thread's: 2.5.12 made none there - the screen was white
    if (d3d11_) return d3d11_->createTextureAs(id, width, height, levels);
    if (remix_ || remote_) return false;
    Impl& m = *impl_;
    if (!id || m.textures.count(id)) return false;
    const uint32_t made = createTexture(width, height, levels);
    if (!made) return false;
    Texture t = m.textures[made];
    m.textures.erase(made);
    m.textures.emplace(id, t);
    return true;
}

void Renderer::setShaderVariants(bool on) {
    if (d3d11_) d3d11_->setShaderVariants(on);  // Direct3D 11's own thread is given it at its start
}

void Renderer::setLight(uint8_t flags) {
    if (d3d11_) return d3d11_->setLight(flags);
    if (remote_) return remote_->setLight(flags);  // Direct3D 11's own thread; uaro-render64.exe leaves them
}

void Renderer::setWorldScale(double share) {
    if (d3d11_) return d3d11_->setWorldScale(share);
    if (remote_) return remote_->setWorldScale(share);  // Direct3D 11's own thread; uaro-render64.exe leaves it
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

bool Renderer::lost() const { return d3d11_ ? d3d11_->lost() : impl_->lost; }

void Renderer::setStudio(uint8_t flags, int x, int y) {
    if (remix_ || d3d11_) return;
    if (remote_) return remote_->setStudio(flags, x, y);
    Impl& m = *impl_;
    if (m.xray != ((flags & 1) != 0)) LOG_INFO("X-ray: %s", (flags & 1) ? "on" : "off");
    m.xray = (flags & 1) != 0;
    if (flags & 2) m.pick.ask(float(x) + 0.5f, float(y) + 0.5f, GetTickCount64());  // what the next frame draws there
    if (flags & 4) m.abWanted = true;
}

void Renderer::setDrawQuality(int anisotropy, bool alphaToCoverage, uint8_t aiKinds, uint8_t aiSprites) {
    if (remix_) return;
    if (d3d11_) {
        d3d11_->setAnisotropy(anisotropy);
        return d3d11_->setAiShown(aiKinds, aiSprites);
    }
    if (remote_) return remote_->setDrawQuality(anisotropy, alphaToCoverage, aiKinds, aiSprites);
    Impl& m = *impl_;
    if (m.ai) {
        m.ai->shown = aiKinds != 0;
        m.ai->kindsShown = aiKinds;
        m.ai->spritesShown = aiSprites;
    }
    if (anisotropy == m.anisotropy && alphaToCoverage == m.alphaToCoverage) return;
    m.anisotropy = anisotropy;
    m.alphaToCoverage = alphaToCoverage;
    LOG_INFO("quality: anisotropy %d, alpha to coverage %s", anisotropy, alphaToCoverage ? "on" : "off");
}

void Renderer::setEffects(const EffectSettings& e) {
    if (d3d11_) return d3d11_->setEffects(e);  // 0.59: the colours
    if (remix_) return;
    if (remote_) return remote_->setEffects(e);
    Impl& m = *impl_;
    if (e == m.effects) return;
    const bool was = m.glowWanted();
    m.effects = e;
    LOG_INFO("effects: smooth map light %s; torches %s, new flame %s, brightness %d%%, glow %d%%, light %d%%, flicker "
             "%d%%; chimney smoke %s, density %d%%; lamps %s, light %d%%; fire of skills lights %s, %d%%, "
             "the others %s; "
             "fireflies %s, %d%%; hidden lights %s, %d%%; forest beams %s, %d%%; lava %s, %d%%; "
             "characters' shadows %s, %d%%; heat haze %s, %d%%; portals %s, %d%%; "
             "weather %s, %d%%; shadows %s, "
             "strength %d%%, size %d%%; glow %s, strength %d%%, size %d%%, light %d%%; "
             "water %s, waves %d%%, reflections %d%%, clarity %d%%; colour %s, saturation %d%%, contrast %d%%, "
             "brightness %d%%, warmth %d%%, vignette %d%%",  // 2.6.20: the depth of field's places gone with it
             e.smoothLight ? "on" : "off", e.torch ? "on" : "off", e.torchFlame ? "on" : "off", e.torchBrightness,
             e.torchGlow, e.torchLight, e.torchFlicker, e.smoke ? "on" : "off", e.smokeDensity, e.lamps ? "on" : "off",
             e.lampsLight, e.fireLight ? "on" : "off", e.fireLightStrength, e.skillColours ? "on" : "off",
             e.fireflies ? "on" : "off",
             e.firefliesBrightness, e.mapGlows ? "on" : "off", e.mapGlowsBrightness, e.rays ? "on" : "off",
             e.raysBrightness, e.lava ? "on" : "off", e.lavaBrightness, e.actorShadows ? "on" : "off",
             e.actorShadowsStrength,
             e.heat ? "on" : "off", e.heatStrength, e.portals ? "on" : "off", e.portalsStrength,
             e.weather ? "on" : "off", e.weatherAmount,
             e.ao ? "on" : "off", e.aoStrength, e.aoSize, e.glow ? "on" : "off",
             e.glowStrength, e.glowSize,
             e.glowLight, e.water ? "on" : "off", e.waterWaves, e.waterReflections, e.waterClarity,
             e.grade ? "on" : "off", e.gradeSaturation, e.gradeContrast, e.gradeBrightness, e.gradeWarmth,
             e.gradeVignette);
    // 1.9.4: an effect turned off keeps its targets (until the size changes or the game ends): with the console
    // switching effects in a running game, making them anew a moment later failed ("no video memory") and the device
    // was lost soon after (1.9.3, reports R-HAAN9H, R-AQ6PVP). Only the glow's state is dropped for this frame.
    if (was && !m.glowWanted()) m.dropGlow();
}

void Renderer::preparePipelines(const std::string& file) {
    if (remix_ || d3d11_) return;
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
    // 2.4.3: D3D12's device-removed data (DRED): page faults tracked always - they cost nothing until a loss, and
    // then the log names the address and the resource (failed(): dredReport). Asked of the system's d3d12.dll: with
    // Streamline's interposer in m.d3d12 the setting still reaches the runtime underneath.
    using GetDebugFn = HRESULT(WINAPI*)(REFIID, void**);
    const HMODULE system = m.api ? LoadLibraryA("d3d12.dll") : m.d3d12;
    const auto getDebug = system ? reinterpret_cast<GetDebugFn>(
                                       reinterpret_cast<void*>(GetProcAddress(system, "D3D12GetDebugInterface")))
                                 : nullptr;
    if (getDebug) {
        DredSettings* dred = nullptr;
        if (SUCCEEDED(getDebug(kDredSettingsIid, reinterpret_cast<void**>(&dred))) && dred) {
            dred->SetPageFaultEnablement(kDredOn);
            dred->Release();
            m.dred = true;
        }
    }
    // 0.50.1: the fast card of several (a laptop's NVIDIA, not its integrated Intel); no WARP, no Basic Render.
    std::vector<AdapterFacts> all;
    std::vector<IDXGIAdapter1*> adapters = orderedAdapters(m.factory, &all);
    for (IDXGIAdapter1* adapter : adapters) {
        if (m.device) break;
        DXGI_ADAPTER_DESC1 d{};
        adapter->GetDesc1(&d);
        if (SUCCEEDED(createDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                   reinterpret_cast<void**>(&m.device))))
        {
            m.adapter = narrow(d.Description);
            m.adapterMb = uint64_t(d.DedicatedVideoMemory) >> 20;
        }
    }
    releaseAdapters(adapters);
    if (all.size() > 1)
        LOG_INFO("graphics cards: %s - Direct3D 12 on %s", adaptersText(all).c_str(), m.adapter.c_str());
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
    // 1.9.4: the breadcrumbs (crumb())
    if (SUCCEEDED(m.list->QueryInterface(__uuidof(ID3D12GraphicsCommandList2), reinterpret_cast<void**>(&m.list2)))) {
        m.crumbs = createBuffer(m.device, D3D12_HEAP_TYPE_READBACK, 256, D3D12_RESOURCE_STATE_COPY_DEST);
        void* p = nullptr;
        if (!m.crumbs || FAILED(m.crumbs->Map(0, nullptr, &p))) {
            releaseCom(m.crumbs);
            releaseCom(m.list2);
        } else {
            m.crumbsMapped = static_cast<volatile uint32_t*>(p);
        }
    } else {
        m.list2 = nullptr;
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
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 4, false, m.dsvHeap)) {  // 1.16: kDsvMirror
        error = "descriptor heaps";
        return false;
    }
    m.srvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m.samplerStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    m.rtvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m.dsvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    // 2.5.10: slot 0 an empty view (reads 0) of no resource, never handed out. A slot index left at its default 0
    // pointed at the target's view: the torches' light buffer (torchLightSrv, 2.5.0) made its view there at its
    // first light - the frame shown and read was that buffer (black), or once the target's view was made again
    // (Scroll Lock) the light read the picture (a bright square about a skill). A forgotten index costs its own
    // effect now, never the picture.
    m.allocSrv();
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC none{};
        none.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        none.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        none.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        none.Texture2D.MipLevels = 1;
        m.device->CreateShaderResourceView(nullptr, &none, m.cpuSrvHandle(0));
    }
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
    m.biasSrv = m.allocSrv();  // 0.22.3: DLSS's mask
    m.motionSrv = m.allocSrv();  // 0.75: TAA's
    m.taaHistorySrv = m.allocSrv();
    for (UINT& srv : m.bloomSrv) srv = m.allocSrv();  // 0.78
    m.floorSrv = m.allocSrv();  // 0.86
    m.shaftsSrv = m.allocSrv();  // 2.5.7
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
    if (d3d11_) return d3d11_->setTarget(width, height);
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
    if (d3d11_) return d3d11_->setScale(k, targetWidth, targetHeight);  // one sample, no DLSS
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
    if (d3d11_) return d3d11_->createTexture(width, height, levels);
    if (remote_) return remote_->createTexture(width, height, levels);
    Impl& m = *impl_;
    Texture t;
    t.width = width;
    t.height = height;
    t.levels = std::max(1, levels);
    t.srv = m.allocSrv();
    if (t.srv == kNoSrv) {  // never hand out a slot that is in use
        LOG_ERROR("no texture descriptor left for a %dx%d texture (%u textures alive)", width, height,
                  static_cast<unsigned>(m.textures.size()));
        return 0;
    }
    t.resource = createTexture2D(m.device, width, height, t.levels, DXGI_FORMAT_B8G8R8A8_UNORM,
                                 D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
    if (!t.resource) {
        m.freeSrv.push_back(t.srv);
        // 2.4.3: a lost device makes nothing - its reason and the GPU's last pass to the log now, not at the next
        // read of the frame (R-TT5RVW: "not made" six seconds before the loss was told)
        const HRESULT reason = m.device->GetDeviceRemovedReason();
        if (FAILED(reason)) m.failed(7, "making a texture", reason);
        return 0;
    }
    m.device->CreateShaderResourceView(t.resource, nullptr, m.cpuSrvHandle(t.srv));
    const uint32_t id = m.nextTexture++;
    char name[48];
    std::snprintf(name, sizeof name, "game texture %u, %dx%d", id, width, height);
    nameResource(t.resource, name);
    m.textures.emplace(id, t);
    return id;
}

void Renderer::noteObjectTexture(uint32_t id, uint8_t kind) {
    if (d3d11_) return d3d11_->noteObjectTexture(id, kind);  // 0.61: the torches' light
    if (remix_) return;
    if (remote_) return remote_->noteObjectTexture(id, kind);
    impl_->objectTextures.note(id, objects::Kind(kind));
}

void Renderer::releaseTexture(uint32_t id) {
    if (remix_) return remix_->releaseTexture(id);
    if (d3d11_) return d3d11_->releaseTexture(id);
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
        if (it->second.mat) {  // 1.8
            s.garbage.push_back(it->second.mat);
            freeSrv.push_back(it->second.matSrv);
        }
        if (ai) ai->forget(id);
        objectTextures.forget(id);  // 0.23.1
        textures.erase(it);
    }
    dying.clear();
}

void Renderer::execute(const FrameRecord& record) {
    if (remix_) return remix_->execute(record);
    if (d3d11_) return d3d11_->execute(record);
    if (remote_) return remote_->execute(record);
    Impl& m = *impl_;
    Impl::CpuTime cpu(m.cpuTicks);  // 0.86
    if (m.lost || !m.target || record.empty() || !m.begin()) return;
    if (m.upscale && m.upscaler && !m.renderStarted) {  // part 3: latency markers
        m.renderStarted = true;
        m.upscaler->renderStart();
    }
    m.prepareGeometry(record);
    if (m.pick.looking() && m.ai) {  // 2.3: the AI textures under the cursor, the one on top first (2.3.1: all of them)
        std::vector<uint32_t> opaque, blended;
        for (int k : pick::drawsAt(record, m.pick.x(), m.pick.y(), [&m](const DrawCmd& d) {
                 uint8_t how = 0;
                 return d.texture[0] && m.ai->howOf(d.texture[0], how);
             })) {
            const DrawCmd& d = record.draws()[size_t(k)];
            (record.states()[d.state].blend ? blended : opaque).push_back(d.texture[0]);
        }
        m.pick.seen(opaque, blended);
    }
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
    if (d3d11_) return d3d11_->readTarget(argb);
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
    if (d3d11_) return d3d11_->present(window, outputWidth, outputHeight, source, options);
    if (remote_) return remote_->present(window, outputWidth, outputHeight, source, options);
    Impl& m = *impl_;
    Impl::CpuTime presentCpu(m.cpuTicks);  // 0.86
    uint32_t picked = 0;  // 2.3, uaRO Studio: the frame after the key is whole - what it drew under the cursor
    if (m.pick.due(picked) && m.ai) m.ai->writePick(picked, int(m.pick.x()), int(m.pick.y()));
    if (m.abWanted) {
        m.abWanted = false;
        if (m.ai) m.ai->toggleHidden();
    }
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
    if (options.hdr != m.hdrWanted) {  // 0.65: asked for or not any more
        m.hdrWanted = options.hdr;
        m.hdrDecided = false;
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
    } else {
        m.blitHdr = m.hdrOn;  // 0.65: onto the HDR10 back buffer
        const bool ok = m.blit(shown.resource, *shown.state, m.targetSrv, m.width, m.height, from, rtv, fit,
                               int(m.swapWidth), int(m.swapHeight), options.sharpen);
        m.blitHdr = false;
        if (!ok) return;
    }
    if (m.ai) {  // a slice of the AI textures' network, in this frame's list, timed so that it keeps to its share
        if (m.timer) m.list->EndQuery(m.timer, D3D12_QUERY_TYPE_TIMESTAMP, m.slot * kTimes + 4);
        m.ai->work(options.foreground);
        if (m.timer) {
            m.list->EndQuery(m.timer, D3D12_QUERY_TYPE_TIMESTAMP, m.slot * kTimes + 5);
            m.slots[m.slot].aiTimed = true;
        }
    }
    if (!options.screenshot.empty() && m.hdrOn)  // 0.65: a PNG has no HDR10's colours
        LOG_INFO("screenshot: not taken with HDR on - turn HDR off for it ([display] hdr)");
    if (!options.screenshot.empty() && !m.hdrOn) {  // 0.54: the picture as shown, read after Present
        const UINT pitch = UINT(alignUp(UINT64(m.swapWidth) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
        const UINT64 size = UINT64(pitch) * m.swapHeight;
        if (m.shotSize < size) {
            releaseCom(m.shotBuffer);
            m.shotBuffer = createBuffer(m.device, D3D12_HEAP_TYPE_READBACK, size, D3D12_RESOURCE_STATE_COPY_DEST);
            m.shotSize = m.shotBuffer ? size : 0;
        }
        if (m.shotBuffer) {
            m.transition(m.backBuffers[index], m.backStates[index], D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
            dst.pResource = m.shotBuffer;
            dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            dst.PlacedFootprint.Footprint.Width = m.swapWidth;
            dst.PlacedFootprint.Footprint.Height = m.swapHeight;
            dst.PlacedFootprint.Footprint.Depth = 1;
            dst.PlacedFootprint.Footprint.RowPitch = pitch;
            src.pResource = m.backBuffers[index];
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            m.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            m.shotPath = options.screenshot;
            m.shotPitch = pitch;
            m.shotWidth = int(m.swapWidth);
            m.shotHeight = int(m.swapHeight);
        }
    }
    m.transition(m.backBuffers[index], m.backStates[index], D3D12_RESOURCE_STATE_PRESENT);
    m.submit();

    presentCpu.stop();  // 0.86: not the waits
    // 2.5.26: with Reflex the game draws its next frame only after this present (Remote::present), so without frame
    // generation the frame limit waits after the present: the game takes the mouse after the wait instead of the
    // frame being shown a wait late. Frame generation's waits stay before it - its frames paced by the presents.
    const bool reflex = m.upscaler && m.upscaler->latencyMarkers();
    const bool paceAfter = reflex && m.upscaler->shownPerPresent() <= 1;
    if (!paceAfter) {
        Impl::CpuTime paced(m.pacerTicks);  // 2.1.1
        m.pacer.wait(m.pacedLimit(options.fpsLimit), options.pacerSleeps);  // 0.85: kept to the frame
    }
    if (reflex) m.upscaler->presentStart();  // 2.5.26: Reflex's markers right before the present
    const UINT sync = options.vsync ? 1 : 0;
    const UINT flags = !options.vsync && options.allowTearing && m.tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
    Impl::CpuTime swapped(m.swapTicks);  // 2.1.1
    const HRESULT hr = m.swapChain->Present(sync, flags);
    swapped.stop();
    if (!m.shotPath.empty()) {  // 0.54: the copy done, the PNG written
        m.waitIdle();
        uint8_t* data = nullptr;
        D3D12_RANGE all{0, SIZE_T(m.shotSize)};
        bool ok = false;
        if (!m.lost && SUCCEEDED(m.shotBuffer->Map(0, &all, reinterpret_cast<void**>(&data)))) {
            ok = writePng(m.shotPath, data, m.shotWidth, m.shotHeight, m.shotPitch);
            D3D12_RANGE none{0, 0};
            m.shotBuffer->Unmap(0, &none);
        }
        if (ok)
            LOG_INFO("screenshot: %s (%dx%d)", m.shotPath.c_str(), m.shotWidth, m.shotHeight);
        else
            LOG_ERROR("screenshot: %s could not be saved", m.shotPath.c_str());
        m.shotPath.clear();
    }
    m.noteGpuFrame();
    m.notePresentation();  // 2.5.25
    if (paceAfter) {
        Impl::CpuTime paced(m.pacerTicks);
        m.pacer.wait(options.fpsLimit, options.pacerSleeps);
    }
    if (reflex) {  // every frame: the next frame's token, Reflex's sleep
        Impl::CpuTime slept(m.reflexTicks);
        m.upscaler->afterPresent();
    }
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
