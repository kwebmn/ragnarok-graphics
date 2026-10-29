#include "win/reference_load.hpp"

#include <windows.h>

#include <ctime>
#include <string>
#include <vector>

#include "core/layout.hpp"
#include "core/reference_pack.hpp"
#include "core/reference_route.hpp"
#include "core/walk_grid.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::reference {
namespace {

constexpr ULONGLONG kLookEveryMs = 30000;

struct Loader {
    CRITICAL_SECTION lock;  // as the driver's other threads (no std::thread: no winpthread beside the game)
    Loader() { InitializeCriticalSection(&lock); }
    std::shared_ptr<const refindex::Index> current;
    bool busy = false;
    unsigned long long stamp = 0;  // pack.json's time of the last read; 0 none
    ULONGLONG lookedAt = 0;
    bool toldNone = false;
};

Loader& loader() {
    static Loader l;
    return l;
}

unsigned long long writeTime(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a)) return 0;
    return (static_cast<unsigned long long>(a.ftLastWriteTime.dwHighDateTime) << 32) | a.ftLastWriteTime.dwLowDateTime;
}

// 3.0.11-beta.8: the maps' walkable parts - the package's file, read once (by the pack's thread, one at a time)
std::shared_ptr<const walkgrid::Grids> walkable() {
    static std::shared_ptr<const walkgrid::Grids> grids;
    static bool tried = false;
    if (tried) return grids;
    tried = true;
    const std::string path = layout::join(gameDir(), layout::kWalkable);
    std::string file;
    auto read = std::make_shared<walkgrid::Grids>();
    if (readTextFile(path, file) && walkgrid::read(file, *read)) {
        grids = std::move(read);
        LOG_INFO("reference: the walkable parts of %zu maps (%s)", grids->size(), layout::kWalkable);
    } else {
        LOG_INFO("reference: %s cannot be read - the routes go by whole maps", path.c_str());
    }
    return grids;
}

DWORD WINAPI readPack(void*) {  // its own thread: the pack's JSON is a megabyte
    const ULONGLONG started = GetTickCount64();
    std::string text, gz, json;
    refpack::Manifest m;
    auto index = std::make_shared<refindex::Index>();
    const std::string manifest = layout::join(gameDir(), layout::kReferenceManifest);
    bool ok = readTextFile(manifest, text) && refpack::parse(text, m);
    if (ok) {
        const std::string pack = layout::join(gameDir(), std::string(layout::kReference) + "\\" + refpack::fileName(m));
        ok = readTextFile(pack, gz) && refpack::verify(gz, m) && refindex::gunzip(gz, json) &&
             refindex::build(json, *index);
        if (ok) {  // 3.0.11-beta.7: the client's names of the maps, for «Довідник»
            gamefiles::Reader files(gameDir());
            std::vector<uint8_t> bytes;
            if (files.read("data\\mapnametable.txt", bytes, nullptr))
                index->mapNames = refindex::mapNames(std::string(bytes.begin(), bytes.end()));
            index->walk = walkable();  // 3.0.11-beta.8: and the routes' graph over them, once a pack
            index->graph = refroute::prepare(*index);
        }
        if (ok)
            LOG_INFO("reference: the pack of %s (commit %s): %zu maps, %zu monsters, %zu route nodes, read in %llu ms",
                     refpack::dateOf(m).c_str(), m.commit.substr(0, 8).c_str(), index->maps.size(),
                     index->mobs.size(), index->graph ? index->graph->nodes.size() : size_t(0),
                     static_cast<unsigned long long>(GetTickCount64() - started));
        else
            LOG_INFO("reference: %s is not the pack its manifest names (or not whole) - none read", pack.c_str());
    }
    Loader& l = loader();
    EnterCriticalSection(&l.lock);
    if (ok) l.current = std::move(index);
    l.busy = false;
    LeaveCriticalSection(&l.lock);
    return 0;
}

struct Live {  // 3.0.11-beta.10: online.json, small - read on the game's thread when it changes
    std::shared_ptr<const reflive::Online> current;
    unsigned long long stamp = 0;
    ULONGLONG lookedAt = 0;
};

}  // namespace

std::shared_ptr<const reflive::Online> online() {
    static Live l;  // the game's thread alone (the window, the minimap)
    const ULONGLONG now = GetTickCount64();
    if (l.lookedAt == 0 || now - l.lookedAt >= kLookEveryMs) {
        l.lookedAt = now;
        const std::string path = layout::join(gameDir(), layout::kReferenceOnline);
        const unsigned long long stamp = writeTime(path);
        std::string text;
        auto read = std::make_shared<reflive::Online>();
        if (stamp && stamp != l.stamp && readTextFile(path, text) && reflive::read(text, *read)) {
            l.stamp = stamp;
            l.current = std::move(read);
        }
    }
    if (!l.current || !reflive::fresh(*l.current, static_cast<long long>(std::time(nullptr)))) return nullptr;
    return l.current;
}

std::shared_ptr<const refindex::Index> index() {
    Loader& l = loader();
    EnterCriticalSection(&l.lock);
    const ULONGLONG now = GetTickCount64();
    if (!l.busy && (l.lookedAt == 0 || now - l.lookedAt >= kLookEveryMs)) {
        l.lookedAt = now;
        const unsigned long long stamp = writeTime(layout::join(gameDir(), layout::kReferenceManifest));
        if (!stamp && !l.toldNone) {
            l.toldNone = true;
            LOG_INFO("reference: none here yet (%s) - the game's watcher fetches it with the new minimap on",
                     layout::kReferenceManifest);
        }
        if (stamp && stamp != l.stamp) {
            const HANDLE t = CreateThread(nullptr, 0, &readPack, nullptr, 0, nullptr);
            if (t) {
                l.stamp = stamp;
                l.busy = true;
                CloseHandle(t);
            }
        }
    }
    std::shared_ptr<const refindex::Index> current = l.current;
    LeaveCriticalSection(&l.lock);
    return current;
}

}  // namespace uagfx::win::reference
