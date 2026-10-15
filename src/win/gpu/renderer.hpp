#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/start_screen.hpp"

struct ID3D12Device;

namespace uagfx::gpu {

class Remote;
class Remix9;
class Renderer11;
class Upscaler;
class Supersample;
class Taa;

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
    AiSettings ai;                    // AI textures ([ai])
    int logLevel = 1;                // LogLevel
    std::string gpu;                 // 0.50.2: [renderer] gpu, "" the fast card
};

struct PresentOptions {
    bool vsync = true;
    bool allowTearing = false;  // used only with vsync off, when Windows supports it
    int fpsLimit = 0;           // 0 = no limit
    float sharpen = -1.0f;      // CAS sharpness 0..1 of the picture on screen; below 0 = none
    bool foreground = true;     // the game's window is in front: frame generation only then
    bool vsyncSetting = true;   // vsync as set (vsync above is off for a Blt that shows at once); Remix's interval
    std::string screenshot;     // 0.54: the picture as shown, to this PNG file (Ctrl+Shift+F12); "" none
    bool hdr = false;           // 0.65: [display] hdr - HDR10 where the monitor shows HDR (Direct3D 12)
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
    bool upscales() const;  // part 3: DLSS is ready (in uaro-render64.exe)
    // Part 4: through Direct3D 9's fixed function - RTX Remix when d3d9Path is its d3d9.dll (remix9.hpp).
    bool createRemix(const std::string& d3d9Path, HWND window, std::string& error);  // window: the game's, or null
    bool remix() const { return remix_ != nullptr; }
    // 0.46: with Direct3D 11 (renderer11.hpp) - graphics cards without Direct3D 12; its shaders kept in shaderCache.
    bool createD3D11(const std::string& shaderCache, std::string& error);
    bool d3d11() const { return d3d11_ != nullptr; }
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
    // aiShown: AI textures drawn instead of the game's (off with Pause, to compare).
    void setDrawQuality(int anisotropy, bool alphaToCoverage, bool aiShown = true);
    // 0.18: [effects] as they apply now (off with Pause); nothing in Remix's Direct3D 9.
    void setEffects(const EffectSettings& e);

    uint32_t createTexture(int width, int height, int levels);  // an ARGB8 texture; 0 on failure
    void releaseTexture(uint32_t id);                           // freed once the GPU no longer uses it
    // 0.23.1, 0.24: the texture is a frame of a map object the driver draws anew (core/map_objects.hpp: objects::Kind;
    // 0 none) - a 32-bit frame of its sprite, which the game draws from a texture of its own.
    void noteObjectTexture(uint32_t id, uint8_t kind);

    void execute(const FrameRecord& record);        // submits, does not wait
    bool readTarget(std::vector<uint32_t>& argb);   // waits; the game's width * height values, row-major
    // source: in game pixels; shown fitted into a swap chain of outputWidth x outputHeight (real pixels, stage 3).
    void present(HWND window, int outputWidth, int outputHeight, const Rect& source, const PresentOptions& options);

    // uaro-render64.exe's side of the bridge: a texture under the id the driver gave it; the composition surface
    // the game's window shows (when it takes no swap chain from this process); whether the last present found no
    // way to show the picture; whether the GPU is lost.
    bool createTextureAs(uint32_t id, int width, int height, int levels);
    void setUpscaler(Upscaler* u);   // before setScale
    // 0.62: [display] ui_native - the world `ratio` times the picture, made smaller into it, the interface at the
    // picture's pixels (setScale with upscale then); false where it cannot (Direct3D 11, the 64-bit renderer).
    bool setSupersample(double ratio);
    bool setTaa(bool on);  // 0.75: [display] taa - Direct3D 12 in the game's process; false when it is not used
    bool canSupersample() const { return !remote_ && !d3d11_ && !remix_; }
    // 0.39.5, before create(): the shaders' code is kept in this file between starts - compiled once, not each time.
    void setShaderCache(const std::string& file);
    // Before create(): Direct3D 12 and DXGI from this module (Streamline's sl.interposer.dll, so that frame
    // generation can take over the swap chain) instead of the system's.
    void setApi(HMODULE module);
    // Before create(): called with the device as soon as it exists, before anything is made on it (Streamline wants
    // slSetD3DDevice before the command queue, 0.10 log: "API hook is activated without device being created").
    void setOnDevice(void (*fn)(void* context, ID3D12Device* device), void* context);
    // AI textures (ai_textures.hpp, uaro-render64.exe): after create(). false: off, the reason in the log.
    bool startAiTextures(const std::string& gameDir, const AiSettings& settings);
    // 0.43, the pack builder (uaro-render64.exe --pack): the networks alone, after create(); then a picture at a time,
    // waiting for it - 4 times the size, by the network of its kind (ai::Kind), border as AiTextures::enlarge's.
    bool startAiForPack(const std::string& gameDir, const AiSettings& settings, std::string& error);
    bool enlargeForPack(int kind, const uint32_t* argb, int width, int height, uint32_t border,
                        std::vector<uint32_t>& out, std::string& error);
    std::string packNetName(int kind) const;
    // The map the game shows ("prontera"; "" none): the texture pack's index notes the textures seen on it (AI
    // textures made); 0.25: its file is read for its lamps and the torches the game does not show.
    void setMap(const std::string& map);
    // 0.25: the game's folder - its data and DATA.INI (the driver's own renderer; uaro-render64.exe has it at start).
    void setGameDir(const std::string& dir);
    // AI icons (0.14): a picture the game copies into one of its windows - w x h straight ARGB, transparent where its
    // key skips - for the network; the picture made larger comes to the sink, keyed as asked. In the driver the sink
    // gets it from the bridge; in uaro-render64.exe the sink sends it over the bridge.
    using IconSink = void (*)(void* context, uint64_t key, int width, int height, const uint32_t* argb);
    // 0.40, before create() or createRemote(): told what the start does (core/start_screen.hpp) - the shaders loaded
    // or compiled one by one; through the bridge uaro-render64.exe's DLSS and AI textures too.
    using StartSink = void (*)(void* context, const start::Progress& progress);
    void setStartSink(StartSink sink, void* context);
    void requestIcon(uint64_t key, int width, int height, const uint32_t* argb);
    void setIconSink(IconSink sink, void* context);
    // Part 3, DLSS started after the device (in the background, so that the game does not wait for it): the DXGI
    // factory becomes Streamline's (upgrade = slUpgradeInterface) and the swap chain is made again through it at the
    // next present, so that Streamline sees every present. False: the factory stays the system's.
    bool upgradeFactory(bool (*upgrade)(void** object));
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
    Renderer11* d3d11_ = nullptr;
    Supersample* supersample_ = nullptr;  // 0.62
    Taa* taa_ = nullptr;                  // 0.75
};

}  // namespace uagfx::gpu
