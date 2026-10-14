#pragma once
#include <windows.h>

#include <d3d12.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/ai/srvgg.hpp"
#include "core/frame.hpp"
#include "core/grf/grf.hpp"
#include "win/gpu/renderer.hpp"

namespace uagfx::gpu {

struct Texture;
const char* aiShaderSource();  // ai_shaders.cpp

// Neural textures (experimental, [quality] ai_textures, uaro-render64.exe): world textures enlarged four times by
// Real-ESRGAN's compact network (core/ai/srvgg) on this GPU, a slice of it every frame, and drawn instead of the
// game's own once done. Every result goes into upscaled.grf next to uaRO.exe, named by the game texture's pixels, so
// the next start - or another player's client given the file - shows it at once. The game knows nothing of it.
class AiTextures {
public:
    explicit AiTextures(Renderer::Impl& m) : m_(m) {}
    ~AiTextures();
    AiTextures(const AiTextures&) = delete;
    AiTextures& operator=(const AiTextures&) = delete;

    // The model, the shaders, a self-test against the CPU's network, upscaled.grf. false: the feature stays off.
    // targetMs: the GPU time a frame the work may take; the amount of work follows what it measures (observe).
    bool start(const std::string& gameDir, double targetMs, std::string& error);
    void observe(UINT slot, double ms);  // what this slot's last share of the work took on the GPU
    // A texture's pixels as the game uploads them: level 0 of a whole texture is kept for the network.
    void noteUpload(uint32_t id, Texture& t, const UploadCmd& u, const uint32_t* argb);
    void noteWorldUse(uint32_t id);  // drawn in the world: its turn comes
    void forget(uint32_t id);        // the game released it
    void work();                     // with the frame's list open, before its present: a slice of the work
    bool shown = true;               // Pause: the game's own textures

private:
    struct Item {
        enum State { Kept, Waiting, Done, Skipped } state = Kept;
        std::string name;  // in upscaled.grf
        int width = 0, height = 0;
        std::vector<uint32_t> argb;  // until done
        int uploads = 0;
    };
    struct Level {
        UINT64 at = 0;  // bytes into the image
        int width = 0, height = 0;
        UINT pitch = 0;  // bytes, a multiple of 256
    };
    struct Job {
        uint32_t id = 0;
        std::string name;
        int width = 0, height = 0;  // the game's texture
        bool loaded = false;        // from upscaled.grf: pixels holds the finished picture
        std::vector<uint32_t> pixels;
        int conv = -1;  // the next convolution; -1 before the input is on the GPU
        int row = 0;    // its next band
    };
    struct Readback {
        ID3D12Resource* buffer = nullptr;
        UINT64 fence = 0;
        std::string name;
        int width = 0, height = 0;
        UINT pitch = 0;
    };
    struct IoRequest {
        bool write = false;
        uint32_t id = 0;
        std::string name;
        std::vector<uint32_t> argb;
        int width = 0, height = 0;
    };
    struct IoResult {
        uint32_t id = 0;
        bool ok = false;
        std::vector<uint32_t> argb;
        int width = 0, height = 0;
    };

    bool createGpu(std::string& error);
    bool selfTest(std::string& error);
    bool ensureBuffers(int width, int height);
    bool uploadInput(const Job& job);
    double runBand(Job& job, double budget);  // MACs dispatched
    bool finish(Job& job);
    void layout(int width, int height, std::vector<Level>& levels, UINT64& bytes) const;
    void dispatchShuffle(const Job& job, ID3D12Resource* image, const Level& level0);
    void dispatchMips(ID3D12Resource* image, const std::vector<Level>& levels);
    void uavBarrier();
    void bindCompute();
    void dropAi(uint32_t id);
    bool nextJob();
    void takeLoaded();
    void finishReadbacks();
    void request(IoRequest r);
    static DWORD WINAPI ioMain(void* self);
    void ioLoop();
    void logNow(bool final);

    Renderer::Impl& m_;
    bool ready_ = false;
    ai::Model model_;
    std::string modelName_ = "realesr-general-x4v3";
    double budget_ = 3e8;          // multiply-adds a frame, adjusted to targetMs_
    double targetMs_ = 2.0;
    double scheduled_[2] = {0, 0};  // multiply-adds put into each slot's list (kSlots)
    ID3D12RootSignature* root_ = nullptr;
    ID3D12PipelineState* conv_ = nullptr;
    ID3D12PipelineState* shuffle_ = nullptr;
    ID3D12PipelineState* mip_ = nullptr;
    ID3D12Resource* weights_ = nullptr;
    ID3D12Resource* planes_ = nullptr;  // the input: R, G, B, A planes
    ID3D12Resource* feat_[2] = {nullptr, nullptr};
    UINT64 planesBytes_ = 0, featBytes_ = 0;
    D3D12_RESOURCE_STATES planesState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    bool computeBound_ = false;  // in this list

    std::unordered_map<uint32_t, Item> items_;
    size_t keptBytes_ = 0;
    std::deque<uint32_t> queue_;
    std::deque<Job> loaded_;
    std::unique_ptr<Job> job_;
    std::vector<Readback> readbacks_;
    std::unordered_set<std::string> known_;  // names in upscaled.grf, or on their way there

    grf::Archive archive_;  // the I/O thread's once started
    bool writable_ = false;
    HANDLE writerMutex_ = nullptr;
    HANDLE ioThread_ = nullptr;
    HANDLE ioWake_ = nullptr;
    CRITICAL_SECTION ioLock_{};
    bool ioLockMade_ = false;
    bool ioStop_ = false;
    std::deque<IoRequest> ioIn_;
    std::deque<IoResult> ioOut_;

    uint32_t doneNetwork_ = 0, doneLoaded_ = 0, written_ = 0;
    ULONGLONG lastLog_ = 0;
};

}  // namespace uagfx::gpu
