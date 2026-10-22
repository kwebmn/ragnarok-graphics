#pragma once
#include <windows.h>

#include <d3d.h>

#include <cstdint>
#include <vector>

#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/portal_capture.hpp"
#include "core/scene.hpp"
#include "core/states.hpp"
#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

class Surface7;

// IDirect3DDevice7: keeps the D3D7 states and bound textures, and turns Clear, SetViewport and the draws into the
// frame record (Task 2) that the renderer plays on the GPU. XYZRHW vertices come transformed and lit, so
// transforms, lights and materials are accepted and ignored (states.md §4, §6). It holds references to its target
// and to the bound textures; nothing holds it, so the client's last Release returns 0 (surfaces-present-caps.md §7).
class Device7 final : public Device7Base {
public:
    explicit Device7(Surface7* target);
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC7* desc) override;
    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void* ctx) override;
    HRESULT STDMETHODCALLTYPE BeginScene() override;
    HRESULT STDMETHODCALLTYPE EndScene() override;
    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirectDrawSurface7* surface, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirectDrawSurface7** surface) override;
    HRESULT STDMETHODCALLTYPE Clear(DWORD count, D3DRECT* rects, DWORD flags, D3DCOLOR color, D3DVALUE z,
                                    DWORD stencil) override;
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX* matrix) override;
    HRESULT STDMETHODCALLTYPE SetViewport(D3DVIEWPORT7* data) override;
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT7* data) override;
    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL7* data) override;
    HRESULT STDMETHODCALLTYPE SetLight(DWORD idx, D3DLIGHT7* data) override;
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override;
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE state, LPDWORD value) override;
    HRESULT STDMETHODCALLTYPE PreLoad(IDirectDrawSurface7* surface) override;
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, void* vertices, DWORD vertexCount,
                                            DWORD flags) override;
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, void* vertices,
                                                   DWORD vertexCount, WORD* indices, DWORD indexCount,
                                                   DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD stage, IDirectDrawSurface7** surface) override;
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirectDrawSurface7* surface) override;
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, LPDWORD value) override;
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override;
    HRESULT STDMETHODCALLTYPE ValidateDevice(LPDWORD passes) override;
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD idx, WINBOOL enable) override;

private:
    ~Device7() override;
    HRESULT record(D3DPRIMITIVETYPE type, DWORD fvf, const void* vertices, DWORD vertexCount, const WORD* indices,
                   DWORD indexCount);

    LONG refs_ = 1;
    Surface7* target_;
    Surface7* textures_[d3d7::kStages] = {};
    DeviceStates states_;
    SceneTracker scene_;  // which draws are interface (stage 3)
    D3DVIEWPORT7 viewport_{};
    std::vector<uint8_t> soft_;  // 0.27: a draw of the level 99 aura's vertices, softer
    // 2.7.37, diagnostics (the tester: «все умения, и даже когда кастуется что-то, какие-то очень яркие»): the skills'
    // draws made softer and the draws that add light of no effect known, told every 10 s
    uint32_t skillDraws_ = 0, plainAdds_ = 0;
    ULONGLONG skillsTold_ = 0;
    // 2.7.53: the game's portal draws recorded into the log (core/portal_capture.hpp), each texture of theirs once
    portal_capture::Recorder portalCapture_;
    std::vector<uint32_t> portalTexturesTold_;
    void capturePortal(const effect_exe::Tag& tag, const DrawSetup& setup, DWORD fvf, const void* vertices,
                       DWORD vertexCount, const WORD* indices, DWORD indexCount);
};

}  // namespace uagfx::dx7
