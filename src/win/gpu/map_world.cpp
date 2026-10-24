#include "win/gpu/map_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/config.hpp"
#include "core/full_colour.hpp"
#include "core/gnd.hpp"
#include "core/grf/grf.hpp"
#include "core/own_models.hpp"
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
    bool ownWanted = false;                 // 3.0.4-beta.10: the own ground read with the map
    std::unique_ptr<OwnGround> doneOwn;
    bool doneOk = false;
    bool doneReady = false;
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
    LeaveCriticalSection(&shared_->lock);
    if (!thread_) thread_ = CreateThread(nullptr, 0, &MapWorld::threadMain, this, 0, nullptr);
    SetEvent(shared_->wake);
}

void MapWorld::setOwnGround(bool wanted) {
    if (wanted == ownWanted_) return;
    ownWanted_ = wanted;
    EnterCriticalSection(&shared_->lock);
    shared_->ownWanted = wanted;
    LeaveCriticalSection(&shared_->lock);
    if (!wanted) {
        own_.reset();
        return;
    }
    if (!own_) ask(shown_);  // read again, with it
}

void MapWorld::ownPicturesUp() {
    if (!own_) return;
    for (OwnGround::Picture& p : own_->textures) std::vector<std::vector<uint32_t>>().swap(p.levels);
    for (OwnGround::Picture& p : own_->modelTextures) std::vector<std::vector<uint32_t>>().swap(p.levels);
    std::vector<uint32_t>().swap(own_->atlas.argb);
}

void MapWorld::poll() {
    EnterCriticalSection(&shared_->lock);
    const bool fresh = shared_->doneReady && shared_->doneName == shown_;
    if (shared_->doneReady && shared_->doneName == shown_) {
        world_ = std::move(shared_->done);
        ground_ = std::move(shared_->doneGround);
        beds_ = std::move(shared_->doneBeds);
        own_ = std::move(shared_->doneOwn);  // 3.0.4-beta.10
        if (!ownWanted_) own_.reset();       // read before it was turned off
        if (own_) own_->serial = ++ownSerial_;
        ready_ = shared_->doneOk;
        ++reads_;
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
        // 2.6.18: its ground's heights, where its .rsw came from or after it (the walls between a fire and a shadow)
        ground_heights::Ground ground;
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
            LeaveCriticalSection(&shared_->lock);
            if (ownWanted && !gfrom.empty()) {  // 3.0.4-beta.10: the own ground - its whole .gnd, its textures
                own = std::make_unique<OwnGround>();
                gnd::Ground full;
                if (!gnd::parse(gb.data(), gb.size(), full, why)) {
                    LOG_INFO("own ground: %s.gnd (%s): %s", name.c_str(), gfrom.c_str(), why.c_str());
                    own.reset();
                } else {
                    own->map = name;
                    own->width = full.width;
                    own->height = full.height;
                    own->zoom = full.zoom;
                    own->atlas = terrain::lightmapAtlas(full);
                    // 3.0.4-beta.12: the strips between the cells, none round a patch laid by hand
                    const std::vector<uint8_t>* laid = ground.laid.size() == full.cells.size() ? &ground.laid : nullptr;
                    own->mesh = terrain::build(full, sizeof(void*) == 8 ? 3 : 2, own->atlas, laid);
                    own->distant = terrain::build(full, 1, own->atlas);
                    // a file of the game's: the data folder first, then its archives (names as written, or lower case)
                    const auto readGame = [&](const std::string& entry, std::vector<uint8_t>& out) {
                        const std::string low = lower(entry);
                        bool got = win::gamefiles::readWhole(dir + "\\" + entry, out);
                        for (size_t i = 0; i < archives.size() && !got; ++i) {
                            Archive& a = *archives[i];
                            if (!open(a)) continue;
                            got = a.grf ? (a.g.find(entry) && a.g.read(entry, out, why)) ||
                                              (a.g.find(low) && a.g.read(low, out, why))
                                        : a.zip.has(low) && a.zip.read(low, out, why);
                        }
                        return got;
                    };
                    // a texture in full colour with its mipmaps, each half of the one before; `keyed`: the magenta
                    // see-through (3.0.4-beta.14, the models' leaves and fences, as the game keys them)
                    const auto picture = [&](const std::string& name, bool keyed) {
                        OwnGround::Picture pic;
                        std::vector<uint8_t> tb;
                        std::vector<uint32_t> argb;
                        int pw = 0, ph = 0;
                        bool alpha = false;
                        const std::string low = lower(name);
                        const bool tga = low.size() > 4 && low.compare(low.size() - 4, 4, ".tga") == 0;
                        if (!readGame("data\\texture\\" + name, tb) ||
                            !(tga ? pack::decodeTga(tb.data(), tb.size(), argb, pw, ph, alpha)
                                  : pack::decodeBmp(tb.data(), tb.size(), argb, pw, ph)) ||
                            pw <= 0 || ph <= 0)
                            return pic;
                        // keyed to the game's own see-through texel (0x0421 in its 16 bits): the AI textures know it
                        if (keyed && !tga)
                            for (uint32_t& c : argb)
                                if (((c >> 16) & 0xFF) >= 0xFE && ((c >> 8) & 0xFF) <= 3 && (c & 0xFF) >= 0xFE)
                                    c = 0x00080808u;
                        pic.width = pw;
                        pic.height = ph;
                        pic.levels.push_back(std::move(argb));
                        while (pw > 1 || ph > 1) {
                            std::vector<uint32_t> half;
                            int hw = 0, hh = 0;
                            full_colour::halve(pic.levels.back(), pw, ph, half, hw, hh);
                            if (half.empty()) break;
                            pic.levels.push_back(std::move(half));
                            pw = hw, ph = hh;
                        }
                        return pic;
                    };
                    unsigned found = 0;
                    for (const std::string& t : full.textures) {
                        own->textures.push_back(picture(t, false));
                        found += own->textures.back().levels.empty() ? 0 : 1;
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
                    if (w.lightKnown) {  // the way to the light as the client turns the file's (Impl::mapSun)
                        const float lon = float(w.lightLongitude) * 0.0174533f;
                        const float lat = float(w.lightLatitude) * 0.0174533f;
                        own->toLight[0] = -std::cos(lon) * std::sin(lat);
                        own->toLight[1] = -std::cos(lat);
                        own->toLight[2] = -std::sin(lon) * std::sin(lat);
                    }
                    LOG_INFO("own ground: %s's models - %u placed from %u of %u files, %u triangles, %u textures",
                             name.c_str(), unsigned(w.models.size()), own->modelFilesRead, own->modelFiles,
                             unsigned(own->models.indices.size() / 3), unsigned(own->modelTextures.size()));
                    LOG_INFO("own ground: %s - %u cells' worth in %u chunks, %u triangles, %u of %u textures, "
                             "lightmaps %dx%d", name.c_str(), unsigned(full.cells.size()),
                             unsigned(own->mesh.chunks.size()), unsigned(own->mesh.indices.size() / 3), found,
                             unsigned(full.textures.size()), own->atlas.width, own->atlas.height);
                }
            }
        }
        // 3.0.1-beta.3 (R-SA3W7Y): its flower beds' tops, the grass's ground too - each bed's model read once
        std::vector<grass_beds::Box> beds;
        std::vector<std::pair<std::string, rsm::Model>> bedModels;
        for (const rsw::Model& m : w.models) {
            if (!ok || !grass_beds::bedModel(m.file)) continue;
            auto it = std::find_if(bedModels.begin(), bedModels.end(),
                                   [&](const auto& b) { return b.first == lower(m.file); });
            if (it == bedModels.end()) {
                const std::string entry = "data\\model\\" + m.file;
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
                bedModels.emplace_back(lower(m.file), model);
                it = bedModels.end() - 1;
            }
            for (const grass_beds::Box& b : grass_beds::place(it->second, m)) beds.push_back(b);
        }
        if (!beds.empty())
            LOG_INFO("map objects: %u flower beds' tops on %s - the grass grows on them", unsigned(beds.size()),
                     name.c_str());
        EnterCriticalSection(&shared_->lock);
        shared_->doneName = name;
        shared_->done = std::move(w);
        shared_->doneGround = std::move(ground);
        shared_->doneBeds = std::move(beds);
        shared_->doneOwn = std::move(own);
        shared_->doneOk = ok;
        shared_->doneReady = true;
        LeaveCriticalSection(&shared_->lock);
    }
}

}  // namespace uagfx::gpu
