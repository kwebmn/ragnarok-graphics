#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <cstring>
#include <string>

#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 3.0.19-beta.9 (R-T8FGMM; the owner: «делай оба шага»): a laptop drew the game on its AMD card while its screen hung
// off its Intel - every present waited 201 ms, Windows handing each frame across the cards. With [renderer]
// cross_present the frames go across here instead, as Microsoft's heterogeneous multi-adapter scheme has it: drawn on
// the fast card into its own pictures (backBuffers), each copied into a buffer of a cross-adapter heap both cards see,
// a shared fence telling the screen's card when it is there; that card copies it into its swap chain's buffer and
// presents. A 1080p frame is 8 MB across - a millisecond or two.

namespace {

std::string cardName(IDXGIAdapter1* a) {
    DXGI_ADAPTER_DESC1 d{};
    if (FAILED(a->GetDesc1(&d))) return {};
    char out[256] = {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, out, sizeof out, nullptr, nullptr);
    return n > 0 ? std::string(out) : std::string();
}

UINT64 alignTo(UINT64 v, UINT64 a) { return (v + a - 1) & ~(a - 1); }

D3D12_RESOURCE_BARRIER transitionOf(ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    return b;
}

// The screen's card's own work done up to `value`
void waitFor(ID3D12Fence* fence, HANDLE event, UINT64 value) {
    if (!fence || !event || !value || fence->GetCompletedValue() >= value) return;
    if (SUCCEEDED(fence->SetEventOnCompletion(value, event))) WaitForSingleObject(event, 2000);
}

}  // namespace

bool Renderer::Impl::crossDevice(HWND hwnd) {
    if (cross.device) return true;
    if (!factory || !device || !hwnd) return false;
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    const LUID drawing = device->GetAdapterLuid();
    IDXGIAdapter1* screen = nullptr;
    IDXGIAdapter1* a = nullptr;
    for (UINT i = 0; !screen && factory->EnumAdapters1(i, &a) == S_OK; ++i) {
        IDXGIOutput* o = nullptr;
        bool shows = false;
        for (UINT j = 0; !shows && a->EnumOutputs(j, &o) == S_OK; ++j) {
            DXGI_OUTPUT_DESC d{};
            shows = SUCCEEDED(o->GetDesc(&d)) && d.Monitor == monitor;
            o->Release();
        }
        if (shows)
            screen = a;
        else
            a->Release();
    }
    if (!screen) {
        LOG_INFO("cross present: no card names the game's monitor - the frames go the usual way");
        return false;
    }
    DXGI_ADAPTER_DESC1 sd{};
    screen->GetDesc1(&sd);
    cross.card = cardName(screen);
    if (std::memcmp(&sd.AdapterLuid, &drawing, sizeof drawing) == 0) {
        LOG_INFO("cross present: the game's monitor hangs off %s, the card drawing - nothing to hand across",
                 cross.card.c_str());
        screen->Release();
        return false;
    }
    // the system's own d3d12.dll: not Streamline's interposer (its hooks are the drawing card's)
    const HMODULE system = LoadLibraryA("d3d12.dll");
    const auto create = system ? reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
                                     reinterpret_cast<void*>(GetProcAddress(system, "D3D12CreateDevice")))
                               : nullptr;
    HRESULT hr = create ? create(screen, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                 reinterpret_cast<void**>(&cross.device))
                        : E_NOINTERFACE;
    screen->Release();
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (SUCCEEDED(hr))
        hr = cross.device->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue),
                                              reinterpret_cast<void**>(&cross.queue));
    for (ID3D12CommandAllocator*& al : cross.allocators)
        if (SUCCEEDED(hr))
            hr = cross.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                      reinterpret_cast<void**>(&al));
    if (SUCCEEDED(hr))
        hr = cross.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cross.allocators[0], nullptr,
                                             __uuidof(ID3D12GraphicsCommandList),
                                             reinterpret_cast<void**>(&cross.list));
    if (SUCCEEDED(hr)) cross.list->Close();
    if (SUCCEEDED(hr))
        hr = cross.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                       reinterpret_cast<void**>(&cross.fence));
    if (SUCCEEDED(hr) && !(cross.event = CreateEventA(nullptr, FALSE, FALSE, nullptr))) hr = E_FAIL;
    // the hand-over: signalled by this card's queue after a frame's copy, waited for by the screen's
    if (SUCCEEDED(hr))
        hr = device->CreateFence(0, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER,
                                 __uuidof(ID3D12Fence), reinterpret_cast<void**>(&cross.handHere));
    HANDLE shared = nullptr;
    if (SUCCEEDED(hr)) hr = device->CreateSharedHandle(cross.handHere, nullptr, GENERIC_ALL, nullptr, &shared);
    if (SUCCEEDED(hr))
        hr = cross.device->OpenSharedHandle(shared, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&cross.handThere));
    if (shared) CloseHandle(shared);
    if (FAILED(hr)) {
        LOG_ERROR("cross present: %s's device for the screen could not be made (0x%08lx) - the frames go the usual way",
                  cross.card.c_str(), static_cast<unsigned long>(hr));
        crossRelease();
        return false;
    }
    LOG_INFO("cross present: the game's monitor hangs off %s - the frames drawn on %s are handed to it, it shows them",
             cross.card.c_str(), adapter.c_str());
    return true;
}

bool Renderer::Impl::crossBuffers(UINT w, UINT h) {
    crossReleaseBuffers();
    for (UINT i = 0; i < 2; ++i)  // the swap chain's buffers - the screen's card's
        if (FAILED(swapChain->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&cross.back[i]))))
            return false;
    for (UINT i = 0; i < 2; ++i) {  // this card's own pictures, where a swap chain's buffers would be
        backBuffers[i] = createTexture2D(device, int(w), int(h), 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                         D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON,
                                         nullptr);
        if (!backBuffers[i]) return false;
        nameResource(backBuffers[i], "cross present picture");
        device->CreateRenderTargetView(backBuffers[i], nullptr, rtvHandle(1 + i));
        backStates[i] = D3D12_RESOURCE_STATE_PRESENT;  // the same as COMMON
    }
    cross.pitch = UINT(alignTo(UINT64(w) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
    const UINT64 size = UINT64(cross.pitch) * h;
    const UINT64 slot = alignTo(size, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    D3D12_HEAP_DESC hd{};
    hd.SizeInBytes = slot * 2;
    hd.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    hd.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    hd.Flags = D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
    HRESULT hr = device->CreateHeap(&hd, __uuidof(ID3D12Heap), reinterpret_cast<void**>(&cross.heapHere));
    HANDLE shared = nullptr;
    if (SUCCEEDED(hr)) hr = device->CreateSharedHandle(cross.heapHere, nullptr, GENERIC_ALL, nullptr, &shared);
    if (SUCCEEDED(hr))
        hr = cross.device->OpenSharedHandle(shared, __uuidof(ID3D12Heap), reinterpret_cast<void**>(&cross.heapThere));
    if (shared) CloseHandle(shared);
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = size;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
    for (UINT i = 0; i < 2 && SUCCEEDED(hr); ++i) {
        hr = device->CreatePlacedResource(cross.heapHere, slot * i, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr,
                                          __uuidof(ID3D12Resource), reinterpret_cast<void**>(&cross.bufHere[i]));
        if (SUCCEEDED(hr))
            hr = cross.device->CreatePlacedResource(cross.heapThere, slot * i, &rd, D3D12_RESOURCE_STATE_COMMON,
                                                    nullptr, __uuidof(ID3D12Resource),
                                                    reinterpret_cast<void**>(&cross.bufThere[i]));
    }
    if (FAILED(hr)) {
        LOG_ERROR("cross present: the buffers both cards see could not be made (0x%08lx, %ux%u)",
                  static_cast<unsigned long>(hr), w, h);
        return false;
    }
    cross.width = w;
    cross.height = h;
    cross.index = 0;
    if (cross.told++ < 4)
        LOG_INFO("cross present: %ux%u, %.1f MB a frame across the cards", w, h, double(size) / (1024.0 * 1024.0));
    return true;
}

void Renderer::Impl::crossReleaseBuffers() {
    if (cross.queue && cross.fence && cross.event) {  // the screen's card done with all of it
        cross.queue->Signal(cross.fence, ++cross.value);
        waitFor(cross.fence, cross.event, cross.value);
    }
    for (ID3D12Resource*& r : cross.back) releaseCom(r);
    for (ID3D12Resource*& r : cross.bufHere) releaseCom(r);
    for (ID3D12Resource*& r : cross.bufThere) releaseCom(r);
    releaseCom(cross.heapHere);
    releaseCom(cross.heapThere);
    cross.done[0] = cross.done[1] = 0;
    cross.width = cross.height = cross.pitch = 0;
}

void Renderer::Impl::crossRelease() {
    crossReleaseBuffers();
    releaseCom(cross.handThere);
    releaseCom(cross.handHere);
    releaseCom(cross.list);
    for (ID3D12CommandAllocator*& a : cross.allocators) releaseCom(a);
    releaseCom(cross.fence);
    if (cross.event) CloseHandle(cross.event);
    cross.event = nullptr;
    releaseCom(cross.queue);
    releaseCom(cross.device);
    cross.on = false;
    cross.value = cross.handed = 0;
}

void Renderer::Impl::crossCopy(UINT index) {
    waitFor(cross.fence, cross.event, cross.done[index]);  // the screen's card done reading it two frames ago
    transition(backBuffers[index], backStates[index], D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
    dst.pResource = cross.bufHere[index];
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    dst.PlacedFootprint.Footprint.Width = cross.width;
    dst.PlacedFootprint.Footprint.Height = cross.height;
    dst.PlacedFootprint.Footprint.Depth = 1;
    dst.PlacedFootprint.Footprint.RowPitch = cross.pitch;
    src.pResource = backBuffers[index];
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

HRESULT Renderer::Impl::crossPresent(UINT index, UINT sync, UINT flags) {
    queue->Signal(cross.handHere, ++cross.handed);  // after the frame's list, with its copy in it
    const UINT bi = swapChain->GetCurrentBackBufferIndex();
    ID3D12CommandAllocator* const a = cross.allocators[index];
    if (FAILED(a->Reset()) || FAILED(cross.list->Reset(a, nullptr))) return E_FAIL;
    D3D12_RESOURCE_BARRIER b =
        transitionOf(cross.back[bi], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    cross.list->ResourceBarrier(1, &b);
    D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
    dst.pResource = cross.back[bi];
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    src.pResource = cross.bufThere[index];
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    src.PlacedFootprint.Footprint.Width = cross.width;
    src.PlacedFootprint.Footprint.Height = cross.height;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = cross.pitch;
    cross.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    b = transitionOf(cross.back[bi], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    cross.list->ResourceBarrier(1, &b);
    if (FAILED(cross.list->Close())) return E_FAIL;
    cross.queue->Wait(cross.handThere, cross.handed);
    ID3D12CommandList* lists[] = {cross.list};
    cross.queue->ExecuteCommandLists(1, lists);
    cross.queue->Signal(cross.fence, ++cross.value);
    cross.done[index] = cross.value;
    const HRESULT hr = swapChain->Present(sync, flags);
    cross.index = index ^ 1u;
    return hr;
}

}  // namespace uagfx::gpu
