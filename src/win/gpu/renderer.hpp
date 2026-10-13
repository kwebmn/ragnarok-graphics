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
    bool setTarget(int width, int height);  // the client's target (BGRA8) and depth (D32), cleared to black
    int targetWidth() const;
    int targetHeight() const;

    uint32_t createTexture(int width, int height, int levels);  // an ARGB8 texture; 0 on failure
    void releaseTexture(uint32_t id);                           // freed once the GPU no longer uses it

    void execute(const FrameRecord& record);        // submits, does not wait
    bool readTarget(std::vector<uint32_t>& argb);   // waits; width * height values, row-major
    void present(HWND window, const Rect& source, const PresentOptions& options);

    struct Impl;

private:
    Impl* impl_;
};

}  // namespace uagfx::gpu
