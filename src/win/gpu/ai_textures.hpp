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

#include "core/ai/kinds.hpp"
#include "core/ai/srvgg.hpp"
#include "core/config.hpp"
#include "core/frame.hpp"
#include "core/grf/grf.hpp"
#include "core/texture_pack.hpp"
#include "win/gpu/renderer.hpp"

namespace uagfx::gpu {

struct Texture;
const char* aiShaderSource();  // ai_shaders.cpp

// Neural textures (experimental, [ai], uaro-render64.exe): world textures enlarged by Real-ESRGAN's compact networks
// (core/ai/srvgg) on this GPU, a slice of the work every frame, and drawn instead of the game's own once done. The
// kind of a texture (core/ai/kinds: the ground, objects, sprites, effects) comes from the draw that first shows it
// and chooses whether and through which network it goes. Every result goes into upscaled.grf next to uaRO.exe, named
// by the game texture's pixels, so the next start - or another player's client given the file - shows it at once;
// beside it the kinds and maps the textures were seen as and on, for the settings app. The game knows nothing of it.
class AiTextures {
public:
    explicit AiTextures(Renderer::Impl& m) : m_(m) {}
    ~AiTextures();
    AiTextures(const AiTextures&) = delete;
    AiTextures& operator=(const AiTextures&) = delete;

    // The models, the shaders, a self-test of each against the CPU's network, upscaled.grf. false: the feature stays
    // off. The amount of work a frame follows the GPU time it takes (observe) to the settings' budget.
    bool start(const std::string& gameDir, const AiSettings& settings, std::string& error);
    void observe(UINT slot, double ms);  // what this slot's last share of the work took on the GPU
    // A texture's pixels as the game uploads them: level 0 of a whole texture is kept for the network. cells: the
    // cells of the game's sprite cache that got a new frame (UploadCmd::slots) - each enlarged on its own.
    void noteUpload(uint32_t id, Texture& t, const UploadCmd& u, const uint32_t* argb, const SpriteCell* cells,
                    uint32_t cellCount);
    void noteWorldUse(uint32_t id, ai::Kind kind);  // drawn in the world as `kind`: its turn comes, if that is on
    // 0.14.1: drawn in the interface - a big picture (the loading screen, the login's backgrounds) gets its turn as
    // an icon of the interface (the kind Icons) and stands in for interface draws.
    void noteUiUse(uint32_t id);
    void forget(uint32_t id);                       // the game released it
    void setMap(const std::string& map);            // the map the game shows now, "" none
    // 0.14: an icon of the game's windows (Renderer::requestIcon); made larger, it goes to Impl::iconSink.
    void noteIcon(uint64_t key, int width, int height, const uint32_t* argb);
    void work();                                    // with the frame's list open, before its present: a slice
    bool shown = true;                              // Pause: the game's own textures

    // 0.43, the pack builder (Renderer::buildPack): the networks of the settings' kinds - no upscaled.grf, no pack.
    bool startForPack(const std::string& gameDir, const AiSettings& settings, std::string& error);
    int netFor(ai::Kind kind) const { return int(kind) < ai::kKinds ? netOf_[int(kind)] : -1; }
    const std::string& netName(int net) const { return nets_[size_t(net)]->name; }
    // The network's picture of argb (w x h) at once, on the renderer's list, waiting for the GPU: 4 times its size;
    // border 0 the other side past the edges, 1 nothing, 2 the edge again (a world texture, a sprite's cell, an
    // interface picture).
    bool enlarge(int net, const uint32_t* argb, int w, int h, uint32_t border, std::vector<uint32_t>& out,
                 std::string& error);

private:
    struct Net {
        std::string token;  // [ai] model: general, anime
        std::string name;   // its file in uaro-graphics\ai: realesr-general-x4v3
        int denoise = 100;  // general: % of it in the mix with its weak-denoise twin
        ai::Model model;
        ID3D12Resource* weights = nullptr;
        std::string folder;  // of its pictures in upscaled.grf (ai::netFolder)
    };
    struct Item {
        // Away (0.44): done, its copy put away over the video memory's budget - drawn again, it comes back from the
        // texture pack or upscaled.grf.
        enum State { Kept, Waiting, Done, Skipped, Away } state = Kept;
        std::string key;                 // the game texture's (ai::keyName)
        ai::Kind kind = ai::Kind::None;  // as first drawn, or its tag in upscaled.grf (ahead)
        int net = -1;
        std::string name;  // its picture in upscaled.grf, once the kind is known
        int width = 0, height = 0;
        std::vector<uint32_t> argb;  // until done
        int uploads = 0;
        uint32_t mapSerial = 0;  // the map whose tag it has been seen to
        bool later = false;      // in later_: done ahead, before it is drawn
        bool loading = false;    // its picture is being read from upscaled.grf
        bool ui = false;         // a picture of the interface: it stands in for interface draws, not the world's
        uint64_t lastUse = 0;    // 0.44: the frame it was last drawn in (frame_)
        uint64_t bytes = 0;      // its copy's in video memory (Texture::ai), all its levels
    };
    struct Level {
        UINT64 at = 0;  // bytes into the image
        int width = 0, height = 0;
        UINT pitch = 0;  // bytes, a multiple of 256
    };
    struct Job {
        uint32_t id = 0;
        int net = 0;
        std::string name;
        int width = 0, height = 0;  // the game's texture, or the cell
        bool loaded = false;        // from upscaled.grf: pixels holds the finished picture
        std::vector<uint32_t> pixels;
        int conv = -1;  // the next convolution; -1 before the input is on the GPU
        int row = 0;    // its next band
        bool cell = false;  // a cell of a sprite cache: the picture goes into the cache's enlarged copy
        int cellX = 0, cellY = 0;
        uint32_t frame = 0;  // the cell's frame it is for (Atlas::frames)
        // 0.13.1, transparent texels: a second pass of the network over the alpha (ai_alpha) after the colour's,
        // whose picture waits in image meanwhile.
        bool alpha = false;
        int phase = 0;  // 0 the colour, 1 the alpha
        bool icon = false;  // 0.14: an icon of a window - the picture goes back to the driver
        uint64_t iconKey = 0;
        bool packed = false;  // 0.42: loaded from the texture pack
        uint32_t border = 0;  // past the picture's edge (ai_conv): 0 the other side, 1 zeros, 2 the edge again
        ID3D12Resource* image = nullptr;
        std::vector<Level> levels;
    };
    // A sprite cache of the game (0.13): its enlarged copy is the texture's Texture::ai, made cell by cell.
    struct Atlas {
        int levels = 0;
        std::unordered_map<uint32_t, uint32_t> frames;  // a cell's corner (y << 16 | x) -> its frames so far
    };
    struct Readback {
        ID3D12Resource* buffer = nullptr;
        UINT64 fence = 0;
        std::string name;
        int width = 0, height = 0;
        UINT pitch = 0;
        bool icon = false;  // also to the driver, as iconKey
        uint64_t iconKey = 0;
    };
    struct IoRequest {
        bool write = false;
        bool tag = false;  // an empty entry: a kind or a map (texture_cache.hpp)
        bool pack = false;  // 0.42: a read from the texture pack; width x height the game texture's (its result's / 4)
        uint32_t id = 0;
        std::string name;
        std::vector<uint32_t> argb;  // to write; for a cell's read, the cell as the game has it
        int width = 0, height = 0;
        Job cell;  // a cell's read: the job it becomes (its pixels the network's input should the read fail)
    };
    struct IoResult {
        uint32_t id = 0;
        std::string name;
        bool ok = false;
        bool pack = false;
        std::vector<uint32_t> argb;
        int width = 0, height = 0;
        Job cell;
    };

    bool loadNet(const std::string& dir, const std::string& token, int denoise, Net& net, std::string& error);
    bool createGpu(std::string& error);
    bool uploadWeights(Net& net, std::string& error);
    bool selfTest(int net, std::string& error);
    bool ensureBuffers(int width, int height);
    bool uploadInput(const Job& job);
    bool uploadAlphaInput(const Job& job);  // A in the R, G, B and A planes
    bool startAlpha(Job& job);              // the colour into a new image, then the alpha's pass
    void dispatchAlpha(const Job& job, ID3D12Resource* image, const Level& level0);
    void dropJob();                         // the running job goes, its image with it
    void trimBuffers();                     // after a big picture: its half a gigabyte of buffers goes
    double runBand(Job& job, double budget);  // MACs dispatched
    bool finish(Job& job);
    ID3D12Resource* makeImage(Job& job, std::vector<Level>& levels);  // in COPY_SOURCE, all its levels
    void readBack(const Job& job, ID3D12Resource* image, const Level& top);  // for upscaled.grf
    void noteCells(uint32_t id, Texture& t, const UploadCmd& u, const uint32_t* argb, const SpriteCell* cells,
                   uint32_t count);
    bool cellOn(uint8_t source) const;  // a frame of this sprite file's kind goes to the network (sprite_exe)
    bool makeAtlas(uint32_t id, Texture& t);
    void standIn(Texture& t, const Atlas& a, const Rect& r, const uint32_t* argb, int pitch);
    void writeRegion(Texture& t, int level, int x, int y, int w, int h, const std::vector<uint32_t>& argb);
    bool cellCurrent(const Job& job) const;
    bool finishCell(Job& job);
    bool finishIcon(Job& job);
    void layout(int width, int height, std::vector<Level>& levels, UINT64& bytes) const;
    void dispatchShuffle(const Job& job, ID3D12Resource* image, const Level& level0);
    void dispatchMips(ID3D12Resource* image, const std::vector<Level>& levels);
    void uavBarrier();
    void bindCompute();
    void dropAi(uint32_t id);
    void drop(Item& item);  // its pixels go: never its turn
    void begin(uint32_t id, Item& item, ai::Kind kind, bool later);
    void tag(const std::string& name);
    bool nextJob();
    void takeLoaded();
    void finishReadbacks();
    void request(IoRequest r);
    static DWORD WINAPI ioMain(void* self);
    void ioLoop();
    void logNow(bool final);
    // 0.42: a picture of the texture pack (pictureName) for a game texture of width x height: read on the I/O thread.
    void requestPacked(uint32_t id, const std::string& name, int width, int height, Job cell);

    Renderer::Impl& m_;
    bool ready_ = false;
    AiSettings settings_;
    std::vector<std::unique_ptr<Net>> nets_;
    int netOf_[ai::kKinds] = {-1, -1, -1, -1};  // the network of each kind; -1: off, or (0.42) not generating
    bool kindOn_[ai::kKinds] = {};              // 0.42: the kinds shown improved, from the pack or upscaled.grf
    std::string folders_[ai::kKinds];           // upscaled.grf's folder of each kind's pictures (netFolder)
    bool generate_ = false;                     // [ai] generate: the network makes what is missing
    pack::Reader pack_;                         // 0.42: uaro-textures.zip, read by the I/O thread
    int scale_ = 4;                             // the result: 4 or 2 times the game's texture
    int minSide_ = 64, maxSide_ = 512;          // the textures taken
    bool ahead_ = true;
    int features_ = 64;                         // the most of any network: its feature buffers
    std::string weakName_ = "realesr-general-wdn-x4v3";  // general's weak-denoise twin
    double budget_ = 3e8;          // multiply-adds a frame, adjusted to targetMs_
    double targetMs_ = 2.0;
    double scheduled_[2] = {0, 0};  // multiply-adds put into each slot's list (kSlots)
    ID3D12RootSignature* root_ = nullptr;
    ID3D12PipelineState* conv_ = nullptr;
    ID3D12PipelineState* shuffle_ = nullptr;
    ID3D12PipelineState* mip_ = nullptr;
    ID3D12PipelineState* alpha_ = nullptr;
    ID3D12Resource* planes_ = nullptr;  // the input: R, G, B, A planes
    ID3D12Resource* feat_[2] = {nullptr, nullptr};
    UINT64 planesBytes_ = 0, featBytes_ = 0;
    D3D12_RESOURCE_STATES planesState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    bool computeBound_ = false;  // in this list

    std::unordered_map<uint32_t, Item> items_;
    size_t keptBytes_ = 0;
    std::deque<uint32_t> queue_;  // drawn and waiting for the network
    std::deque<uint32_t> later_;  // ahead: known from earlier visits, not drawn yet
    std::deque<Job> cells_;       // sprite cache cells waiting for the network
    bool cellTurn_ = false;       // whose turn it is: a sprite frame or the rest of the world
    std::unordered_map<uint32_t, Atlas> atlases_;
    std::deque<Job> loaded_;
    std::unique_ptr<Job> job_;
    std::vector<Readback> readbacks_;
    std::unordered_set<std::string> known_;            // pictures in upscaled.grf, or on their way there
    std::unordered_map<std::string, ai::Kind> kinds_;  // texture key -> its kind tag
    std::unordered_set<std::string> mapTags_;          // the map tags in upscaled.grf or on their way
    std::string map_;
    uint32_t mapSerial_ = 1;
    // 0.44: the copies' video memory - their bytes, the budget, the frames, what was put away and brought back.
    uint64_t frame_ = 0, aiBytes_ = 0, vramBudget_ = 0, systemBudget_ = 0, lastPutAway_ = 0;
    uint32_t putAway_ = 0, broughtBack_ = 0, packAhead_ = 0;
    void putAwayOld();
    void bringBack(uint32_t id, Item& item);
    uint64_t systemMemory(uint64_t* used) const;  // DXGI's budget for the process, and what it uses

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

    uint32_t doneNetwork_ = 0, doneLoaded_ = 0, written_ = 0, donePack_ = 0;
    uint32_t cellsPack_ = 0, iconsPack_ = 0;  // 0.42: from the texture pack
    uint32_t cellsWritten_ = 0, cellsNetwork_ = 0, cellsLoaded_ = 0;  // sprite frames: by the game, by us
    std::unordered_set<uint64_t> icons_;  // the icons asked for this run
    uint32_t iconsNetwork_ = 0, iconsLoaded_ = 0;
    uint32_t seen_[6] = {};  // textures first drawn as each ai::Kind this run
    uint32_t soft_ = 0;      // textures and sprite frames of soft light alone, left as the game's (0.22.2)
    ULONGLONG lastLog_ = 0;
};

}  // namespace uagfx::gpu
