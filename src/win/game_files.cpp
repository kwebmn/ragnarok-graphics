#include "win/game_files.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <utility>

#include "core/config.hpp"
#include "core/grf/grf.hpp"
#include "core/layout.hpp"
#include "core/updates.hpp"
#include "core/zip.hpp"
#include "win/log.hpp"

namespace uagfx::win::gamefiles {

bool readWhole(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[65536];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && out.size() < (size_t(64) << 20))
        out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return !out.empty();
}

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// 3.0.2-beta.5: the administrator's GRFs in updates\ (core/updates.hpp), looked in before DATA.INI's as the game
// does - none with [client] updates off
std::vector<std::string> updateFiles(const std::string& dir) {
    std::vector<uint8_t> ini;
    if (readWhole(layout::join(dir, layout::now(layout::File::Ini)), ini) &&
        !parseConfig(std::string(ini.begin(), ini.end())).clientUpdates)
        return {};
    std::vector<std::string> names;
    WIN32_FIND_DATAA f{};
    HANDLE h = FindFirstFileA(layout::join(layout::join(dir, updates::kFolder), "*.grf").c_str(), &f);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) names.push_back(f.cFileName);
        } while (FindNextFileA(h, &f));
        FindClose(h);
    }
    return names;
}

// DATA.INI's [Data] archives in the order of their numbers (the client looks in them in that order).
std::vector<std::string> dataArchives(const std::string& dir) {
    std::vector<uint8_t> b;
    if (!readWhole(dir + "\\DATA.INI", b)) return {};
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

std::vector<std::string> archives(const std::string& gameDir) {
    return updates::archiveOrder(updateFiles(gameDir), dataArchives(gameDir));
}

struct Reader::Archive {
    std::string path;
    bool grf = false, tried = false, ok = false;
    zip::Index zip;
    grf::Archive g;
};

namespace {

// 3.0.19-beta.44 (R-XHRKGN: 1.9 s stood still at the first wings - the HD data.grf's table read and its names folded
// on the game's thread, and again by each reader: the party's map names, the pick card, the reference): a GRF opened
// once a process - its table read, its names folded - and shared by every reader (a file handle each: openLike);
// read ahead on a thread of its own (warmUp)
// (MinGW here has no std::mutex: Windows' slim locks)
struct Held {
    explicit Held(SRWLOCK& l) : lock(l) { AcquireSRWLockExclusive(&lock); }
    ~Held() { ReleaseSRWLockExclusive(&lock); }
    Held(const Held&) = delete;
    Held& operator=(const Held&) = delete;
    SRWLOCK& lock;
};

struct Master {
    SRWLOCK lock = SRWLOCK_INIT;
    bool tried = false, ok = false;
    std::string why;
    grf::Archive archive;
};

Master& masterOf(const std::string& path) {
    static SRWLOCK lock = SRWLOCK_INIT;
    static std::map<std::string, std::unique_ptr<Master>> masters;
    Held held(lock);
    std::unique_ptr<Master>& m = masters[lower(path)];
    if (!m) m = std::make_unique<Master>();
    return *m;
}

bool openShared(const std::string& path, grf::Archive& out, std::string& why) {
    Master& m = masterOf(path);
    Held held(m.lock);  // the first opens it (the warm-up or a reader), the others wait
    if (!m.tried) {
        m.tried = true;
        m.ok = m.archive.openToRead(path, m.why);
        if (m.ok) m.archive.foldNames();
    }
    if (!m.ok) {
        why = m.why;
        return false;
    }
    return out.openLike(m.archive, why);
}

// The archives a reader of `dir` looks in (as Reader::read lists them): their paths and whether a GRF
std::vector<std::pair<std::string, bool>> archivePaths(const std::string& dir, bool updatesOnly) {
    std::vector<std::pair<std::string, bool>> out;
    for (const std::string& file : updatesOnly ? updateFiles(dir) : archives(dir)) {
        const std::string ext = lower(file.size() > 4 ? file.substr(file.size() - 4) : "");
        if (ext != ".grf" && ext != ".zip" && ext != ".gpf") continue;
        out.emplace_back(file.find(':') != std::string::npos ? file : dir + "\\" + file, ext != ".zip");
    }
    return out;
}

}  // namespace

namespace {
DWORD WINAPI warmUpThread(LPVOID p) {
    const std::unique_ptr<std::string> gameDir(static_cast<std::string*>(p));
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    Sleep(3000);  // the game's own start first
    const DWORD start = GetTickCount();
    int opened = 0, all = 0;
    for (const auto& [path, isGrf] : archivePaths(*gameDir, false)) {
        if (!isGrf) continue;
        ++all;
        grf::Archive reader;
        std::string why;
        if (openShared(path, reader, why)) ++opened;
    }
    LOG_INFO("game files: %d of %d archives' tables read ahead and their names folded in %lu ms - shared by every "
             "reader (none on the game's thread)", opened, all, (unsigned long)(GetTickCount() - start));
    return 0;
}
}  // namespace

void warmUp(const std::string& gameDir) {
    static LONG started = 0;
    if (InterlockedCompareExchange(&started, 1, 0) != 0) return;  // once a process
    auto* dir = new std::string(gameDir);
    if (HANDLE t = CreateThread(nullptr, 0, &warmUpThread, dir, 0, nullptr))
        CloseHandle(t);
    else
        delete dir;
}

Reader::Reader(std::string gameDir) : dir_(std::move(gameDir)) {}
Reader::Reader(std::string gameDir, bool updatesOnly) : dir_(std::move(gameDir)), updatesOnly_(updatesOnly) {}
Reader::~Reader() = default;

bool Reader::read(const std::string& entry, std::vector<uint8_t>& out, std::string* from) {
    if (readWhole(dir_ + "\\" + entry, out)) {
        if (from) *from = "the data folder";
        return true;
    }
    if (!listed_) {
        listed_ = true;
        for (const auto& [path, isGrf] : archivePaths(dir_, updatesOnly_)) {
            auto a = std::make_unique<Archive>();
            a->path = path;
            a->grf = isGrf;
            archives_.push_back(std::move(a));
        }
    }
    const std::string want = lower(entry);
    for (auto& ap : archives_) {
        Archive& a = *ap;
        if (!a.tried) {
            a.tried = true;
            std::string why;
            const auto all = [](const std::string&) { return true; };
            a.ok = a.grf ? openShared(a.path, a.g, why) : a.zip.open(a.path, all, why);  // 3.0.19-beta.44: shared
            if (!a.ok) LOG_INFO("game files: %s cannot be read (%s)", a.path.c_str(), why.c_str());
        }
        if (!a.ok) continue;
        std::string error;
        if (a.grf ? a.g.find(entry) && a.g.read(entry, out, error) : a.zip.has(want) && a.zip.read(want, out, error)) {
            if (from) *from = a.path.substr(a.path.find_last_of('\\') + 1);
            return true;
        }
    }
    return false;
}

}  // namespace uagfx::win::gamefiles
