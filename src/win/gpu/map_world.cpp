#include "win/gpu/map_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/grf/grf.hpp"
#include "core/zip.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[65536];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && out.size() < (size_t(64) << 20)) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return !out.empty();
}

// DATA.INI's [Data] archives in the order of their numbers (the client looks in them in that order).
std::vector<std::string> dataArchives(const std::string& dir) {
    std::vector<uint8_t> b;
    if (!readFile(dir + "\\DATA.INI", b)) return {};
    std::vector<std::pair<long, std::string>> found;
    bool data = false;
    std::string text(b.begin(), b.end()), line;
    for (size_t p = 0; p <= text.size(); ++p) {
        if (p < text.size() && text[p] != '\n') {
            line += text[p];
            continue;
        }
        const std::string l = trim(line);
        line.clear();
        if (l.empty() || l[0] == ';') continue;
        if (l[0] == '[') {
            data = lower(l) == "[data]";
            continue;
        }
        const size_t eq = l.find('=');
        if (!data || eq == std::string::npos) continue;
        char* end = nullptr;
        const std::string key = trim(l.substr(0, eq));
        const long n = std::strtol(key.c_str(), &end, 10);
        const std::string file = trim(l.substr(eq + 1));
        if (end && *end == '\0' && !key.empty() && !file.empty()) found.emplace_back(n, file);
    }
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::string> out;
    for (const auto& f : found) out.push_back(f.second);
    return out;
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
    if (readFile(dir + "\\uaro-graphics\\map-axes.txt", b)) {
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
    placed_.clear();
    lamps_.clear();
    seen_.clear();
    if (name.empty() || gameDir_.empty()) return;
    EnterCriticalSection(&shared_->lock);
    shared_->wanted = name;
    shared_->asked = true;
    LeaveCriticalSection(&shared_->lock);
    if (!thread_) thread_ = CreateThread(nullptr, 0, &MapWorld::threadMain, this, 0, nullptr);
    SetEvent(shared_->wake);
}

void MapWorld::poll() {
    EnterCriticalSection(&shared_->lock);
    const bool fresh = shared_->doneReady && shared_->doneName == shown_;
    if (shared_->doneReady && shared_->doneName == shown_) {
        world_ = std::move(shared_->done);
        ground_ = std::move(shared_->doneGround);
        ready_ = shared_->doneOk;
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
            for (const std::string& file : dataArchives(dir)) {
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
        if (readFile(dir + "\\" + entry, bytes)) from = "the data folder";
        // an archive opened when first looked in (2.0: also for the models, which may lie in a later one)
        const auto open = [&](Archive& a) {
            if (a.tried) return a.ok;
            a.tried = true;
            const auto maps = [](const std::string& n) {  // 2.0: and the models, for the trees
                return n.size() > 4 && (n.compare(n.size() - 4, 4, ".rsw") == 0 ||
                                        n.compare(n.size() - 4, 4, ".rsm") == 0 ||
                                        n.compare(n.size() - 4, 4, ".gnd") == 0);  // 2.6.18: its ground
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
        if (ok) {
            const std::string gnd = "data\\" + name + ".gnd";
            std::vector<uint8_t> gb;
            std::string gfrom, why;
            if (readFile(dir + "\\" + gnd, gb)) gfrom = "the data folder";
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
        }
        EnterCriticalSection(&shared_->lock);
        shared_->doneName = name;
        shared_->done = std::move(w);
        shared_->doneGround = std::move(ground);
        shared_->doneOk = ok;
        shared_->doneReady = true;
        LeaveCriticalSection(&shared_->lock);
    }
}

}  // namespace uagfx::gpu
