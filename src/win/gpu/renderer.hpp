#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"

struct ID3D12Device;

namespace uagfx::gpu {

class Remote;
class Remix9;
class Upscaler;

// Part 3: the renderer in the 64-bit process uaro-render64.exe ([renderer] bridge = 1).
struct BridgeOptions {
    std::string hostExe;             // uaro-render64.exe
    std::string gameDir;             // its log, uaro-render64.log, goes there
    std::string present = "auto";    // auto: the game's window, else a composition surface; window; composition
    bool dpiScale = true;            // composition: scaled for Windows scaling of a DPI-unaware game window
    std::string dlss = "off";        // off, dlaa, quality, balanced, performance, ultra
    int frameGeneration = 0;          // 2-4: DLSS frame generation, 0 none
    bool dlssJitterInverted = false;  // test switches ([quality] dlss_jitter, dlss_motion)
    bool dlssMotionInverted = false;
    int logLevel = 1;                // LogLevel
};

struct PresentOptions {
    bool vsync = true;
    bool allowTearing = false;  // used only with vsync off, when Windows supports it
    int fpsLimit = 0;           // 0 = no limit
    float sharpen = -1.0f;      // CAS sharpness 0..1 of the picture on screen; below 0 = none
};

// Direct3D 12 behind the DirectDraw surfaces: plays frame records onto the render target the client draws into,
// reads that target back for the client and shows it in the window. Called under the driver's global lock.
class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool create(std::string& error);  // device, queue, heaps, shaders; false when Direct3D 12 is not usable
    // Part 3: the same, in uaro-render64.exe; false (and nothing left running) when that process cannot draw.
    bool createRemote(const BridgeOptions& o, std::string& error);
    bool remote() const { return remote_ != nullptr; }
    // Part 4: through Direct3D 9's fixed function - RTX Remix when d3d9Path is its d3d9.dll (remix9.hpp).
    bool createRemix(const std::string& d3d9Path, std::string& error);
    bool remix() const { return remix_ != nullptr; }
    void setWindow(HWND window);  // the game's window, as soon as it is known (Direct3D 9 makes its device on it)
    std::string adapterName() const;
    void setUiSharp(bool sharp);  // [display] ui_filter for interface draws
    // 1B review M8: builds the pipelines whose keys file lists, so effects do not stutter the first time they are
    // drawn, and adds every new pipeline to it.
    void preparePipelines(const std::string& file);
    bool setTarget(int width, int height);  // the client's size: target (BGRA8) and depth (D32) at k = 1, black
    // Stage 3: from now on draw into a target k times the game's size (targetWidth x targetHeight); the current
    // picture is stretched into it, as the back buffer keeps its contents across a present. Stage 4: with `samples`
    // samples per pixel (1, 2, 4 or 8; fewer when the card or its memory has not that many).
    // Part 3: with upscale (and an upscaler, uaro-render64.exe) the world is drawn smaller with a jitter and DLSS
    // makes the target's size of it; MSAA is off then.
    bool setScale(double k, int targetWidth, int targetHeight, int samples = 1, bool upscale = false);
    // Part 3: the game's view matrix of the frame being drawn (false: not known - no motion).
    void setCamera(bool valid, const float view[12]);
    bool upscaling() const;
    int samples() const;  // of the target now
    // Stage 4: anisotropic filtering of the world's textures (0 = off, else 2-16), and alpha testing as coverage in
    // draws without blending while the target has several samples.
    void setDrawQuality(int anisotropy, bool alphaToCoverage);

    uint32_t createTexture(int width, int height, int levels);  // an ARGB8 texture; 0 on failure
    void releaseTexture(uint32_t id);                           // freed once the GPU no longer uses it

    void execute(const FrameRecord& record);        // submits, does not wait
    bool readTarget(std::vector<uint32_t>& argb);   // waits; the game's width * height values, row-major
    // source: in game pixels; shown fitted into a swap chain of outputWidth x outputHeight (real pixels, stage 3).
    void present(HWND window, int outputWidth, int outputHeight, const Rect& source, const PresentOptions& options);

    // uaro-render64.exe's side of the bridge: a texture under the id the driver gave it; the composition surface
    // the game's window shows (when it takes no swap chain from this process); whether the last present found no
    // way to show the picture; whether the GPU is lost.
    bool createTextureAs(uint32_t id, int width, int height, int levels);
    void setUpscaler(Upscaler* u);   // before setScale
    // Before create(): Direct3D 12 and DXGI from this module (Streamline's sl.interposer.dll, so that frame
    // generation can take over the swap chain) instead of the system's.
    void setApi(HMODULE module);
    void finish();                   // waits until the GPU has done everything submitted
    ID3D12Device* device() const;
    void setPresentSurface(HANDLE surface);
    bool presentRefused() const;
    bool lost() const;

    struct Impl;

private:
    Impl* impl_;
    Remote* remote_ = nullptr;
    Remix9* remix_ = nullptr;
};

}  // namespace uagfx::gpu
