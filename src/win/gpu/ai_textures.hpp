#pragma once
#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/ai/cell_grid.hpp"
#include "core/ai/kinds.hpp"
#include "core/ai/ready.hpp"
#include "core/ai/rrdb.hpp"
#include "core/ai/srvgg.hpp"
#include "core/config.hpp"
#include "core/frame.hpp"
#include "core/studio_link.hpp"
#include "core/texture_pack.hpp"
#include "win/gpu/missing_dump.hpp"
#include "win/gpu/renderer.hpp"

namespace uagfx::gpu {

struct Texture;
const char* aiShaderSource();  // ai_shaders.cpp

// Neural textures (experimental, [ai], uaro-render64.exe): world textures enlarged by Real-ESRGAN's compact networks
// (core/ai/srvgg) on this GPU, a slice of the work every frame, and drawn instead of the game's own once done. The
// kind of a texture (core/ai/kinds: the ground, objects, sprites, effects) comes from the draw that first shows it
// and chooses whether and through which network it goes. 0.48: the pictures come from the texture pack
// (uaro-textures.zip, core/ai/ready) alone - upscaled.grf (0.11-0.47) is no more; with [ai] generate (a debugging aid)
// what the pack has not is made and written into the pack, named by the game texture's pixels, its index noting the
// kinds and the maps they were seen as and on, for the settings app. The game knows nothing of it.
class AiTextures {
public:
    explicit AiTextures(Renderer::Impl& m) : m_(m) {
        std::fill(std::begin(netOf_), std::end(netOf_), -1);
        std::fill(std::begin(groupNet_), std::end(groupNet_), -1);
    }
    ~AiTextures();
    AiTextures(const AiTextures&) = delete;
    AiTextures& operator=(const AiTextures&) = delete;

    // The models, the shaders, a self-test of each against the CPU's network, the texture pack. false: the feature
    // stays off. The amount of work a frame follows the GPU time it takes (observe) to the settings' budget.
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
    // With the frame's list open, before its present: a slice. 1.5.4: foreground false - the game behind other
    // windows draws 15 frames a second (save_power), so a generating network takes up to kBackgroundMs of each.
    void work(bool foreground = true);
    bool shown = true;                              // Pause: the game's own textures
    uint8_t kindsShown = 0xFF;  // 2.2.1: the kinds shown ([ai] as the file has it now; bits as ai::Kind)
    bool kindShown(uint8_t kind) const { return kind >= 8 || (kindsShown >> kind & 1) != 0; }
    uint8_t spritesShown = 0xFF;  // 2.2.2: the sprites' kinds shown (aiSpritesShown's bits)
    // 2.2.2: a sprite cache's cell holding `source` (sprite_exe::Source) drawn from the copy (aiCellShown)
    bool cellShown(uint8_t source) const { return aiCellShown(source, kindsShown, spritesShown); }
    // 2.3, uaRO Studio's X-ray: how a texture is shown (studio::How); false: not one of the AI's (a lightmap, a
    // sprite cache, a texture outside the sides).
    // 2.3, uaRO Studio (L2): the texture picked under the cursor into %LOCALAPPDATA%\uaroGraphics\studio\link\pick.txt
    // and the studio told (its event); 0: nothing of the AI's was there. toggleHidden: that texture's picture off in
    // the game, or on again (A/B of one texture).
    void writePick(uint32_t id, int x, int y);
    // 2.3 (T7): a texture's material scales ('#' lines of the pack's index); 1, 1, 1 when it has none
    void materialScales(uint32_t id, pack::MaterialScales& out) const {
        const auto it = items_.find(id);
        out = pack::MaterialScales();
        if (it != items_.end() && pictures_) pictures_->materialOf(it->second.key, out);
    }
    void toggleHidden();
    bool howOf(uint32_t id, uint8_t& how) const {
        const auto it = items_.find(id);
        if (it == items_.end()) return false;
        how = it->second.how;
        return true;
    }

    // 0.43, the pack builder (Renderer::buildPack): the networks of the settings' kinds - no pack.
    bool startForPack(const std::string& gameDir, const AiSettings& settings, std::string& error);
    // 2.3, uaRO Studio's engine: the compute pipelines alone; a model's network loaded when it is first asked for
    // (netForModel: the general one at the denoise asked, -1 the settings'), self-tested, its file closed after.
    bool startForStudio(const std::string& gameDir, std::string& error);
    int netForModel(const std::string& token, int denoise, std::string& error);
    int netFor(ai::Kind kind) const { return int(kind) < ai::kKinds ? netOf_[int(kind)] : -1; }
    // 1.6: the network of a group of pictures (its own model); -1 none.
    int netForGroup(ai::Group g) const { return int(g) < ai::kGroups ? groupNet_[int(g)] : -1; }
    // 1.6.3: a picture's - the small pictures' model below [ai] small_side, else its group's.
    int netForPicture(ai::Group g, int width, int height) const {
        const int group = netForGroup(g);
        if (group < 0 || smallNet_ < 0 || width >= settings_.smallSide || height >= settings_.smallSide) return group;
        return smallNet_;
    }
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
        ai::Model model;    // 1.5, RRDB: rrdb.net - its convolutions and weights
        bool rrdb = false;  // 1.5: anime6b, Real-ESRGAN's RRDBNet: tiles through its steps (stepRrdb)
        ai::RrdbModel rrdbModel;
        std::vector<ai::Op> program;
        ID3D12Resource* weights = nullptr;
    };
    struct Item {
        // Away (0.44): done, its copy put away over the video memory's budget - drawn again, it comes back from the
        // texture pack.
        enum State { Kept, Waiting, Done, Skipped, Away } state = Kept;
        std::string key;                 // the game texture's (ai::keyName)
        ai::Kind kind = ai::Kind::None;  // as first drawn, or its kind in the pack's index (ahead)
        int net = -1;
        std::string name;  // its picture in the pack, once the kind is known
        int width = 0, height = 0;
        std::vector<uint32_t> argb;  // until done
        int uploads = 0;
        uint32_t mapSerial = 0;  // the map whose tag it has been seen to
        bool later = false;      // in later_: done ahead, before it is drawn
        bool loading = false;    // its picture is being read from the pack
        bool aheadRead = false;  // 0.66: that read asked for ahead - drawn meanwhile, it goes first
        bool ui = false;         // a picture of the interface: it stands in for interface draws, not the world's
        uint64_t lastUse = 0;    // 0.44: the frame it was last drawn in (frame_)
        uint64_t bytes = 0;      // its copy's in video memory (Texture::ai), all its levels
        ai::Group group = ai::Group::World;  // 1.6
        uint8_t how = uint8_t(studio::How::Missing);  // 2.3: how it is shown (the map's report, the X-ray)
        uint64_t counted = ~0ull;                     // 2.3: the frame it was last counted in the report
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
        bool loaded = false;        // from the texture pack: pixels holds the finished picture
        std::vector<uint32_t> pixels;
        int conv = -1;  // the next convolution; -1 before the input is on the GPU
        int row = 0;    // its next band
        ai::Kind kind = ai::Kind::None;  // 0.48: for the pack's index when it is written there
        bool cell = false;  // a cell of a sprite cache: the picture goes into the cache's enlarged copy
        int cellX = 0, cellY = 0;
        uint32_t frame = 0;  // the cell's frame it is for (Atlas::frames)
        // 0.13.1, transparent texels: a second pass of the network over the alpha (ai_alpha) after the colour's,
        // whose picture waits in image meanwhile.
        bool alpha = false;
        int phase = 0;  // 0 the colour, 1 the alpha
        bool icon = false;  // 0.14: an icon of a window - the picture goes back to the driver
        uint64_t iconKey = 0;
        uint32_t border = 0;  // past the picture's edge (ai_conv): 0 the other side, 1 zeros, 2 the edge again
        ai::Group group = ai::Group::World;  // 1.6: whose model makes it, the index's word
        ID3D12Resource* image = nullptr;
        std::vector<Level> levels;
        // 1.5, RRDB: the picture's planes (ai::prepareInput), its tiles, the one going through, done with all.
        std::vector<float> source;
        std::vector<ai::Tile> tiles;
        size_t tile = 0;
        bool rrdbDone = false;
    };
    // A sprite cache of the game (0.13): its enlarged copy is the texture's Texture::ai, made cell by cell.
    struct Atlas {
        int levels = 0;
        std::unordered_map<uint32_t, uint32_t> frames;  // a cell's corner (y << 16 | x) -> its frames so far
        std::unordered_map<uint32_t, uint8_t> sources;  // a cell's corner -> its last frame's sprite_exe::Source
        ai::CellGrid grid;                              // 0.49.3: the game texture as it is, cut into its cells
    };
    struct Readback {
        ID3D12Resource* buffer = nullptr;
        UINT64 fence = 0;
        std::string name;
        ai::Kind kind = ai::Kind::None;
        std::string model, group;  // 1.6: for the index
        int width = 0, height = 0;
        UINT pitch = 0;
        bool icon = false;  // also to the driver, as iconKey
        uint64_t iconKey = 0;
    };
    // 0.48: the I/O thread's work - a picture of the pack read (width x height the game texture's: its result's / 4),
    // one made written into the pack, or the index told of a kind and a map a picture was seen as and on.
    struct IoRequest {
        // 2.3 Reload: the pack's directory and index read again (uaRO Studio changed it), name the keys changed
        enum Type : uint8_t { Read, Write, Note, Reload } type = Read;
        uint32_t id = 0;
        std::string name;
        std::vector<uint32_t> argb;  // to write
        int width = 0, height = 0;
        ai::Kind kind = ai::Kind::None;  // Write, Note
        std::string map;                 // Write, Note: "" none
        std::string model, group;        // 1.6, Write: the index's words
        Job cell;  // a cell's or an icon's read: the job it becomes (its pixels the network's input should it fail)
        // 0.66: read in this order - 0 a sprite frame or an icon, 1 a texture in view, 2 one known ahead (a map's
        // hundreds as it loads), 3 a picture made, to write
        uint8_t rank = 1;
        ULONGLONG asked = 0;  // 0.66: when (the log's waits)
    };
    struct IoResult {
        uint32_t id = 0;
        std::string name;
        bool ok = false;
        std::shared_ptr<pack::Reader> reloaded;  // 2.3: the pack opened again, its index in `text`, its keys in name
        std::string text;
        std::vector<uint32_t> argb;
        int width = 0, height = 0;
        Job cell;
        ULONGLONG asked = 0;  // 0.66: its request's
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
    // 1.5, RRDB: the job's next piece of work - its image made, a tile's input up, a band of a step, a tile placed;
    // MACs dispatched. failed: no GPU memory for it.
    double stepRrdb(Job& job, double budget, bool& failed);
    bool ensureScratch(UINT64 words);
    bool selfTestRrdb(int net, std::string& error);
    // 1.5.5: the texture pack's builder (enlarge) through RRDB - tile by tile, a submission every so much work.
    bool enlargeRrdb(int net, const uint32_t* argb, int w, int h, uint32_t border, std::vector<uint32_t>& out,
                     std::string& error);
    // 1.5.1: the steps one by one, each step's output read back and compared with ai::convReference.
    bool traceRrdb(int net, const std::vector<uint32_t>& argb, int width, int height, std::string& error);
    void dispatchRrdbConv(const Net& net, const uint32_t k[16]);
    bool finish(Job& job);
    ID3D12Resource* makeImage(Job& job, std::vector<Level>& levels);  // in COPY_SOURCE, all its levels
    void readBack(const Job& job, ID3D12Resource* image, const Level& top);  // for the pack (0.48)
    void noteCells(uint32_t id, Texture& t, const UploadCmd& u, const uint32_t* argb, const SpriteCell* cells,
                   uint32_t count);
    bool cellOn(uint8_t source) const;
    // A frame in the cell c of the cache (argb its top left texel): seen - through the game's call; else found changed.
    void cellWritten(uint32_t id, Texture& t, Atlas& a, int net, const Rect& c, const uint32_t* argb, int pitch,
                     uint8_t source, bool seen);  // a frame of this sprite file's kind goes to the network (sprite_exe)
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
    void note(const std::string& name, ai::Kind kind);  // 0.48: its kind and the map, into the pack's index
    // 2.3, uaRO Studio's report of a map (L3): each texture drawn, how it was shown and in how many frames - written
    // to uaro-graphics\seen\<map>.txt when the map changes and at the end, the last 20 maps kept.
    void noteSeen(Item& item);
    void writeSeen();
    bool nextJob();
    void takeLoaded();
    void finishReadbacks();
    void request(IoRequest r);
    static DWORD WINAPI ioMain(void* self);
    void ioLoop();
    bool ioRead(const IoRequest& r, IoResult& res);  // the I/O thread's
    bool ioCheckpoint();                             // the I/O thread's: the index and the central directory
    std::string packIndex() const;                   // the I/O thread's
    void logNow(bool final);
    // 0.42: a picture of the texture pack (pictureName) for a game texture of width x height: read on the I/O thread.
    void requestPacked(uint32_t id, const std::string& name, int width, int height, Job cell, uint8_t rank = 1);
    bool nextIo(IoRequest& r);  // 0.66: under ioLock_: the most urgent request, taken out

    Renderer::Impl& m_;
    bool ready_ = false;
    AiSettings settings_;
    std::vector<std::unique_ptr<Net>> nets_;
    // The network of each kind; -1: off, or (0.42) not generating. 0.47.1: every kind -1 at first (the constructor) -
    // the icons' was 0 since 0.14: the first network made them, and without one (0.42) the start read nets_[0] of
    // none and uaro-render64.exe fell (0xc0000005) whenever the AI textures were on.
    int netOf_[ai::kKinds];
    int groupNet_[ai::kGroups];  // 1.6
    int smallNet_ = -1;          // 1.6.3: [ai] small_model with small_pictures on
    // 1.6: the network of a model word, loaded once (the same model for several groups is one network); -1, error.
    int netOfToken(const std::string& dir, const std::string& token, std::string& error);
    std::string modelDir_;  // 2.3: the studio's engine - where netForModel finds the models
    std::string seenDir_;   // 2.3: uaro-graphics\seen ("" no reports: the pack builder, the studio's engine)
    std::string picked_;    // 2.3: the key picked last (Ctrl+Shift+F8)
    std::unordered_set<std::string> hidden_;  // 2.3: keys whose pictures are off (Ctrl+Shift+F7)
    std::unordered_map<std::string, studio::Seen> seenOnMap_;
    bool kindOn_[ai::kKinds] = {};              // 0.42: the kinds shown improved, from the pack
    bool generate_ = false;                     // [ai] generate: the network makes what is missing
    // 0.42: uaro-textures.zip, read by the I/O thread; 2.3: shared - opened again there when uaRO Studio changes it,
    // then swapped in here under ioLock_ (each read takes it under the lock)
    std::shared_ptr<pack::Reader> pack_ = std::make_shared<pack::Reader>();
    HANDLE changedEvent_ = nullptr;  // 2.3: Local\uaro-studio-changed, set by the studio after it wrote the pack
    void studioChanged();             // 2.3: the event set - the pack asked to be read again
    void reloaded(IoResult& r);       // 2.3: the pack read again - the keys changed taken from it anew
    std::unique_ptr<ai::ReadyPictures> pictures_;  // 0.48: the pack's pictures and kinds, looked up here
    MissingDump dump_;                             // 0.49.1
    int scale_ = 4;                             // the result: 4 or 2 times the game's texture
    int minSide_ = 64, maxSide_ = 512;          // the textures taken
    bool ahead_ = true;
    int features_ = 64;                         // the most of any network: its feature buffers
    std::string weakName_ = "realesr-general-wdn-x4v3";  // general's weak-denoise twin
    double budget_ = 3e8;          // multiply-adds a frame, adjusted to targetMs_
    double targetMs_ = 2.0;
    bool background_ = false;  // 1.5.4: the last frame was drawn with the game behind other windows
    double scheduled_[2] = {0, 0};  // multiply-adds put into each slot's list (kSlots)
    ID3D12RootSignature* root_ = nullptr;
    ID3D12PipelineState* conv_ = nullptr;
    ID3D12PipelineState* shuffle_ = nullptr;
    ID3D12PipelineState* mip_ = nullptr;
    ID3D12PipelineState* alpha_ = nullptr;
    ID3D12PipelineState* place_ = nullptr;  // 1.5: a tile of RRDB into the image
    ID3D12Resource* scratch_ = nullptr;     // 1.5: RRDB's planes (ai::Buf), one tile's
    UINT64 scratchBytes_ = 0;
    D3D12_RESOURCE_STATES scratchState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
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
    std::unordered_set<std::string> noted_;  // a picture and a map the pack's index was told of (note)
    std::string map_;
    uint32_t mapSerial_ = 1;
    // 0.44: the copies' video memory - their bytes, the budget, the frames, what was put away and brought back.
    uint64_t frame_ = 0, aiBytes_ = 0, vramBudget_ = 0, systemBudget_ = 0, lastPutAway_ = 0;
    uint32_t putAway_ = 0, broughtBack_ = 0;
    void putAwayOld();
    void bringBack(uint32_t id, Item& item);
    uint64_t systemMemory(uint64_t* used) const;  // DXGI's budget for the process, and what it uses

    // 0.48: the pack written by the generating game - by its I/O thread, which alone touches these once it runs.
    std::string packPath_;
    bool writable_ = false;  // generating, and no builder or other game writes the pack (pack::kWriterMutex)
    HANDLE writerMutex_ = nullptr;
    pack::Writer writer_;   // opened at the first picture made (it drops the pack's directory until a checkpoint)
    pack::Reader fresh_;    // the pack again after each checkpoint: the pictures made this run
    std::unordered_map<std::string, std::vector<uint8_t>> unsaved_;  // written since the last checkpoint (WebP)
    pack::Index index_;     // the pack's, with what this run made and saw
    bool indexChanged_ = false;
    std::atomic<uint32_t> written_{0};  // pictures written into the pack (the log reads it)
    HANDLE ioThread_ = nullptr;
    HANDLE ioWake_ = nullptr;
    CRITICAL_SECTION ioLock_{};
    bool ioLockMade_ = false;
    bool ioStop_ = false;
    std::deque<IoRequest> ioIn_;
    std::unordered_set<uint32_t> urgent_;  // 0.66: textures read ahead and in view since: their turn first
    std::deque<IoResult> ioOut_;

    uint32_t doneNetwork_ = 0, donePack_ = 0;
    uint32_t cellsPack_ = 0, iconsPack_ = 0;  // 0.42: from the texture pack
    uint32_t unseen_ = 0, unseenFound_ = 0;  // 0.49.3: frames found changed past the game's call, of them in the pack
    uint32_t cellsWritten_ = 0, cellsNetwork_ = 0;
    ULONGLONG cellWait_ = 0, cellWaitMost_ = 0;  // 0.66: the frames' pictures from asked to in, all and the longest
    uint32_t cellWaits_ = 0;  // sprite frames: by the game, by the network
    std::unordered_set<uint64_t> icons_;  // the icons asked for this run
    uint32_t iconsNetwork_ = 0;
    // 0.48 (in place of the settings app's "Check the keys"): what the pack had no picture of, by kind; frames; icons.
    uint32_t missing_[ai::kKinds] = {}, cellsMissing_ = 0, iconsMissing_ = 0;
    uint32_t seen_[6] = {};  // textures first drawn as each ai::Kind this run
    uint32_t soft_ = 0;      // textures and sprite frames of soft light alone, left as the game's (0.22.2)
    ULONGLONG lastLog_ = 0;
};

}  // namespace uagfx::gpu
