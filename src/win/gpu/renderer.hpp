#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"

namespace uagfx::gpu {

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
    std::string adapterName() const;
    void setUiSharp(bool sharp);  // [display] ui_filter for interface draws
    // 1B review M8: builds the pipelines whose keys file lists, so effects do not stutter the first time they are
    // drawn, and adds every new pipeline to it.
    void preparePipelines(const std::string& file);
    bool setTarget(int width, int height);  // the client's size: target (BGRA8) and depth (D32) at k = 1, black
    // Stage 3: from now on draw into a target k times the game's size (targetWidth x targetHeight); the current
    // picture is stretched into it, as the back buffer keeps its contents across a present. Stage 4: with `samples`
    // samples per pixel (1, 2, 4 or 8; fewer when the card or its memory has not that many).
    bool setScale(double k, int targetWidth, int targetHeight, int samples = 1);
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

    struct Impl;

private:
    Impl* impl_;
};

}  // namespace uagfx::gpu
