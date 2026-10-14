#pragma once
#include <windows.h>
#include <unknwn.h>

#include <cstdint>
#include <string>

struct IDCompositionDevice;
struct IDCompositionTarget;
struct IDCompositionVisual;

namespace uagfx::win::bridge {

// Part 3: when Windows gives the renderer process no swap chain for the game's window (another process owns it), the
// window shows a DirectComposition surface instead and the renderer draws into it through its handle.
class Composition {
public:
    Composition() = default;
    ~Composition();
    Composition(const Composition&) = delete;
    Composition& operator=(const Composition&) = delete;

    // The surface in front of `window`'s client area, its handle duplicated into `renderer` (handleThere). scale: the
    // window's points per real pixel - the renderer's pictures have the real pixels of a DPI-unaware window, which
    // Windows stretches by its scaling; 1 when that does not apply.
    bool attach(HWND window, HANDLE renderer, float scale, uint64_t& handleThere, std::string& error);
    void setScale(float scale);
    void release();
    HWND window() const { return window_; }

private:
    HMODULE dll_ = nullptr;
    IDCompositionDevice* device_ = nullptr;
    IDCompositionTarget* target_ = nullptr;
    IDCompositionVisual* visual_ = nullptr;
    IUnknown* surface_ = nullptr;
    HANDLE handle_ = nullptr;
    HWND window_ = nullptr;
    float scale_ = 1.0f;
};

}  // namespace uagfx::win::bridge
