#include "win/gpu/map_world.hpp"

#include <algorithm>
#include <deque>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "core/config.hpp"
#include "core/full_colour.hpp"
#include "core/gnd.hpp"
#include "core/grf/grf.hpp"
#include "core/jpeg_image.hpp"
#include "core/outdoors.hpp"
#include "core/outskirts.hpp"
#include "core/pit_walls.hpp"
#include "core/own_ground.hpp"
#include "core/own_models.hpp"
#include "core/room_blocks.hpp"
#include "core/pack_source.hpp"
#include "core/layout.hpp"
#include "core/updates.hpp"
#include "core/zip.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

}  // namespace

struct MapWorld::Shared {
    CRITICAL_SECTION lock;
    HANDLE wake = nullptr;
    bool stop = false;
    std::string gameDir;   // set before the first map
    std::string wanted;    // the map to read; the thread takes the latest
    bool asked = false;
    std::string doneName;  // a finished read
    rsw::World done;
    ground_heights::Ground doneGround;  // 2.6.18: its .gnd's heights (none: not read)
    std::vector<grass_beds::Box> doneBeds;  // 3.0.1-beta.3: its flower beds' tops
    std::vector<cave_vault::Post> donePosts;  // 3.0.20-beta.46: a dungeon's posts
    float doneWalls = 0.0f;                   // 3.0.20-beta.52: indoors, the walls' height by the models
    float doneWallsColour[3] = {0.0f, 0.0f, 0.0f};  // 3.0.20-beta.54: and their upper part's colour
    bool doneWallsColourKnown = false;
    MapWorld::VaultRock doneRock;             // 3.0.20-beta.54: a dungeon's own rock for its vault
    rooms::Map doneRooms;                     // 3.0.20-beta.57: a house's rooms
    bool ownWanted = false;                 // 3.0.4-beta.10: the own ground read with the map
    bool roomWalls = true;                  // 3.0.20-beta.71: [view3d] room_walls
    bool wallsWanted = false;               // 3.0.23-beta.6: the own ground for its walls alone
    bool outskirtsRelief = true;            // 3.0.22-beta.16: off under the horizon panorama
    std::unique_ptr<OwnGround> doneOwn;
    bool doneOk = false;
    bool doneReady = false;
    uint32_t askSerial = 0, doneSerial = 0;  // 3.0.6-beta.3: each ask's number, the finished read's
};

MapWorld::MapWorld() : shared_(new Shared) {
    InitializeCriticalSection(&shared_->lock);
    shared_->wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

MapWorld::~MapWorld() {
    if (thread_) {
        EnterCriticalSection(&shared_->lock);
        shared_->stop = true;
        LeaveCriticalSection(&shared_->lock);
        SetEvent(shared_->wake);
        WaitForSingleObject(thread_, INFINITE);  // a read under way ends first: the thread uses what goes next
        CloseHandle(thread_);
    }
    if (shared_->wake) CloseHandle(shared_->wake);
    DeleteCriticalSection(&shared_->lock);
}

void MapWorld::setGameDir(const std::string& dir) {
    gameDir_ = dir;
    EnterCriticalSection(&shared_->lock);
    shared_->gameDir = dir;
    LeaveCriticalSection(&shared_->lock);
    // The axes found in an earlier run.
    std::vector<uint8_t> b;
    if (win::gamefiles::readWhole(dir + "\\uaro-graphics\\map-axes.txt", b)) {
        int x = 0, z = 0;
        const std::string text(b.begin(), b.end());
        if (std::sscanf(text.c_str(), "x %d z %d", &x, &z) == 2 && (x == 1 || x == -1) && (z == 1 || z == -1)) {
            axes_ = map::Axes{int8_t(x), 1, int8_t(z)};
            LOG_INFO("map objects: the maps' axes x %+d, z %+d (uaro-graphics\\map-axes.txt)", x, z);
        }
    }
}

void MapWorld::setMap(const std::string& name) {
    if (name == shown_) return;
    shown_ = name;
    ready_ = false;
    world_ = rsw::World{};
    ground_ = ground_heights::Ground{};
    beds_.clear();
    posts_.clear();
    walls_ = 0.0f;
    wallsColourKnown_ = false;
    rock_ = VaultRock{};
    rooms_ = rooms::Map{};
    placed_.clear();
    lamps_.clear();
    seen_.clear();
    own_.reset();
    ask(name);
}

void MapWorld::ask(const std::string& name) {
    if (name.empty() || gameDir_.empty()) return;
    EnterCriticalSection(&shared_->lock);
    shared_->wanted = name;
    shared_->asked = true;
    asked_ = ++shared_->askSerial;
    LeaveCriticalSection(&shared_->lock);
    if (!thread_) thread_ = CreateThread(nullptr, 0, &MapWorld::threadMain, this, 0, nullptr);
    SetEvent(shared_->wake);
}

void MapWorld::setRoomWalls(bool wanted) {
    if (wanted == roomWalls_) return;
    roomWalls_ = wanted;
    EnterCriticalSection(&shared_->lock);
    shared_->roomWalls = wanted;
    LeaveCriticalSection(&shared_->lock);
}

void MapWorld::setOutskirtsRelief(bool wanted) {
    if (wanted == outskirtsRelief_) return;
    outskirtsRelief_ = wanted;
    EnterCriticalSection(&shared_->lock);
    shared_->outskirtsRelief = wanted;
    LeaveCriticalSection(&shared_->lock);
}

void MapWorld::setOwnGround(bool wanted) {
    if (wanted == ownWanted_) return;
    ownWanted_ = wanted;
    EnterCriticalSection(&shared_->lock);
    shared_->ownWanted = wanted;
    LeaveCriticalSection(&shared_->lock);
    if (!wanted) {
        own_.reset();
        if (wallsWanted_) ask(shown_);  // 3.0.23-beta.6: read again for its walls alone
        return;
    }
    if (!own_ || own_->wallsOnly) ask(shown_);  // read again, with it (whole)
}

void MapWorld::setWallsWanted(bool wanted) {
    if (wanted == wallsWanted_) return;
    wallsWanted_ = wanted;
    EnterCriticalSection(&shared_->lock);
    shared_->wallsWanted = wanted;
    LeaveCriticalSection(&shared_->lock);
    if (wanted && !own_ && !ownWanted_) ask(shown_);  // read again for its walls (none read for this map yet)
}

void MapWorld::ownPicturesUp() {
    if (!own_) return;
    for (OwnGround::Picture& p : own_->textures) std::vector<std::vector<uint32_t>>().swap(p.levels);
    for (OwnGround::Picture& p : own_->modelTextures) std::vector<std::vector<uint32_t>>().swap(p.levels);
    for (OwnGround::Picture& p : own_->waterPictures) std::vector<std::vector<uint32_t>>().swap(p.levels);
    // 3.0.20-beta.50: the lightmaps kept where there is room (the 64-bit renderer) - the ground's light without its
    // painted shadows is made of them when the 3D view's real shadows are turned on, any time
    if (sizeof(void*) == 4) std::vector<uint32_t>().swap(own_->atlas.argb);
}

void MapWorld::poll() {
    EnterCriticalSection(&shared_->lock);
    const bool fresh = shared_->doneReady && shared_->doneName == shown_;
    if (shared_->doneReady && shared_->doneName == shown_) {
        world_ = std::move(shared_->done);
        ground_ = std::move(shared_->doneGround);
        beds_ = std::move(shared_->doneBeds);
        posts_ = std::move(shared_->donePosts);
        walls_ = shared_->doneWalls;
        std::copy(shared_->doneWallsColour, shared_->doneWallsColour + 3, wallsColour_);
        wallsColourKnown_ = shared_->doneWallsColourKnown;
        rock_ = std::move(shared_->doneRock);
        rooms_ = std::move(shared_->doneRooms);
        own_ = std::move(shared_->doneOwn);  // 3.0.4-beta.10
        if (!ownWanted_ && !(wallsWanted_ && own_ && own_->wallsOnly)) own_.reset();  // read before it was turned off
        if (own_) own_->serial = ++ownSerial_;
        ready_ = shared_->doneOk;
        ++reads_;
        taken_ = shared_->doneSerial;
    }
    shared_->doneReady = shared_->doneReady && !fresh;
    LeaveCriticalSection(&shared_->lock);
    if (!fresh || !ready_) return;
    placed_.clear();
    std::vector<map::Point> torches;  // the game's and those it does not show (red, green, purple)
    for (const rsw::Effect& e : world_.effects) {
        if (e.id == 47) placed_.push_back(map::Point{e.pos[0], e.pos[1], e.pos[2]});
        if (e.id == 47 || e.id == 690 || e.id == 691 || e.id == 696)
            torches.push_back(map::Point{e.pos[0], e.pos[1], e.pos[2]});
    }
    lamps_.clear();
    for (const rsw::Light& l : world_.lights) {
        bool onTorch = false;
        for (const map::Point& t : torches)
            onTorch = onTorch || std::hypot(l.pos[0] - t.x, l.pos[2] - t.z) < 20.0f;
        if (!onTorch) lamps_.push_back(l);
    }
    tryAxes();
}

bool MapWorld::groundAt(float x, float z, float& y) const {
    const ground_heights::Ground* g = ground();
    if (!g || !axes_.known()) return false;
    const map::Point at = map::toWorld(axes_, map::Point{x, 0.0f, z});  // the signs: the file's place, and back
    float h = 0.0f;
    if (!ground_heights::heightAt(*g, at.x, at.z, h)) return false;
    y = float(axes_.y) * h;
    return true;
}

bool MapWorld::laidAt(float x, float z) const {
    const ground_heights::Ground* g = ground();
    if (!g || !axes_.known()) return false;
    const map::Point at = map::toWorld(axes_, map::Point{x, 0.0f, z});
    return ground_heights::laidAt(*g, at.x, at.z);
}

bool MapWorld::insideAt(float x, float z) const {
    if (!hallsKnown()) return false;
    const map::Point at = map::toWorld(axes_, map::Point{x, 0.0f, z});
    return ground_heights::insideAt(ground_, at.x, at.z);
}

bool MapWorld::bareAt(float x, float z) const {
    if (!bareKnown()) return false;
    const map::Point at = map::toWorld(axes_.known() ? axes_ : map::Axes{1, 1, 1}, map::Point{x, 0.0f, z});
    return ground_heights::bareAt(ground_, at.x, at.z);
}

void MapWorld::noteTorch(const map::Point& world) {
    if (axes_.known() || seen_.size() >= 64) return;
    for (const map::Point& s : seen_)  // the same torch again
        if (std::fabs(s.x - world.x) < 3.0f && std::fabs(s.z - world.z) < 3.0f) return;
    seen_.push_back(world);
    tryAxes();
}

void MapWorld::tryAxes() {
    if (axes_.known() || placed_.size() < 3 || seen_.size() < 4) return;
    float spread = 0.0f;
    const map::Axes a = map::fitAxes(placed_, seen_, &spread);
    if (!a.known()) return;
    axes_ = a;
    LOG_INFO("map objects: the maps' axes found on %s: x %+d, z %+d (%u torches, %.1f units off)", shown_.c_str(),
             int(a.x), int(a.z), unsigned(seen_.size()), spread);
    seen_.clear();
    std::FILE* f = std::fopen((gameDir_ + "\\uaro-graphics\\map-axes.txt").c_str(), "wb");
    if (f) {
        std::fprintf(f, "x %d z %d\r\n", int(a.x), int(a.z));
        std::fclose(f);
    }
}

DWORD WINAPI MapWorld::threadMain(void* self) {
    static_cast<MapWorld*>(self)->loop();
    return 0;
}

void MapWorld::loop() {
    struct Archive {
        std::string path;
        bool grf = false;
        bool tried = false;
        bool ok = false;
        zip::Index zip;
        grf::Archive g;
    };
    std::vector<std::unique_ptr<Archive>> archives;
    std::string listedFor;
    for (;;) {
        WaitForSingleObject(shared_->wake, INFINITE);
        EnterCriticalSection(&shared_->lock);
        const bool stop = shared_->stop;
        const bool asked = shared_->asked;
        const std::string name = shared_->wanted, dir = shared_->gameDir;
        const uint32_t serial = shared_->askSerial;
        shared_->asked = false;
        LeaveCriticalSection(&shared_->lock);
        if (stop) return;
        if (!asked || name.empty() || dir.empty()) continue;
        if (listedFor != dir) {  // DATA.INI's archives, opened when first looked in
            listedFor = dir;
            archives.clear();
            for (const std::string& file : win::gamefiles::archives(dir)) {
                auto a = std::make_unique<Archive>();
                a->path = file.find(':') != std::string::npos ? file : dir + "\\" + file;
                const std::string ext = lower(file.size() > 4 ? file.substr(file.size() - 4) : "");
                if (ext != ".grf" && ext != ".zip" && ext != ".gpf") continue;
                a->grf = ext != ".zip";
                archives.push_back(std::move(a));
            }
        }
        const std::string entry = "data\\" + name + ".rsw";
        std::vector<uint8_t> bytes;
        std::string from, error;
        if (win::gamefiles::readWhole(dir + "\\" + entry, bytes)) from = "the data folder";
        // an archive opened when first looked in (2.0: also for the models, which may lie in a later one)
        const auto open = [&](Archive& a) {
            if (a.tried) return a.ok;
            a.tried = true;
            const auto maps = [](const std::string& n) {  // 2.0: and the models, for the trees
                return n.size() > 4 && (n.compare(n.size() - 4, 4, ".rsw") == 0 ||
                                        n.compare(n.size() - 4, 4, ".rsm") == 0 ||
                                        n.compare(n.size() - 4, 4, ".gnd") == 0 ||  // 2.6.18: its ground
                                        (n.compare(0, 13, "data\\texture\\") == 0 &&  // 3.0.4-beta.10: its pictures
                                         n.compare(n.size() - 4, 4, ".bmp") == 0));
            };
            std::string why;
            a.ok = a.grf ? a.g.openToRead(a.path, why) : a.zip.open(a.path, maps, why);  // 2.3.1: reading alone
            if (!a.ok) LOG_INFO("map objects: %s cannot be read (%s)", a.path.c_str(), why.c_str());
            return a.ok;
        };
        for (size_t i = 0; i < archives.size() && from.empty(); ++i) {
            Archive& a = *archives[i];
            open(a);
            if (!a.ok) continue;
            if (a.grf ? a.g.find(entry) && a.g.read(entry, bytes, error) : a.zip.has(entry) && a.zip.read(entry, bytes, error))
                from = a.path.substr(a.path.find_last_of('\\') + 1);
        }
        rsw::World w;
        bool ok = false;
        if (from.empty()) {
            LOG_INFO("map objects: %s.rsw is in neither the data folder nor DATA.INI's archives", name.c_str());
        } else if (!rsw::parse(bytes.data(), bytes.size(), w, error)) {
            LOG_INFO("map objects: %s.rsw (%s): %s", name.c_str(), from.c_str(), error.c_str());
        } else {
            ok = true;
            unsigned torches = 0, hidden = 0;
            for (const rsw::Effect& e : w.effects) {
                torches += e.id == 47;
                hidden += e.id > 629;
            }
            LOG_INFO("map objects: %s from %s - %u lamps, %u effects (%u torches, %u the game does not show)",
                     name.c_str(), from.c_str(), unsigned(w.lights.size()), unsigned(w.effects.size()), torches, hidden);
        }
        std::string readWhy;  // (3.0.20-beta.54: the readers here - the walls' colour indoors reads pictures too)
        // a file of the game's: the data folder first, then its archives (names as written, or lower case)
        const auto readGame = [&](const std::string& entry, std::vector<uint8_t>& out) {
            const std::string low = lower(entry);
            bool got = win::gamefiles::readWhole(dir + "\\" + entry, out);
            for (size_t i = 0; i < archives.size() && !got; ++i) {
                Archive& a = *archives[i];
                if (!open(a)) continue;
                got = a.grf ? (a.g.find(entry) && a.g.read(entry, out, readWhy)) ||
                                  (a.g.find(low) && a.g.read(low, out, readWhy))
                            : a.zip.has(low) && a.zip.read(low, out, readWhy);
            }
            return got;
        };
        // a texture in full colour with its mipmaps, each half of the one before; `keyed`: the magenta
        // see-through (3.0.4-beta.14, the models' leaves and fences, as the game keys them; 3.0.9-beta.3
        // the ground's too - R-6JBVTG, Izlude's bridge and wall)
        // (3.0.20-beta.53: `mips` false - its first level alone, for what only looks at it)
        const auto picture = [&](const std::string& name, bool keyed, bool mips = true) {
            OwnGround::Picture pic;
            std::vector<uint8_t> tb;
            std::vector<uint32_t> argb;
            int pw = 0, ph = 0;
            bool alpha = false;
            const std::string low = lower(name);
            const bool tga = low.size() > 4 && low.compare(low.size() - 4, 4, ".tga") == 0;
            const bool jpg = low.size() > 4 && low.compare(low.size() - 4, 4, ".jpg") == 0;  // the water's
            if (!readGame("data\\texture\\" + name, tb) ||
                !(tga   ? pack::decodeTga(tb.data(), tb.size(), argb, pw, ph, alpha)
                  : jpg ? jpeg::decode(tb.data(), tb.size(), argb, pw, ph)
                        : pack::decodeBmp(tb.data(), tb.size(), argb, pw, ph)) ||
                pw <= 0 || ph <= 0)
                return pic;
            // keyed to the game's own see-through texel (0x0421 in its 16 bits): the AI textures know it
            if (keyed && !tga) pic.keyed = full_colour::keyMagenta(argb);
            pic.width = pw;
            pic.height = ph;
            pic.levels.push_back(std::move(argb));
            while (mips && (pw > 1 || ph > 1)) {
                std::vector<uint32_t> half;
                int hw = 0, hh = 0;
                full_colour::halve(pic.levels.back(), pw, ph, half, hw, hh);
                if (half.empty()) break;
                pic.levels.push_back(std::move(half));
                pw = hw, ph = hh;
            }
            return pic;
        };
        // 2.6.18: its ground's heights, where its .rsw came from or after it (the walls between a fire and a shadow)
        ground_heights::Ground ground;
        MapWorld::VaultRock rock;        // 3.0.20-beta.54
        rooms::Map roomsOfMap;           // 3.0.20-beta.57
        std::unique_ptr<OwnGround> own;  // 3.0.4-beta.10
        if (ok) {
            const std::string gnd = "data\\" + name + ".gnd";
            std::vector<uint8_t> gb;
            std::string gfrom, why;
            if (win::gamefiles::readWhole(dir + "\\" + gnd, gb)) gfrom = "the data folder";
            for (size_t i = 0; i < archives.size() && gfrom.empty(); ++i) {
                Archive& a = *archives[i];
                if (!open(a)) continue;
                if (a.grf ? a.g.find(gnd) && a.g.read(gnd, gb, why) : a.zip.has(gnd) && a.zip.read(gnd, gb, why))
                    gfrom = a.path.substr(a.path.find_last_of('\\') + 1);
            }
            if (gfrom.empty())
                LOG_INFO("map objects: %s.gnd is in neither the data folder nor DATA.INI's archives", name.c_str());
            else if (!ground_heights::parse(gb.data(), gb.size(), ground, why))
                LOG_INFO("map objects: %s.gnd (%s): %s", name.c_str(), gfrom.c_str(), why.c_str());
            EnterCriticalSection(&shared_->lock);
            const bool ownWanted = shared_->ownWanted;
            const bool roomWalls = shared_->roomWalls;  // 3.0.20-beta.71
            const bool wallsWanted = shared_->wallsWanted;  // 3.0.23-beta.6
            const bool outskirtsRelief = shared_->outskirtsRelief;  // 3.0.22-beta.16
            LeaveCriticalSection(&shared_->lock);
            // 3.0.4-beta.10: the own ground - its whole .gnd, its textures. 3.0.4-beta.20: no room for it in the game's
            // memory (32-bit) - the game's own ground, the game goes on
            // 3.0.23-beta.6: or, «Своя земля» off, for the 3D view's walls of a map under a roof alone
            const bool wallsAlone = !ownWanted && wallsWanted && roomWalls && pitwalls::wallsFor(name);
            if ((ownWanted || wallsAlone) && !gfrom.empty()) try {
                own = std::make_unique<OwnGround>();
                own->wallsOnly = wallsAlone;
                gnd::Ground full;
                if (!gnd::parse(gb.data(), gb.size(), full, why)) {
                    LOG_INFO("own ground: %s.gnd (%s): %s", name.c_str(), gfrom.c_str(), why.c_str());
                    own.reset();
                } else {
                    // 3.0.20-beta.71 (the owner's check list, 18 «есть стены чёрные», his pick (a) of the interiors'
                    // options): in a house the makers' black blocks beside the rooms given the room's own wall
                    // picture - for the 3D view (core/room_blocks); before the filler's own textures and the pictures
                    if (roomWalls && cave_vault::ceilingFor(name, outdoors(name))) {
                        const rooms::Map houseRooms = rooms::build(full);
                        roomblocks::Done dressed = roomblocks::dress(full, houseRooms);
                        if (dressed.tops || dressed.sides || dressed.made) {
                            own->fromAbove = std::move(dressed.fromAbove);
                            own->madeTextures.assign(own->fromAbove.size(), 0);
                            for (size_t t = 0; t < own->fromAbove.size(); ++t)
                                own->madeTextures[t] = own->fromAbove[t] == roomblocks::kNotDrawn ? 1 : 0;
                            LOG_INFO("own ground: %s - the black blocks beside %zu of its %d room(s) given the "
                                     "room's wall picture for the 3D view: %zu tops, %zu black sides, %zu sides made "
                                     "where the makers left none ([view3d] room_walls); from above as they were",
                                     name.c_str(), dressed.rooms, houseRooms.count, dressed.tops, dressed.sides,
                                     dressed.made);
                        }
                    }
                    {  // 3.0.20-beta.48 (the audit's question 3): the makers' filler round the ground (BACKSIDE.BMP,
                       // BLACK.BMP; dry) the 3D view's land takes - its cells' tops given textures of their own, for
                       // the renderer to leave out where the land is drawn. Before the pictures are read (theirs too)
                        outskirts::Options filler;
                        filler.water = w.waterKnown;
                        filler.waterLevel = w.waterLevel;
                        own->fillerTextures = outskirts::splitFiller(full, filler);
                        size_t cells = 0;
                        for (int y = 0; y < full.height; ++y)
                            for (int x = 0; x < full.width; ++x) cells += outskirts::fillerCell(full, filler, x, y);
                        if (cells)
                            LOG_INFO("own ground: %s - %zu cells of the makers' filler round its ground (a black "
                                     "ring): the 3D view's land takes them", name.c_str(), cells);
                    }
                    own->map = name;
                    own->width = full.width;
                    own->height = full.height;
                    own->zoom = full.zoom;
                    own->atlas = terrain::lightmapAtlas(full);
                    own->sunless = sun_shadows::sunlessOf(full);  // 3.0.20-beta.50: for the trial of real shadows
                    own->shade = sun_shadows::shadeOf(full);
                    own->cornerNormals = own_ground::cornerNormals(full);  // 3.0.4-beta.26: the smooth hills' light
                    // 3.0.4-beta.12: the strips between the cells, none round a patch laid by hand
                    const std::vector<uint8_t>* laid = ground.laid.size() == full.cells.size() ? &ground.laid : nullptr;
                    unsigned found = 0;
                    for (const std::string& t : full.textures) {
                        own->textures.push_back(picture(t, true));
                        found += own->textures.back().levels.empty() ? 0 : 1;
                    }
                    // 3.0.18-beta.204 (R-7XY449, a tester's «зайшов і відразу побіліло»: the ground white - the game's
                    // corners' colour on a white picture, as a texture not there is drawn): one not read is read again
                    // (the game's archives may be busy while it loads the map); one still not there that a surface
                    // shows - the game's own ground on this map, not a white one
                    for (int again = 0; again < 3 && found < full.textures.size(); ++again) {
                        Sleep(250);
                        for (size_t i = 0; i < full.textures.size(); ++i)
                            if (own->textures[i].levels.empty()) {
                                own->textures[i] = picture(full.textures[i], true);
                                found += own->textures[i].levels.empty() ? 0 : 1;
                            }
                    }
                    bool whiteRisk = false;
                    if (found < full.textures.size()) {
                        std::vector<bool> used(full.textures.size(), false);
                        for (const gnd::Surface& sf : full.surfaces)
                            if (sf.texture >= 0 && size_t(sf.texture) < used.size()) used[size_t(sf.texture)] = true;
                        std::string missing;
                        for (size_t i = 0; i < full.textures.size(); ++i)
                            if (own->textures[i].levels.empty()) {
                                missing += (missing.empty() ? "" : ", ") + full.textures[i];
                                if (!used[i]) missing += " (unused)";
                                whiteRisk = whiteRisk || used[i];
                            }
                        LOG_INFO("own ground: %s - %u of %u textures not read after 3 tries: %s%s", name.c_str(),
                                 unsigned(full.textures.size()) - found, unsigned(full.textures.size()),
                                 missing.c_str(),
                                 whiteRisk ? " - the game's own ground on this map, not a white one" : "");
                    }
                    // 3.0.4-beta.14: its models, each file read once, placed as the map places them
                    std::vector<std::string> modelNames;
                    std::vector<rsm::Model> files;
                    std::vector<bool> filesOk;
                    std::vector<size_t> fileOf;
                    for (const rsw::Model& m : w.models) {
                        const std::string key = lower(m.file);
                        size_t k = 0;
                        while (k < modelNames.size() && modelNames[k] != key) ++k;
                        if (k == modelNames.size()) {
                            modelNames.push_back(key);
                            std::vector<uint8_t> mb;
                            rsm::Model model;
                            const bool ok = readGame("data\\model\\" + m.file, mb) &&
                                            rsm::parse(mb.data(), mb.size(), model, why);
                            files.push_back(ok ? std::move(model) : rsm::Model{});
                            filesOk.push_back(ok);
                        }
                        fileOf.push_back(k);
                    }
                    std::vector<const rsm::Model*> perInstance;
                    for (size_t k : fileOf) perInstance.push_back(filesOk[k] ? &files[k] : nullptr);
                    std::vector<std::string> modelTextureNames;
                    own->models = own_models::place(w.models, perInstance, full.width, full.height, full.zoom,
                                                    modelTextureNames);
                    for (const std::string& t : modelTextureNames) own->modelTextures.push_back(picture(t, true));
                    own->modelFiles = unsigned(modelNames.size());
                    for (bool ok : filesOk) own->modelFilesRead += ok ? 1 : 0;
                    if (w.lightKnown) {  // the way to the light as uaRO.exe turns the file's (3.0.7-beta.4)
                        float way[3];
                        rsw::lightWay(w.lightLongitude, w.lightLatitude, way);
                        for (int k = 0; k < 3; ++k) own->toLight[k] = -way[k];
                    }
                    LOG_INFO("own ground: %s's models - %u placed from %u of %u files, %u triangles, %u textures",
                             name.c_str(), unsigned(w.models.size()), own->modelFilesRead, own->modelFiles,
                             unsigned(own->models.indices.size() / 3), unsigned(own->modelTextures.size()));
                    // 3.0.6-beta.3 (R-RSK2JR «дирка на сходах в комодо»): the ground after its models - where one
                    // rests on it, its smooth patch no higher than the game's plain cells
                    const std::vector<uint8_t> resting = terrain::restingOn(full, own->models);
                    // 3.0.7-beta.3 (R-YXZM7Y «посмотри насчет освещения стен»): the walls lit as the game lights them
                    // by the map's light (FUN_004e4190), not as the ground beside them - a wall turned from the sun
                    // was up to twice as bright as the game's
                    const terrain::Sun sun = w.lightKnown ? terrain::sunOf(w.lightLongitude, w.lightLatitude,
                                                                           w.lightDiffuse, w.lightAmbient)
                                                          : terrain::Sun{};
                    own->mesh = terrain::build(full, sizeof(void*) == 8 ? 3 : 2, own->atlas, laid, &resting, &sun);
                    own->distant = terrain::build(full, 1, own->atlas, nullptr, nullptr, &sun);
                    {  // 3.0.20-beta.30 (the owner's «6,7,8,9», 6): the land past its edges - the 3D view's horizon
                        outskirts::Options ring;
                        ring.relief = outskirtsRelief;  // 3.0.22-beta.16: the panorama's land past it instead
                        ring.water = w.waterKnown;
                        ring.waterLevel = w.waterLevel;
                        uint32_t seed = 2166136261u;  // the map's name hashed (FNV-1a): its own hills, every time
                        for (const char c : name) seed = (seed ^ uint8_t(c)) * 16777619u;
                        ring.seed = seed;
                        // 3.0.20-beta.31: the sea past it (3.0.20-beta.42: made with it, one plan for both)
                        own->outskirts = outskirts::build(full, own->atlas, sun, ring, &own->seaFrame);
                        // 3.0.22-beta.22 (the owner's R-GK3ZCT «тут тоже походу стен нету»): in a house or a hall
                        // the makers' pits standing for walls (the Pyramids' mazes) given upright walls for the 3D
                        // view, in the third mesh (no land past a house's edges is drawn) - their height the
                        // ceiling's, set once the models tell it (below)
                        // 3.0.22-beta.47 (the owner: «да, для всех»): every map under a roof (pitwalls::wallsFor)
                        if (roomWalls && pitwalls::wallsFor(name)) {
                            terrain::Mesh raised = pitwalls::build(full, own->atlas, sun, cave_vault::kCeilingFlat);
                            if (!raised.chunks.empty()) {
                                own->outskirts = std::move(raised);
                                own->seaFrame.clear();
                                own->pitWalls = true;
                            }
                        }
                        own->edgeCorners.assign(size_t(full.width + 1) * size_t(full.height + 1), 0);
                        for (const terrain::Vertex& v : own->outskirts.vertices) {
                            const uint32_t corner = v.blend & 0xFFFFFFu;
                            if (corner < own->edgeCorners.size()) own->edgeCorners[corner] = 1;
                        }
                        if (own->pitWalls)
                            LOG_INFO("own ground: %s - its pits standing for walls (a maze from above) given upright "
                                     "walls for the 3D view: %u faces in %u chunks ([view3d] room_walls)",
                                     name.c_str(), unsigned(own->outskirts.indices.size() / 6),
                                     unsigned(own->outskirts.chunks.size()));
                        else
                            LOG_INFO("own ground: %s's outskirts - %u chunks, %u vertices, %u triangles past its "
                                     "edges, %d cells off (the 3D view's horizon)%s%s",
                                     name.c_str(), unsigned(own->outskirts.chunks.size()),
                                     unsigned(own->outskirts.vertices.size()),
                                     unsigned(own->outskirts.indices.size() / 3), ring.ring,
                                     own->seaFrame.empty() ? "" : "; the sea past its wet edges",
                                     ring.relief ? "" : "; no hills of its own (the horizon panorama's)");
                    }
                    // 3.0.20-beta.41: the grass's cells of the whole map (60 ms here, not on the renderer's thread);
                    // 3.0.20-beta.42: the land's own within the map's rectangle too (the open cells it fills)
                    own->cells = ground_cells::ofMeshes({&own->mesh, &own->distant, &own->outskirts}, full.width,
                                                        full.height, full.zoom, 1, 1, 1);
                    {  // 3.0.20-beta.46 (R-HGTAKK): the cells whose own texture is not green - no grass's ground
                        std::vector<ground_cells::Picture> pictures(own->textures.size());
                        for (size_t i = 0; i < own->textures.size(); ++i) {
                            const OwnGround::Picture& p = own->textures[i];
                            if (!p.levels.empty()) pictures[i] = {p.width, p.height, p.levels[0].data()};
                        }
                        const std::vector<uint8_t> grassy = ground_cells::grassyCells(full, pictures);
                        size_t bare = 0;
                        if (grassy.size() == size_t(ground.width) * size_t(ground.height)) {
                            ground.bare.resize(grassy.size());
                            for (size_t i = 0; i < grassy.size(); ++i) {
                                ground.bare[i] = grassy[i] ? uint8_t(0) : uint8_t(1);
                                bare += grassy[i] ? 0 : 1;
                            }
                        }
                        LOG_INFO("grass: %s - %zu of its %zu cells' own textures are not green (under %.2f by the "
                                 "grass's measure: paving, sand, rock) - no grass on them",
                                 name.c_str(), bare, grassy.size(), double(ground_cells::kGrassy));
                    }
                    for (uint8_t r : resting) own->restingCells += r;
                    own->wallLight.assign(full.cells.size() * 2, 0u);
                    for (int y = 0; y < full.height; ++y)
                        for (int x = 0; x < full.width; ++x)
                            for (int side = 0; side < 2; ++side)
                                own->wallLight[(size_t(y) * size_t(full.width) + size_t(x)) * 2 + size_t(side)] =
                                    terrain::wallLight(full, x, y, side == 0, sun);
                    LOG_INFO("own ground: %s's walls lit as the game lights them - its light's way %.3f %.3f %.3f "
                             "(longitude %d, latitude %d%s), diffuse %.2f %.2f %.2f, all round %.2f %.2f %.2f",
                             name.c_str(), sun.way[0], sun.way[1], sun.way[2], w.lightLongitude, w.lightLatitude,
                             w.lightKnown ? "" : ", none in the file: the game's", sun.diffuse[0], sun.diffuse[1],
                             sun.diffuse[2], sun.ambient[0], sun.ambient[1], sun.ambient[2]);
                    // 3.0.4-beta.20: its water (460 of the client's 921 maps have some; lava on 3)
                    if (w.waterKnown) {
                        own->water = terrain::water(full, w.waterLevel, w.waveHeight);
                        own->waterType = w.waterType;
                        own->waterLevel = w.waterLevel;
                        own->waterAnim = std::max(w.waterAnim, 1);
                        own->waveHeight = w.waveHeight;  // 3.0.4-beta.23: the game holds pitch and speed whole
                        own->wavePitch = int(w.wavePitch);
                        own->waveSpeed = int(w.waveSpeed);
                        // its 32 pictures as the game reads them (FUN_004e3b80: water%d%02d.jpg under 워터; a kind
                        // it has none of: the kind modulo 6), as many as there are one after another
                        for (int kind : {w.waterType, w.waterType % 6}) {
                            for (int f = 0; f < 32 && !own->water.vertices.empty(); ++f) {
                                char file[40];
                                std::snprintf(file, sizeof file, "\xBF\xF6\xC5\xCD\\water%d%02d.jpg", kind, f);
                                OwnGround::Picture pic = picture(file, false);
                                if (pic.levels.empty()) break;
                                own->waterPictures.push_back(std::move(pic));
                            }
                            if (!own->waterPictures.empty() || kind == w.waterType % 6) break;
                        }
                        if (!own->water.vertices.empty())
                            LOG_INFO("own ground: %s's %s - level %.1f, waves %.1f, %u cells, %u pictures",
                                     name.c_str(), w.waterType == 4 ? "lava" : "water", w.waterLevel, w.waveHeight,
                                     unsigned(own->water.vertices.size() / 4), unsigned(own->waterPictures.size()));
                    }
                    LOG_INFO("own ground: %s - %u cells' worth in %u chunks, %u triangles, %u of %u textures, "
                             "lightmaps %dx%d; %u cells under models kept no higher than the game's",
                             name.c_str(), unsigned(full.cells.size()), unsigned(own->mesh.chunks.size()),
                             unsigned(own->mesh.indices.size() / 3), found, unsigned(full.textures.size()),
                             own->atlas.width, own->atlas.height, own->restingCells);
                    // 3.0.4-beta.20: room left in the game's memory (core/own_ground.hpp roomFor) - the far models let
                    // go, then the smooth ground, then all of it
                    MEMORYSTATUSEX mem{};
                    mem.dwLength = sizeof mem;
                    const uint64_t freeBytes = GlobalMemoryStatusEx(&mem) ? mem.ullAvailVirtual : ~uint64_t(0);
                    const auto bytes = [](const terrain::Mesh& m) {
                        return uint64_t(m.vertices.capacity()) * sizeof(terrain::Vertex) +
                               uint64_t(m.indices.capacity()) * sizeof(uint32_t);
                    };
                    const own_ground::Room room = own_ground::roomFor(freeBytes, bytes(own->models), bytes(own->mesh));
                    if (room != own_ground::Room::Whole) {
                        own->models = terrain::Mesh();
                        std::vector<OwnGround::Picture>().swap(own->modelTextures);
                        own->outskirts = terrain::Mesh();  // 3.0.20-beta.30
                        // 3.0.20-beta.42: and the sea with it - without the land the sea lay round the dry sides too
                        std::vector<float>().swap(own->seaFrame);
                        std::vector<uint8_t>().swap(own->edgeCorners);
                    }
                    if (room == own_ground::Room::Coarse) {  // the plain ground near too (distant empty: mesh all)
                        own->mesh = std::move(own->distant);
                        own->distant = terrain::Mesh();
                    }
                    if (sizeof(void*) == 4 || room != own_ground::Room::Whole)
                        LOG_INFO("own ground: %s - %u MB of the game's memory free with it made%s", name.c_str(),
                                 unsigned(freeBytes >> 20),
                                 room == own_ground::Room::NoModels ? ": the far models let go"
                                 : room == own_ground::Room::Coarse ? ": the far models and the smooth ground let go"
                                 : room == own_ground::Room::None   ? ": none of it - the game's own ground"
                                                                    : "");
                    if (room == own_ground::Room::None) own.reset();
                    if (whiteRisk) own.reset();  // 3.0.18-beta.204
                    // 3.0.23-beta.6: for its walls alone - the far models and the water not wanted (the game's own,
                    // as with «Своя земля» off); none of its walls built: nothing to draw
                    if (own && own->wallsOnly) {
                        own->models = terrain::Mesh();
                        std::vector<OwnGround::Picture>().swap(own->modelTextures);
                        std::vector<OwnGround::Picture>().swap(own->waterPictures);
                        own->water = decltype(own->water)();
                        if (!own->pitWalls) own.reset();
                        else
                            LOG_INFO("own ground: %s - read for the 3D view's room walls alone («Своя земля» off): "
                                     "the game's ground stays its own", name.c_str());
                    }
                }
            } catch (const std::bad_alloc&) {
                own.reset();
                LOG_INFO("own ground: %s - no room for it in the game's memory: the game's own ground", name.c_str());
            }
            // 3.0.20-beta.53 (the owner's «делай 1-9», 8 - «трава не на камне у всех»): beta.46's rule - no grass on a
            // cell whose own texture is not green - where the own ground did not give it (it is off or was not made;
            // Direct3D 11 has none): the ground's file once more, its tops' textures read one at a time, none kept.
            // 3.0.20-beta.54: and a dungeon's own rock for the 3D view's vault, the same way
            const bool wantBare = ground.width > 0 && ground.bare.empty() && grassGrows(name);
            const bool wantRock = cave_vault::forMap(name);
            // 3.0.20-beta.57 (R-J3GKD9): a house's rooms - its floors joined side to side
            const bool wantRooms = cave_vault::ceilingFor(name, outdoors(name));
            if (!gfrom.empty() && (wantBare || wantRock || wantRooms)) try {
                gnd::Ground lite;
                const bool parsed = gnd::parse(gb.data(), gb.size(), lite, why);
                if (parsed && wantRooms) {
                    roomsOfMap = rooms::build(lite);
                    float lowest = 0.0f, highest = 0.0f;
                    for (int r = 1; r <= roomsOfMap.count; ++r) {
                        const float up = roomsOfMap.floor[size_t(r)];
                        lowest = r == 1 ? up : std::min(lowest, up);
                        highest = r == 1 ? up : std::max(highest, up);
                    }
                    LOG_INFO("map objects: %s is %d room(s) side by side (floors joined side to side, %d cells at "
                             "least), their floors %.0f to %.0f up - the 3D view's ceiling over each one's own "
                             "floor, the one the player is in drawn alone ([view3d] one_room)",
                             name.c_str(), roomsOfMap.count, rooms::kLeastCells, double(lowest), double(highest));
                }
                if (parsed && wantRock) {
                    OwnGround::Picture held;  // the one picture in memory: the last asked for - the patch's, if any
                    const cave_vault::Patch found =
                        cave_vault::rockPatch(lite, [&](size_t texture, ground_cells::Picture& out) {
                            held = picture(lite.textures[texture], true, false);
                            if (held.levels.empty()) return false;
                            out = {held.width, held.height, held.levels[0].data()};
                            return true;
                        });
                    rock.known = found.known;
                    rock.texels = found.texels;
                    std::copy(found.mean, found.mean + 3, rock.mean);
                    if (found.side > 0 && !held.levels.empty() && found.x + found.side <= held.width &&
                        found.y + found.side <= held.height) {
                        std::vector<uint32_t> cut(size_t(found.side) * size_t(found.side));
                        for (int y = 0; y < found.side; ++y)
                            std::copy_n(held.levels[0].data() + size_t(found.y + y) * size_t(held.width) +
                                            size_t(found.x),
                                        size_t(found.side), cut.data() + size_t(y) * size_t(found.side));
                        rock.patch.width = rock.patch.height = found.side;
                        rock.patch.levels.push_back(std::move(cut));
                        for (int pw = found.side, ph = found.side; pw > 1 || ph > 1;) {
                            std::vector<uint32_t> half;
                            int hw = 0, hh = 0;
                            full_colour::halve(rock.patch.levels.back(), pw, ph, half, hw, hh);
                            if (half.empty()) break;
                            rock.patch.levels.push_back(std::move(half));
                            pw = hw, ph = hh;
                        }
                    }
                    if (found.side > 0)
                        LOG_INFO("map objects: %s's own rock for the 3D view's vault - a patch of %dx%d texels of "
                                 "its ground's %s (at %d, %d), %.1f units of the world across, its colour %.2f %.2f "
                                 "%.2f", name.c_str(), found.side, found.side,
                                 lite.textures[size_t(found.texture)].c_str(), found.x, found.y,
                                 double(float(found.side) / found.texels), double(found.mean[0]),
                                 double(found.mean[1]), double(found.mean[2]));
                    else if (found.known)
                        LOG_INFO("map objects: %s's ground has no patch of rock large enough for the 3D view's vault "
                                 "- its rock's colour alone (%.2f %.2f %.2f) on the vault's own stone", name.c_str(),
                                 double(found.mean[0]), double(found.mean[1]), double(found.mean[2]));
                    else
                        LOG_INFO("map objects: %s's ground shows no rock of its own (or its pictures were not read) "
                                 "- the 3D view's vault as before", name.c_str());
                }
                if (parsed && wantBare && lite.width == ground.width && lite.height == ground.height) {
                    OwnGround::Picture held;  // the one picture in memory
                    size_t read = 0, missing = 0;
                    int waits = 0;
                    const std::vector<uint8_t> grassy = ground_cells::grassyCells(
                        lite, [&](size_t texture, ground_cells::Picture& out) {
                            held = picture(lite.textures[texture], true, false);
                            // the game's archives may be busy while it loads the map (3.0.18-beta.204): once more
                            for (; held.levels.empty() && waits < 3; ++waits) {
                                Sleep(250);
                                held = picture(lite.textures[texture], true, false);
                            }
                            if (held.levels.empty()) {
                                ++missing;
                                return false;
                            }
                            ++read;
                            out = {held.width, held.height, held.levels[0].data()};
                            return true;
                        });
                    size_t bare = 0;
                    ground.bare.resize(grassy.size());
                    for (size_t i = 0; i < grassy.size(); ++i) {
                        ground.bare[i] = grassy[i] ? uint8_t(0) : uint8_t(1);
                        bare += grassy[i] ? 0 : 1;
                    }
                    LOG_INFO("grass: %s - %zu of its %zu cells' own textures are not green (under %.2f by the grass's "
                             "measure: paving, sand, rock) - no grass on them; without the own ground: %zu of its "
                             "tops' textures read one at a time, %zu not found (their cells as before)",
                             name.c_str(), bare, grassy.size(), double(ground_cells::kGrassy), read, missing);
                }
            } catch (const std::bad_alloc&) {
                if (wantBare) ground.bare.clear();
                rock = MapWorld::VaultRock{};
                roomsOfMap = rooms::Map{};
                LOG_INFO("map objects: %s - no room to read its ground's textures: the grass by the screen's colour "
                         "alone, the vault's own stone", name.c_str());
            }
        }
        // a model's file read once (the data folder, then the archives); an empty model where it is not read
        std::deque<std::pair<std::string, rsm::Model>> readModels;  // a deque: a model given out stays where it is
        const auto modelOf = [&](const std::string& file) -> const rsm::Model& {
            const std::string key = lower(file);
            for (const auto& known : readModels)
                if (known.first == key) return known.second;
            const std::string entry = "data\\model\\" + file;
            std::vector<uint8_t> mb;
            std::string why;
            bool found = win::gamefiles::readWhole(dir + "\\" + entry, mb);
            for (size_t i = 0; i < archives.size() && !found; ++i) {
                Archive& a = *archives[i];
                if (!open(a)) continue;
                found = a.grf ? a.g.find(entry) && a.g.read(entry, mb, why)
                              : a.zip.has(entry) && a.zip.read(entry, mb, why);
            }
            rsm::Model model;
            if (!found || !rsm::parse(mb.data(), mb.size(), model, why)) model = rsm::Model{};
            readModels.emplace_back(key, std::move(model));
            return readModels.back().second;
        };
        // 3.0.1-beta.3 (R-SA3W7Y): its flower beds' tops, the grass's ground too - each bed's model read once
        std::vector<grass_beds::Box> beds;
        for (const rsw::Model& m : w.models) {
            if (!ok || !grass_beds::bedModel(m.file)) continue;
            for (const grass_beds::Box& b : grass_beds::place(modelOf(m.file), m)) beds.push_back(b);
        }
        // 3.0.20-beta.46 (R-3T9MZC «колоны как будто в воздухе», pay_dun00): a dungeon's posts - its models that
        // stand tall and wide-topped (cave_vault::postOf); the 3D view's vault comes down to those that reach its
        // walls' height
        std::vector<cave_vault::Post> posts;
        if (ok && cave_vault::forMap(name)) {
            for (const rsw::Model& m : w.models) {
                cave_vault::Post post;
                if (cave_vault::postOf(modelOf(m.file), m, post)) posts.push_back(post);
            }
            LOG_INFO("map objects: %u of %s's %u models stand as posts (tall, wide-topped) - the 3D view's vault "
                     "comes down to those that reach its walls' height",
                     unsigned(posts.size()), name.c_str(), unsigned(w.models.size()));
        }
        if (!beds.empty())
            LOG_INFO("map objects: %u flower beds' tops on %s - the grass grows on them", unsigned(beds.size()),
                     name.c_str());
        // 3.0.20-beta.52 (the owner's «делай 1-9», 5): indoors - a house, a castle's hall - the walls' height by its
        // tall models, for the 3D view's ceiling where the ground makes no walls
        float walls = 0.0f, wallsColour[3] = {0.0f, 0.0f, 0.0f};
        bool wallsColourKnown = false;
        if (ok && cave_vault::ceilingFor(name, outdoors(name))) try {
            std::vector<float> tall;
            // 3.0.20-beta.54: and their upper part's colour, the ceiling's - each texture's mean colour read once
            cave_vault::Tone tone;
            std::vector<std::pair<std::string, cave_vault::Mean>> meanOfTexture;
            for (const rsw::Model& m : w.models) {
                const rsm::Model& model = modelOf(m.file);
                const float h = cave_vault::tallOf(model, m);
                if (h < cave_vault::kPostLeast) continue;
                tall.push_back(h);
                std::vector<cave_vault::Mean> means(model.textures.size());
                for (size_t t = 0; t < model.textures.size(); ++t) {
                    const std::string key = lower(model.textures[t]);
                    size_t k = 0;
                    while (k < meanOfTexture.size() && meanOfTexture[k].first != key) ++k;
                    if (k == meanOfTexture.size()) {
                        const OwnGround::Picture pic = picture(model.textures[t], true, false);
                        cave_vault::Mean mean;
                        if (!pic.levels.empty()) mean = cave_vault::meanOf(pic.levels[0].data(), pic.levels[0].size());
                        meanOfTexture.emplace_back(key, mean);
                    }
                    means[t] = meanOfTexture[k].second;
                }
                cave_vault::wallTone(model, m, means, tone);
            }
            walls = cave_vault::wallsByModels(tall);
            wallsColourKnown = cave_vault::toneOf(tone, wallsColour);
            LOG_INFO("map objects: %s is indoors - %u of its %u models stand tall, the walls' height by them %.0f, "
                     "their upper part's colour %.2f %.2f %.2f%s (the 3D view's ceiling; %u of their textures read)",
                     name.c_str(), unsigned(tall.size()), unsigned(w.models.size()), double(walls),
                     double(wallsColour[0]), double(wallsColour[1]), double(wallsColour[2]),
                     wallsColourKnown ? "" : " - not known", unsigned(meanOfTexture.size()));
        } catch (const std::bad_alloc&) {
            wallsColourKnown = false;
            LOG_INFO("map objects: %s - no room to read its walls' textures: the ceiling in its plain stone",
                     name.c_str());
        }
        // 3.0.22-beta.22 (R-GK3ZCT): the pits' walls up to the room's ceiling - its walls' height by the models, as
        // the 3D view's ceiling over the room's floor (cave_vault::buildCeiling)
        if (own && own->pitWalls) {
            const float tall = walls > 0.0f ? walls + cave_vault::kCeilingOver : cave_vault::kCeilingFlat;
            pitwalls::raise(own->outskirts, tall);
            LOG_INFO("own ground: %s - its pits' walls %.0f high (the ceiling's)", name.c_str(), double(tall));
        }
        EnterCriticalSection(&shared_->lock);
        shared_->doneName = name;
        shared_->done = std::move(w);
        shared_->doneGround = std::move(ground);
        shared_->doneBeds = std::move(beds);
        shared_->donePosts = std::move(posts);
        shared_->doneWalls = walls;
        std::copy(wallsColour, wallsColour + 3, shared_->doneWallsColour);
        shared_->doneWallsColourKnown = wallsColourKnown;
        shared_->doneRock = std::move(rock);
        shared_->doneRooms = std::move(roomsOfMap);
        shared_->doneOwn = std::move(own);
        shared_->doneOk = ok;
        shared_->doneReady = true;
        shared_->doneSerial = serial;
        LeaveCriticalSection(&shared_->lock);
    }
}

}  // namespace uagfx::gpu
