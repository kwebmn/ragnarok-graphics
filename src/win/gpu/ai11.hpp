#pragma once
#include <windows.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/ai/kinds.hpp"
#include "core/ai/ready.hpp"
#include "core/config.hpp"
#include "core/frame.hpp"
#include "core/grf/grf.hpp"
#include "core/texture_pack.hpp"
#include "win/gpu/renderer.hpp"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct IDXGIAdapter;

namespace uagfx::gpu {

// 0.47: the AI textures with Direct3D 11 - the ready pictures of the texture pack and upscaled.grf (core/ai/ready)
// drawn in place of the game's textures as Direct3D 12 draws them (ai_textures.hpp), none made: no network runs in
// the game's own process. A picture is read, decoded and given its mip levels on a thread of its own, which makes its
// texture too (the device is free-threaded); the game's thread only puts it in. The game's sprite cache gets an
// enlarged copy whose cells take their frames' pictures, the windows' icons go back to crisp text, and the copies keep
// to a share of the video memory, as with Direct3D 12.
class Ai11 {
public:
    Ai11(ID3D11Device* device, ID3D11DeviceContext* context, IDXGIAdapter* adapter);
    ~Ai11();
    Ai11(const Ai11&) = delete;
    Ai11& operator=(const Ai11&) = delete;

    bool start(const std::string& gameDir, const AiSettings& settings, std::string& error);
    // A texture's pixels as the game uploads them - level 0 of a whole one gives its key; cells: the cells of the
    // game's sprite cache that got a new frame (UploadCmd::slots).
    void noteUpload(uint32_t id, int width, int height, const UploadCmd& u, const uint32_t* argb,
                    const SpriteCell* cells, uint32_t cellCount);
    void noteWorldUse(uint32_t id, ai::Kind kind);  // drawn in the world as kind
    void noteUiUse(uint32_t id);                    // drawn in the interface: its big pictures
    void forget(uint32_t id);                       // the game released it
    // A picture the game copies into a window (Renderer::requestIcon): made larger, it goes to the sink.
    void noteIcon(uint64_t key, int width, int height, const uint32_t* argb);
    void setIconSink(Renderer::IconSink sink, void* context) {
        iconSink_ = sink;
        iconContext_ = context;
    }
    // The picture a draw of the world (ui false) or of the interface takes in place of the texture; null: the game's.
    ID3D11ShaderResourceView* copyOf(uint32_t id, bool ui) const;
    void work();  // once a frame, on the game's thread: what the thread has made goes in
    bool shown = true;  // Pause: the game's own textures

private:
    struct Item {
        enum State { Kept, Waiting, Done, Skipped, Away } state = Kept;
        std::string key;                 // the game texture's (ai::keyName)
        ai::Kind kind = ai::Kind::None;  // as first drawn, or known ahead
        int width = 0, height = 0;
        int uploads = 0;
        bool soft = false;  // soft light alone or a soft alpha (0.22.2, 0.39.2): it stays the game's
        bool ui = false;    // a picture of the interface: it stands in for the interface's draws
        uint32_t serial = 0;   // its pixels': a picture asked for older ones is not taken
        uint64_t lastUse = 0;  // the frame it was last drawn in
    };
    struct Copy {
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        bool world = true, ui = false;  // the draws it stands in for
        uint64_t bytes = 0;             // all its levels (the sprite caches' copies are not counted)
    };
    struct Atlas {  // the game's sprite cache: its copy (copies_) made cell by cell
        int width = 0, height = 0;  // the game texture's
        std::unordered_map<uint32_t, uint32_t> frames;  // a cell's corner (y << 16 | x) -> its frames so far
    };
    enum class Want : uint8_t { Texture, Cell, Icon };
    struct Request {
        Want want = Want::Texture;
        uint32_t id = 0;
        uint32_t serial = 0;  // a texture's Item::serial, a cell's frame
        std::string name;
        bool pack = false;
        int width = 0, height = 0;  // the game texture's, the cell's, the icon's
        int cellX = 0, cellY = 0;
        uint64_t iconKey = 0;
    };
    struct Level {
        int width = 0, height = 0;
        std::vector<uint32_t> argb;
    };
    struct Result {
        Request request;
        bool ok = false;
        ID3D11Texture2D* texture = nullptr;  // a texture's, made on the thread
        ID3D11ShaderResourceView* srv = nullptr;
        uint64_t bytes = 0;
        std::vector<Level> levels;  // a cell's (the sprite cache's levels), an icon's one
    };

    void begin(uint32_t id, Item& item, ai::Kind kind);
    void bringBack(uint32_t id, Item& item);
    void dropCopy(uint32_t id);
    void putAwayOld();
    void noteCells(uint32_t id, int width, int height, const UploadCmd& u, const uint32_t* argb,
                   const SpriteCell* cells, uint32_t count);
    bool cellOn(uint8_t source) const;
    bool makeAtlas(uint32_t id, int width, int height);
    void standIn(uint32_t id, const Atlas& a, const Rect& r, const uint32_t* argb, int pitch);
    void writeLevel(uint32_t id, const Atlas& a, int level, int x, int y, int w, int h, const uint32_t* argb);
    bool cellCurrent(const Request& r) const;
    void take(Result& r);
    void request(Request r);
    static DWORD WINAPI ioMain(void* self);
    void ioLoop();
    void serve(const Request& r, Result& out);  // on the thread
    bool load(const Request& r, std::vector<uint32_t>& argb, int& width, int& height);  // on the thread
    uint64_t videoMemory(uint64_t* used) const;
    void logNow(bool final);

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* ctx_ = nullptr;
    IDXGIAdapter* adapter_ = nullptr;
    bool ready_ = false;
    AiSettings settings_;
    std::unique_ptr<ai::ReadyPictures> pictures_;
    int scale_ = 4;
    int minSide_ = 16, maxSide_ = 512;
    int atlasLevels_ = 4;
    Renderer::IconSink iconSink_ = nullptr;
    void* iconContext_ = nullptr;

    std::unordered_map<uint32_t, Item> items_;
    std::unordered_map<uint32_t, Copy> copies_;
    std::unordered_map<uint32_t, Atlas> atlases_;
    std::unordered_set<uint64_t> icons_;  // asked for this run
    std::vector<uint64_t> noPicture_;     // none there: crisp text is told so at the next frame (xBR's then)
    std::deque<Result> cellsWaiting_;     // read, put into their caches a few a frame
    uint32_t serial_ = 0;
    uint64_t frame_ = 0, aiBytes_ = 0, vramBudget_ = 0, systemBudget_ = 0, lastPutAway_ = 0;
    uint32_t putAway_ = 0, broughtBack_ = 0;

    pack::Reader pack_;     // read by the thread; has() on either
    grf::Archive archive_;  // the thread's once it runs
    HANDLE thread_ = nullptr;
    HANDLE wake_ = nullptr;
    CRITICAL_SECTION lock_{};
    bool lockMade_ = false;
    bool stop_ = false;
    std::deque<Request> in_;
    std::deque<Result> out_;
    std::unordered_set<uint32_t> gone_;  // textures released while their pictures were asked for

    uint32_t texturesPack_ = 0, texturesGrf_ = 0, cellsWritten_ = 0, cellsPack_ = 0, cellsGrf_ = 0;
    uint32_t iconsPack_ = 0, iconsGrf_ = 0, iconsNone_ = 0, failed_ = 0, soft_ = 0, waiting_ = 0;
    uint32_t seen_[6] = {};  // textures first drawn as each ai::Kind this run
    ULONGLONG lastLog_ = 0;
};

}  // namespace uagfx::gpu
