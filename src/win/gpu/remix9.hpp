#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/frame.hpp"
#include "core/game_camera.hpp"
#include "core/image.hpp"
#include "win/gpu/renderer.hpp"

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace uagfx::gpu {

// Part 4 (RTX Remix): the game drawn through Direct3D 9's fixed-function pipeline - through RTX Remix when its d3d9.dll
// is next to uaRO.exe. Remix path traces geometry it gets in world space with a camera; the game projects its world on
// the CPU, so world draws are turned back into world space with the game's camera (core/game_camera, the same exact
// inverse as DLSS's motion) and drawn with its view and projection. The interface stays in screen space, which Remix
// draws over its picture (rtx.preTransformedVerticesIsUI in rtx.conf).
class Remix9 {
public:
    Remix9() = default;
    ~Remix9();
    Remix9(const Remix9&) = delete;
    Remix9& operator=(const Remix9&) = delete;

    bool create(const std::string& d3d9Path, std::string& error);  // the library; the device comes with the window
    std::string adapterName() const { return adapter_; }
    void setWindow(HWND window) { window_ = window; }
    bool setTarget(int width, int height);
    uint32_t createTexture(int width, int height, int levels);
    void releaseTexture(uint32_t id);
    void execute(const FrameRecord& record);
    bool readTarget(std::vector<uint32_t>& argb);
    void present(HWND window, const PresentOptions& options);
    void setCamera(bool valid, const float view[12]);

private:
    struct Texture {
        IDirect3DTexture9* texture = nullptr;
        int width = 0;
        int height = 0;
        int levels = 1;
    };
    bool ensureDevice();
    bool resetDevice();
    void applyState(const DrawSetup& s, bool world);
    void draw(const FrameRecord& rec, const DrawCmd& d);
    IDirect3DTexture9* textureOf(uint32_t id);

    HMODULE dll_ = nullptr;
    IDirect3D9* d3d_ = nullptr;
    IDirect3DDevice9* device_ = nullptr;
    std::string adapter_;
    HWND window_ = nullptr;
    int width_ = 0;  // the game's size: the back buffer
    int height_ = 0;
    bool vsync_ = true;
    bool lost_ = false;
    std::unordered_map<uint32_t, Texture> textures_;
    uint32_t nextTexture_ = 1;
    IDirect3DTexture9* white_ = nullptr;
    bool haveState_ = false;
    DrawSetup state_;  // applied last
    bool stateWorld_ = false;
    bool cameraValid_ = false;
    float camera_[12]{};
    float inverseView_[16]{};
    camera::Projection projection_;
    std::vector<uint8_t> worldVertices_;  // a world draw's vertices in world space
    std::vector<uint32_t> pixels_;
};

}  // namespace uagfx::gpu
