#include "win/bridge/composition.hpp"

#include <dcomp.h>

#include <cstdio>

namespace uagfx::win::bridge {
namespace {

using CreateDeviceFn = HRESULT(WINAPI*)(IDXGIDevice*, REFIID, void**);
using CreateSurfaceHandleFn = HRESULT(WINAPI*)(DWORD, SECURITY_ATTRIBUTES*, HANDLE*);

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

std::string failed(const char* what, HRESULT hr) {
    char line[160];
    std::snprintf(line, sizeof line, "%s failed: 0x%08lx", what, static_cast<unsigned long>(hr));
    return line;
}

}  // namespace

Composition::~Composition() { release(); }

bool Composition::attach(HWND window, HANDLE renderer, float scale, uint64_t& handleThere, std::string& error) {
    release();
    dll_ = LoadLibraryA("dcomp.dll");
    const auto createDevice = dll_ ? reinterpret_cast<CreateDeviceFn>(reinterpret_cast<void*>(
                                         GetProcAddress(dll_, "DCompositionCreateDevice")))
                                   : nullptr;
    const auto createHandle = dll_ ? reinterpret_cast<CreateSurfaceHandleFn>(reinterpret_cast<void*>(
                                         GetProcAddress(dll_, "DCompositionCreateSurfaceHandle")))
                                   : nullptr;
    if (!createDevice || !createHandle) {
        error = "dcomp.dll has no DCompositionCreateSurfaceHandle (Windows 8.1 or newer)";
        release();
        return false;
    }
    HRESULT hr = createDevice(nullptr, __uuidof(IDCompositionDevice), reinterpret_cast<void**>(&device_));
    if (SUCCEEDED(hr)) hr = device_->CreateTargetForHwnd(window, TRUE, &target_);
    if (SUCCEEDED(hr)) hr = device_->CreateVisual(&visual_);
    if (SUCCEEDED(hr)) hr = createHandle(COMPOSITIONOBJECT_ALL_ACCESS, nullptr, &handle_);
    if (SUCCEEDED(hr)) hr = device_->CreateSurfaceFromHandle(handle_, &surface_);
    if (SUCCEEDED(hr)) hr = visual_->SetContent(surface_);
    if (SUCCEEDED(hr)) hr = target_->SetRoot(visual_);
    if (FAILED(hr)) {
        error = failed("the composition surface", hr);
        release();
        return false;
    }
    window_ = window;
    scale_ = 0.0f;
    setScale(scale);  // commits
    HANDLE there = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), handle_, renderer, &there, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        error = "DuplicateHandle failed (error " + std::to_string(GetLastError()) + ")";
        release();
        return false;
    }
    handleThere = uint64_t(uintptr_t(there));  // a handle's value means the same in a 64-bit process
    return true;
}

void Composition::setScale(float scale) {
    if (!visual_ || !(scale > 0.0f) || scale == scale_) return;
    scale_ = scale;
    D2D_MATRIX_3X2_F m{};
    m._11 = scale;
    m._22 = scale;
    visual_->SetTransform(m);
    device_->Commit();
}

void Composition::release() {
    if (target_) target_->SetRoot(nullptr);
    if (device_) device_->Commit();
    drop(surface_);
    drop(visual_);
    drop(target_);
    drop(device_);
    if (handle_) CloseHandle(handle_);
    handle_ = nullptr;
    if (dll_) FreeLibrary(dll_);
    dll_ = nullptr;
    window_ = nullptr;
}

}  // namespace uagfx::win::bridge
