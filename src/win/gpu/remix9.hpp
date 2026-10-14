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
// the CPU, so world draws are turned back into world space with the game's camera (core/game_camera: remixPoint,
// remixView, remixProjection) and drawn with its view and projection. The interface stays in screen space, which
// Remix draws over its picture (rtx.preTransformedVerticesIsUI in rtx.conf), and so does a world draw without a
// depth the camera could have given it (a picture flat on the screen).
class Remix9 {
public:
    Remix9() = default;
    ~Remix9();
    Remix9(const Remix9&) = delete;
    Remix9& operator=(const Remix9&) = delete;

    // The library and, when the game's window is known, the device on it - so a Direct3D 9 that cannot draw (RTX
    // Remix not starting) is known before the game draws and leaves it to Direct3D 12. Without the window the device
    // comes with it.
    bool create(const std::string& d3d9Path, HWND window, std::string& error);
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
    struct Pending {  // pixels the game gave before the texture could be made
        int level = 0;
        Rect rect;
        std::vector<uint32_t> argb;
    };
    struct Texture {
        IDirect3DTexture9* texture = nullptr;
        int width = 0;
        int height = 0;
        int levels = 1;
        std::vector<Pending> pending;
    };
    bool ensureDevice();
    bool makeDevice(long& hr);
    bool ensureTexture(Texture& t);
    void upload(const FrameRecord& rec, const UploadCmd& u);
    void noteShape(HWND window);
    bool resetDevice();
    void applyState(const DrawSetup& s);
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
    bool cameraValid_ = false;
    float camera_[12]{};
    float inverseView_[16]{};
    camera::Projection projection_;
    bool haveSpace_ = false;  // decided from the first camera, kept for the run
    camera::RemixSpace space_;
    uint32_t flatDraws_ = 0;  // world draws of this frame kept on the screen
    bool flatLogged_ = false;
    int shapeLines_ = 0;                // lines about the picture stretched to the window's shape
    SIZE shapeGame_{}, shapeWindow_{};  // the sizes last looked at
    std::vector<uint8_t> worldVertices_;  // a world draw's vertices in world space
    std::vector<uint32_t> pixels_;
};

}  // namespace uagfx::gpu
