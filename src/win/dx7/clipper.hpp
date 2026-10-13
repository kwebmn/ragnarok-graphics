#pragma once
#include <windows.h>

#include <ddraw.h>

#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

// IDirectDrawClipper. The client attaches one to the primary in windowed mode only to name its window
// (surfaces-present-caps.md §4); frames always go to the window given to SetCooperativeLevel.
class Clipper final : public ClipperBase {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE GetHWnd(HWND* out) override;
    HRESULT STDMETHODCALLTYPE SetHWnd(DWORD flags, HWND hwnd) override;

private:
    LONG refs_ = 1;
    HWND hwnd_ = nullptr;
};

}  // namespace uagfx::dx7
