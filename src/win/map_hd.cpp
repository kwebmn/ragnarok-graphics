#include "win/map_hd.hpp"

#include <windows.h>

#include <utility>

#include "core/layout.hpp"
#include "core/pack_source.hpp"
#include "core/texture_pack.hpp"
#include "core/webp_image.hpp"
#include "core/zip.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::maphd {
namespace {

// "texture\유저인터페이스\map\" as texture_x4.zip names it (UTF-8, its names' flag), lower case as zip::normalName
const std::string kZipFolder =
    "texture\\\xEC\x9C\xA0\xEC\xA0\x80\xEC\x9D\xB8\xED\x84\xB0\xED\x8E\x98\xEC\x9D\xB4\xEC\x8A\xA4\\map\\";
constexpr int kLeast = 1024;  // a side at least: smaller is no better than the game's own

struct Worker {
    CRITICAL_SECTION lock;  // as the driver's other threads (no std::thread: no winpthread beside the game)
    Worker() { InitializeCriticalSection(&lock); }
    bool busy = false;
    std::string asked;  // the request waiting
    std::vector<uint8_t> bmp;
    bool waiting = false;
    std::string doneMap;  // the last made, till handed
    std::shared_ptr<const minimap::MapPicture> done;
    // the thread's alone (one at a time: busy)
    bool packTried = false, zipsTried = false;
    std::unique_ptr<zip::Index> pack;
    std::vector<std::unique_ptr<zip::Index>> zips;
    std::vector<std::string> zipNames;
};

Worker& worker() {
    static Worker w;
    return w;
}

std::string lower(std::string s) {
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

// The AI pack's interface pictures, its directory read once a game
void openPack(Worker& w) {
    if (w.packTried) return;
    w.packTried = true;
    const std::string path = ourFile(gameDir(), layout::File::Pack);
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        LOG_INFO("minimap: no AI texture pack (%s) - the maps' bigger pictures from the game's zips alone",
                 path.c_str());
        return;
    }
    const std::string ui = std::string(pack::groupName(pack::Group::Ui)) + "\\";
    auto index = std::make_unique<zip::Index>();
    std::string why;
    const ULONGLONG t0 = GetTickCount64();
    if (!index->open(path, [&ui](const std::string& n) { return n.compare(0, ui.size(), ui) == 0; }, why)) {
        LOG_INFO("minimap: the AI texture pack cannot be read for the maps' pictures (%s)", why.c_str());
        return;
    }
    LOG_INFO("minimap: the AI texture pack's %zu interface pictures known in %llu ms (the maps' bigger pictures)",
             index->count(), static_cast<unsigned long long>(GetTickCount64() - t0));
    w.pack = std::move(index);
}

// DATA.INI's zips with maps' pictures (the old client's texture_x4.zip), their directories read once a game
void openZips(Worker& w) {
    if (w.zipsTried) return;
    w.zipsTried = true;
    const std::string dir = gameDir();
    for (const std::string& file : gamefiles::archives(dir)) {
        if (lower(file.size() > 4 ? file.substr(file.size() - 4) : "") != ".zip") continue;
        const std::string path = file.find(':') != std::string::npos ? file : dir + "\\" + file;
        auto index = std::make_unique<zip::Index>();
        std::string why;
        if (!index->open(path, [](const std::string& n) { return n.compare(0, kZipFolder.size(), kZipFolder) == 0; },
                         why) ||
            index->count() == 0)
            continue;
        LOG_INFO("minimap: %s has %zu maps' pictures", file.c_str(), index->count());
        w.zips.push_back(std::move(index));
        w.zipNames.push_back(file);
    }
}

std::shared_ptr<const minimap::MapPicture> find(Worker& w, const std::string& map, const std::vector<uint8_t>& bmp) {
    const ULONGLONG t0 = GetTickCount64();
    std::vector<uint8_t> file;
    std::string from, why;
    openPack(w);
    if (w.pack && !bmp.empty()) {  // by the keys the pack's builder gave the game's own picture
        std::vector<std::string> keys;
        pack::Picture p;
        if (pack::interfacePicture(bmp.data(), bmp.size(), p)) keys.push_back(p.key);
        if (pack::worldTexture(bmp.data(), bmp.size(), false, p)) keys.push_back(p.key);
        for (const std::string& key : keys) {
            const std::string name = zip::normalName(pack::pictureName(pack::Group::Ui, key));
            if (w.pack->has(name) && w.pack->read(name, file, why)) {
                from = "the AI texture pack (" + name + ")";
                break;
            }
        }
    }
    if (file.empty()) {
        openZips(w);
        const std::string name = kZipFolder + lower(map) + ".webp";
        for (size_t i = 0; i < w.zips.size() && file.empty(); ++i)
            if (w.zips[i]->has(name) && w.zips[i]->read(name, file, why)) from = w.zipNames[i];
    }
    if (file.empty()) {
        LOG_INFO("minimap: %s - no picture bigger than the game's (the AI pack %s, %zu zips with maps' pictures)",
                 map.c_str(), w.pack ? "has none of it" : "not here", w.zips.size());
        return nullptr;
    }
    std::vector<uint32_t> argb;
    int width = 0, height = 0;
    if (!webp::decode(file.data(), file.size(), argb, width, height) || width < kLeast || height < kLeast) {
        LOG_INFO("minimap: %s's picture from %s not taken (%dx%d)", map.c_str(), from.c_str(), width, height);
        return nullptr;
    }
    auto made = std::make_shared<minimap::MapPicture>(minimap::prepare(argb, width, height));
    LOG_INFO("minimap: %s's picture %dx%d from %s, made in %llu ms", map.c_str(), width, height, from.c_str(),
             static_cast<unsigned long long>(GetTickCount64() - t0));
    return made;
}

DWORD WINAPI work(void*) {
    Worker& w = worker();
    for (;;) {
        EnterCriticalSection(&w.lock);
        if (!w.waiting) {
            w.busy = false;
            LeaveCriticalSection(&w.lock);
            return 0;
        }
        const std::string map = w.asked;
        const std::vector<uint8_t> bmp = std::move(w.bmp);
        w.bmp.clear();
        w.waiting = false;
        LeaveCriticalSection(&w.lock);
        std::shared_ptr<const minimap::MapPicture> made = find(w, map, bmp);
        EnterCriticalSection(&w.lock);
        w.doneMap = map;
        w.done = std::move(made);
        LeaveCriticalSection(&w.lock);
    }
}

}  // namespace

void ask(const std::string& map, std::vector<uint8_t> bmp) {
    Worker& w = worker();
    EnterCriticalSection(&w.lock);
    w.asked = map;
    w.bmp = std::move(bmp);
    w.waiting = true;
    if (!w.busy) {
        const HANDLE t = CreateThread(nullptr, 0, &work, nullptr, 0, nullptr);
        if (t) {
            w.busy = true;
            CloseHandle(t);
        } else {
            w.waiting = false;
            LOG_INFO("minimap: no thread for %s's bigger picture (%lu)", map.c_str(), GetLastError());
        }
    }
    LeaveCriticalSection(&w.lock);
}

std::shared_ptr<const minimap::MapPicture> take(const std::string& map) {
    Worker& w = worker();
    EnterCriticalSection(&w.lock);
    std::shared_ptr<const minimap::MapPicture> out;
    if (w.done && w.doneMap == map) out = std::move(w.done);
    w.done.reset();  // another map's: not kept
    LeaveCriticalSection(&w.lock);
    return out;
}

}  // namespace uagfx::win::maphd
