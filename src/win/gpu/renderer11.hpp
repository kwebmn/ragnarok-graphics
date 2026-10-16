#pragma once
#include <windows.h>
#include <dxgiformat.h>

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/draw_batch.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/map_objects.hpp"
#include "core/torch.hpp"
#include "core/retry_timer.hpp"
#include "win/gpu/frame_pacer.hpp"
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
    void saveScreenshot(const std::string& path);  // 0.54: the back buffer as it is now, to a PNG
    void setEffects(const EffectSettings& e) { effects_ = e; }  // 0.59: the colours (grade) are drawn here
    void setMap(const std::string& map) { map_ = map; }         // 0.79: the region's look
    void noteObjectTexture(uint32_t id, uint8_t kind) { objectTextures_.note(id, objects::Kind(kind)); }  // 0.61
    void setUiSharp(bool sharp) { uiSharp_ = sharp; }
    void setStateCache(bool on) { stateCache_ = on; }  // 0.88: [debug] state_cache
    void setMergeDraws(bool on) { mergeDraws_ = on; }  // 0.90: [debug] merge_draws
    void setCamera(bool valid, const float view[12], float targetZ = 0);  // 0.99: the game's camera (the modern water)
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
        std::vector<SpriteCell> cells;  // 0.60.1: a sprite cache's cells and what each holds (the shadows)
        bool leafy = false;             // 0.64: leaves (core/foliage.hpp): swayed in the world
    };
    struct Target {  // a picture drawn into and read from
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0, height = 0;
    };
    // 1.0: the glow of the effects (glow11.cpp, as Direct3D 12's effects.cpp): from the world's first draw that adds
    // light the draws write that light into a second target too; at the interface it is shrunk level by level,
    // blurred back up and added onto the picture.
    static constexpr int kGlowLevels11 = 8;
    bool glowWanted() const {
        return effects_.glow && effects_.glowStrength > 0 && drawGlowPs_ && glowDownPs_ && glowUpPs_ && glowAddPs_;
    }
    bool startGlow();
    void addGlow();
    void glowPass(ID3D11PixelShader* ps, ID3D11ShaderResourceView* src, int srcW, int srcH, ID3D11RenderTargetView* dst,
                  int dstW, int dstH, ID3D11BlendState* blend, float factor, float strength, bool first);
    ID3D11PixelShader* drawGlowPs_ = nullptr;
    ID3D11PixelShader* waterGlowPs_ = nullptr;
    ID3D11PixelShader* glowDownPs_ = nullptr;
    ID3D11PixelShader* glowUpPs_ = nullptr;
    ID3D11PixelShader* glowAddPs_ = nullptr;
    Target glow_;
    Target glowChain_[kGlowLevels11];
    int glowLevels_ = 0;
    bool glowActive_ = false;  // the world's draws write into glow_ too
    bool glowDrawn_ = false;   // ... and some did this frame
    bool glowFailed_ = false;  // not made at this size
    bool makeTarget(Target& t, int width, int height, DXGI_FORMAT format);
    // 1.0.1: the game's fireflies drawn anew (torches11.cpp, firefly_ps as Direct3D 12's): taken from its draws, each
    // a soft blinking light added at the interface, behind what stands in front of it
    struct Fly11 {
        float x, y, rhw, size, alpha, colour[3], seed;
    };
    std::vector<Fly11> flies_;
    ID3D11PixelShader* fireflyPs_ = nullptr;
    bool firefliesWanted() const { return effects_.fireflies && effects_.firefliesBrightness > 0 && fireflyPs_; }
    bool takeFirefly(const FrameRecord& rec, const DrawCmd& d);
    void applyFireflies();
    void applyGrade();  // 0.59
    void applyNight();  // 0.63
    void worldPass(ID3D11PixelShader* ps, const Constants& c);
    // 0.60.1: the characters' shadows (actor_shadows11.cpp, as Direct3D 12's actor_shadows.cpp - the sun's light
    // alone: no map's lamps without the camera)
    bool shadowsWanted() const {
        return effects_.actorShadows && effects_.actorShadowsStrength > 0 && shadowVs_ && shadowPs_ && shadowBlurPs_ &&
               shadowApplyPs_ && shadowLayout_;
    }
    void applyActorShadows(const FrameRecord& rec, size_t from);
    bool ensureShadowMask(int w, int h);
    static void noteCells(Texture& t, const SpriteCell* slots, uint32_t count);
    // 0.61: the torches' light (torches11.cpp, as Direct3D 12's torch_light_ps): the flames the game draws noted,
    // their light onto the ground before the colours; the flames and their glow stay the game's.
    void noteTorch(const FrameRecord& rec, const DrawCmd& d);
    void applyTorchLight();
    bool torchesWanted() const { return effects_.torch && effects_.torchLight > 0 && torchLightPs_; }
    bool compileShaders(const std::string& cacheFile, Renderer::StartSink sink, void* sinkContext, std::string& error);
    bool makeTarget(Target& t, int width, int height) {
        return makeTarget(t, width, height, DXGI_FORMAT_B8G8R8A8_UNORM);
    }
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
    // 0.99: the modern water (water_vs/water_ps as Direct3D 12's): the picture and the depth as they were before the
    // frame's first water (scene_, depthCopy_), the game's camera backwards
    ID3D11VertexShader* waterVs_ = nullptr;
    ID3D11PixelShader* waterPs_ = nullptr;
    ID3D11Texture2D* depthCopy_ = nullptr;
    ID3D11DepthStencilView* depthCopyDsv_ = nullptr;
    ID3D11ShaderResourceView* depthCopySrv_ = nullptr;
    bool waterCopied_ = false;  // this frame's copies made (or could not be)
    bool waterReady_ = false;   // ... and the water is drawn from them
    bool cameraValid_ = false;
    float viewToWorld_[12] = {};
    LARGE_INTEGER waterClock_{};
    unsigned long long waterCells_ = 0;
    float waterTime();
    bool copyForWater();
    ID3D11VertexShader* presentVs_ = nullptr;
    ID3D11PixelShader* presentPs_ = nullptr;
    ID3D11PixelShader* gradePs_ = nullptr;  // 0.59
    ID3D11VertexShader* shadowVs_ = nullptr;  // 0.60.1
    ID3D11PixelShader* shadowPs_ = nullptr;
    ID3D11PixelShader* shadowBlurPs_ = nullptr;
    ID3D11PixelShader* shadowApplyPs_ = nullptr;
    ID3D11InputLayout* shadowLayout_ = nullptr;
    ID3D11Buffer* shadowVb_ = nullptr;
    size_t shadowVbBytes_ = 0;
    ID3D11ShaderResourceView* depthSrv_ = nullptr;
    ID3D11Texture2D* depthSrvOf_ = nullptr;  // the depth buffer depthSrv_ reads
    // 0.67, soft particles: the depth tested but not written, while the effect reads it (t3)
    ID3D11DepthStencilView* readOnlyDsv_ = nullptr;
    ID3D11Texture2D* readOnlyDsvOf_ = nullptr;
    bool depthViews();  // depthSrv_ and readOnlyDsv_ for depth_; false when they cannot be made
    ID3D11BlendState* maxBlend_ = nullptr;   // the mask: the darkest shadow
    ID3D11BlendState* mulBlend_ = nullptr;   // onto the picture: multiplied
    Target shadowMask_[2];
    bool groundDrawn_ = false, shadowsDone_ = false, shadowsLogged_ = false;
    // 0.61: the torches' light
    ID3D11PixelShader* torchLightPs_ = nullptr;
    ID3D11PixelShader* nightPs_ = nullptr;  // 0.63
    bool nightDone_ = false;
    ID3D11Texture2D* lightData_ = nullptr;  // two texels a light (torch_light_ps's t1)
    ID3D11ShaderResourceView* lightDataSrv_ = nullptr;
    objects::Textures objectTextures_;
    std::vector<torch::Flame> flames_;  // this frame's
    bool lightDone_ = false, lightLogged_ = false;
    unsigned long long clockStart_ = 0;
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
    Target scene_;   // 0.59: its copy the colours are read from
    EffectSettings effects_;
    std::string map_;
    bool worldDrawn_ = false, gradeDone_ = false;  // this frame
    ID3D11Texture2D* depth_ = nullptr;
    ID3D11DepthStencilView* dsv_ = nullptr;
    Target game_;  // the game's size: its own pixels on the way in, the picture on the way out
    ID3D11Texture2D* staging_ = nullptr;
    int gameWidth_ = 0, gameHeight_ = 0;
    double scale_ = 1.0;
    ViewportCmd gameViewport_;
    bool targetBound_ = false;
    // 0.88: what the game's draws bound last - a draw calls only for what differs (an old graphics driver spends much
    // of the processor on each call, and on Direct3D 11 that is the game's own thread). Forgotten when the target is
    // bound anew: every other pass resets targetBound_ before it binds anything of its own.
    struct Bound {
        bool valid = false;
        ID3D11BlendState* blend = nullptr;
        ID3D11DepthStencilState* depth = nullptr;
        ID3D11RasterizerState* raster = nullptr;
        ID3D11SamplerState* samplers[2] = {nullptr, nullptr};
        ID3D11ShaderResourceView* srvs[2] = {nullptr, nullptr};
        ID3D11InputLayout* layout = nullptr;
        unsigned topology = 0;  // a D3D11_PRIMITIVE_TOPOLOGY (0 none)
        ID3D11Buffer* vertexBuffer = nullptr;  // bound at an offset of `phase` bytes with `stride`
        UINT stride = 0, phase = 0;
        ID3D11Buffer* indexBuffer = nullptr;
        ID3D11VertexShader* vs = nullptr;  // 0.99
        ID3D11PixelShader* ps = nullptr;
    } bound_;
    // 0.90: a draw call's state - the game's draws with the same one and their vertices one after another go to the
    // driver as one draw call (core/draw_batch.hpp); the views it reads are held while it is gathered
    struct Issue {
        Constants c{};
        ID3D11BlendState* blend = nullptr;
        ID3D11DepthStencilState* depth = nullptr;
        ID3D11RasterizerState* raster = nullptr;
        ID3D11SamplerState* samplers[2] = {nullptr, nullptr};
        ID3D11ShaderResourceView* srvs[2] = {nullptr, nullptr};
        ID3D11InputLayout* layout = nullptr;
        ID3D11VertexShader* vs = nullptr;  // 0.99: the game's draw's, or the water's
        ID3D11PixelShader* ps = nullptr;
        uint32_t primitive = 0, firstIndex = 0, indexCount = 0;  // a draw by itself: its own
        bool sameState(const Issue& o) const;
    };
    void flushBatch();  // the gathered draw call to the driver
    Issue issue_;
    batch::Batch batch_;
    ID3D11Buffer* quadIndices_ = nullptr;  // batch::quadIndices: the quads as lists of triangles
    bool mergeDraws_ = true;
    // 0.88: what the constant buffer holds - the same constants are not written again
    Constants constantsNow_{};
    bool constantsKnown_ = false;
    bool stateCache_ = true;
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
    FramePacer pacer_;  // 0.85: [frames] fps_limit

    static constexpr UINT kTimedFrames = 4;
    static constexpr UINT kStamps = 16;  // 8 spans a frame; more of them stretch the last
    struct FrameTimes {
        ID3D11Query* disjoint = nullptr;
        ID3D11Query* stamps[kStamps] = {};
        ID3D11Query* effect[4][2] = {};  // 0.89: the effects' own spans (EffectTime11)
        unsigned effectBegun = 0, effectEnded = 0;  // bits by EffectTime11
        UINT used = 0;          // stamps ended
        bool open = false;      // its disjoint begun: this frame is timed
        bool inSpan = false;    // a span begun, its end not yet
        bool pending = false;   // ended, its data not read yet
    };
    FrameTimes times_[kTimedFrames];
    // 0.89: the effects timed one by one on the card (the log's "GPU: effects a frame" line)
    enum EffectTime11 { kTimeShadows11, kTimeLight11, kTimeNight11, kTimeGrade11, kEffectTimes11 };
    void effectBegin(EffectTime11 e);  // once a frame each, in an open frame
    void effectEnd(EffectTime11 e);
    struct EffectSpan {  // the scope of an effect's pass
        Renderer11& r;
        EffectTime11 e;
        EffectSpan(Renderer11& renderer, EffectTime11 effect) : r(renderer), e(effect) { r.effectBegin(e); }
        ~EffectSpan() { r.effectEnd(e); }
        EffectSpan(const EffectSpan&) = delete;
        EffectSpan& operator=(const EffectSpan&) = delete;
    };
    double effectMs_[kEffectTimes11] = {};
    // 0.89: what the frames ask of the processor and the driver, the log every half minute - draws, the driver's
    // calls made and spared, the processor's time in drawing; the game's textures made, reused, freed
    struct Stats {
        uint64_t frames = 0, draws = 0, calls = 0, spared = 0, made = 0, reused = 0, freed = 0, uploaded = 0;
        uint64_t batches = 0;  // 0.90: the draw calls the game's draws went in
        LONGLONG ticks = 0;
    } stats_;
    // 0.89: textures the game let go, kept for its next one of the same size and levels - it makes and frees small
    // ones again and again, and an old driver takes long to make each
    struct Pooled {
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0, height = 0, levels = 1;
    };
    std::vector<Pooled> pool_;
    size_t pooledBytes_ = 0;
    UINT timeSlot_ = 0;
    bool timing_ = false;   // the queries were made
    bool untimed_ = false;  // this frame's slot was still busy: the frame is not timed
    double gpuMs_ = 0.0;
    uint32_t gpuFrames_ = 0;
    ULONGLONG gpuSince_ = 0;
    std::map<std::pair<std::string, long>, int> failures_;  // a failure of each place and code logged once
};

}  // namespace uagfx::gpu
