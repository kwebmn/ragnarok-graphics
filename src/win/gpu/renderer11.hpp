#pragma once
#include <windows.h>

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/retry_timer.hpp"
#include "win/gpu/renderer.hpp"
#include "win/gpu/shaders.hpp"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11Buffer;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;
struct ID3D11Query;
struct IDXGIFactory1;
struct IDXGISwapChain;

namespace uagfx::gpu {

class Ai11;

// 0.46: the game drawn with Direct3D 11 - for graphics cards without Direct3D 12 (Radeon HD 5000-7000, GeForce GTX
// 400-600 with old drivers, Intel HD 4000) and Windows 7 and 8. The same shaders as Direct3D 12's (shaders.cpp): the
// game's fixed function, the target at the window's size with the interface's filter, crisp text's twins, the
// picture fitted into the window with its sharpening; no DLSS or effects yet. Feature level 11_0 takes shader model 5,
// 10_0 and 10_1 shader model 4 (when the shaders compile for it). 0.47: the AI textures' ready pictures (ai11.hpp).
class Renderer11 {
public:
    Renderer11() = default;
    ~Renderer11();
    Renderer11(const Renderer11&) = delete;
    Renderer11& operator=(const Renderer11&) = delete;

    // The device and its shaders - read from shaderCache when a run kept them, else compiled and kept there; the
    // start screen is told of them.
    bool create(const std::string& shaderCache, Renderer::StartSink sink, void* sinkContext, std::string& error);
    std::string adapterName() const { return adapter_; }
    bool setTarget(int width, int height);
    bool setScale(double k, int targetWidth, int targetHeight);
    uint32_t createTexture(int width, int height, int levels);
    void releaseTexture(uint32_t id);
    void execute(const FrameRecord& record);
    bool readTarget(std::vector<uint32_t>& argb);
    void present(HWND window, int outputWidth, int outputHeight, const Rect& source, const PresentOptions& options);
    void setUiSharp(bool sharp) { uiSharp_ = sharp; }
    void setAnisotropy(int anisotropy) { anisotropy_ = anisotropy; }
    // 0.47: the AI textures from the texture pack (none made); false: off, the reason in the log.
    bool startAi(const std::string& gameDir, const AiSettings& settings);
    void setAiShown(bool shown);  // Pause: the game's own textures
    void requestIcon(uint64_t key, int width, int height, const uint32_t* argb);
    void setIconSink(Renderer::IconSink sink, void* context);
    void finish();
    bool lost() const { return lost_; }

private:
    struct Texture {
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0, height = 0, levels = 1;
    };
    struct Target {  // a picture drawn into and read from
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0, height = 0;
    };
    bool compileShaders(const std::string& cacheFile, Renderer::StartSink sink, void* sinkContext, std::string& error);
    bool makeTarget(Target& t, int width, int height);
    void dropTarget(Target& t);
    bool makeDepth(int width, int height, ID3D11Texture2D*& depth, ID3D11DepthStencilView*& dsv);
    bool makeStaging(int width, int height);
    bool uploadGeometry(const FrameRecord& rec);
    void clear(const ClearCmd& c);
    void applyViewport();
    void upload(const FrameRecord& rec, const UploadCmd& u);
    void writeTarget(const FrameRecord& rec, const WriteTargetCmd& w);
    void draw(const FrameRecord& rec, const DrawCmd& d);
    // srcRect of a picture (its pixels) into dstRect of a target: bilinear when enlarged, the footprint's average
    // when shrunk; sharpen 0-1 sharpens (CAS), below 0 does not.
    void blit(ID3D11ShaderResourceView* src, int srcWidth, int srcHeight, const float srcRect[4],
              ID3D11RenderTargetView* dst, const Rect& dstRect, int dstWidth, int dstHeight, float sharpen);
    void bindTarget();  // the target, its depth, the game's viewport: for the game's draws
    void setConstants(const Constants& c);
    ID3D11BlendState* blendState(bool enable, uint32_t src, uint32_t dst, uint8_t writeMask);
    ID3D11DepthStencilState* depthState(bool enable, bool write, uint32_t func);
    ID3D11RasterizerState* rasterState(uint32_t cull);
    ID3D11SamplerState* samplerState(const StageSetup& s, int aniso);
    bool ensureSwapChain(HWND window, UINT width, UINT height, bool wantTearing);
    void releaseSwapChain();
    void failed(const char* what, long hr);
    // 0.46.1: the GPU's time a frame, as Direct3D 12 logs it - timestamps around each record's work and the
    // picture's, summed (the GPU waits for the game between them); read 4 frames on, never waited for.
    void timeBegin();
    void timeEnd();
    void timeFrame();
    bool readTimes(UINT slot, bool wait);
    void logGpu(ULONGLONG now);

    HMODULE dll_ = nullptr;
    Ai11* ai_ = nullptr;  // 0.47
    Renderer::IconSink iconSink_ = nullptr;
    void* iconContext_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* ctx_ = nullptr;
    unsigned level_ = 0;  // D3D_FEATURE_LEVEL
    IDXGIFactory1* factory_ = nullptr;
    std::string adapter_;
    bool bgraVertices_ = true;  // B8G8R8A8 vertex colours: else they are turned into R8G8B8A8 on the way
    bool lost_ = false;

    ID3D11VertexShader* drawVs_ = nullptr;
    ID3D11PixelShader* drawPs_ = nullptr;
    ID3D11VertexShader* presentVs_ = nullptr;
    ID3D11PixelShader* presentPs_ = nullptr;
    ID3D11VertexShader* clearVs_ = nullptr;
    ID3D11PixelShader* clearPs_ = nullptr;
    ID3D11InputLayout* layout_[2] = {nullptr, nullptr};  // one UV set, two
    ID3D11Buffer* constants_ = nullptr;
    ID3D11Buffer* clearConstants_ = nullptr;
    ID3D11Buffer* vertices_ = nullptr;
    ID3D11Buffer* indices_ = nullptr;
    UINT vertexBytes_ = 0, indexBytes_ = 0;
    bool geometry_ = false;  // this record's vertices are in vertices_
    Texture white_;
    std::unordered_map<uint32_t, Texture> textures_;
    uint32_t nextTexture_ = 1;
    std::unordered_map<uint64_t, ID3D11BlendState*> blends_;
    std::unordered_map<uint64_t, ID3D11DepthStencilState*> depths_;
    std::unordered_map<uint64_t, ID3D11RasterizerState*> rasters_;
    std::unordered_map<uint64_t, ID3D11SamplerState*> samplers_;

    Target target_;  // the game's picture at the window's size (k times the game's)
    ID3D11Texture2D* depth_ = nullptr;
    ID3D11DepthStencilView* dsv_ = nullptr;
    Target game_;  // the game's size: its own pixels on the way in, the picture on the way out
    ID3D11Texture2D* staging_ = nullptr;
    int gameWidth_ = 0, gameHeight_ = 0;
    double scale_ = 1.0;
    ViewportCmd gameViewport_;
    bool targetBound_ = false;
    bool uiSharp_ = true;
    int anisotropy_ = 0;

    IDXGISwapChain* swap_ = nullptr;
    ID3D11RenderTargetView* backRtv_ = nullptr;
    HWND swapWindow_ = nullptr;
    UINT swapWidth_ = 0, swapHeight_ = 0, swapFlags_ = 0;
    UINT failedWidth_ = 0, failedHeight_ = 0;
    RetryTimer swapRetry_{2000};
    bool tearing_ = false;
    Rect picture_;
    LARGE_INTEGER lastPresent_{};

    static constexpr UINT kTimedFrames = 4;
    static constexpr UINT kStamps = 16;  // 8 spans a frame; more of them stretch the last
    struct FrameTimes {
        ID3D11Query* disjoint = nullptr;
        ID3D11Query* stamps[kStamps] = {};
        UINT used = 0;          // stamps ended
        bool open = false;      // its disjoint begun: this frame is timed
        bool inSpan = false;    // a span begun, its end not yet
        bool pending = false;   // ended, its data not read yet
    };
    FrameTimes times_[kTimedFrames];
    UINT timeSlot_ = 0;
    bool timing_ = false;   // the queries were made
    bool untimed_ = false;  // this frame's slot was still busy: the frame is not timed
    double gpuMs_ = 0.0;
    uint32_t gpuFrames_ = 0;
    ULONGLONG gpuSince_ = 0;
    std::map<std::pair<std::string, long>, int> failures_;  // a failure of each place and code logged once
};

}  // namespace uagfx::gpu
