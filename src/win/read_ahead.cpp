#include "win/read_ahead.hpp"

#include <windows.h>

#include <ctime>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/layout.hpp"
#include "core/read_ahead.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/updates_hook.hpp"

namespace uagfx::win::readaheadwin {
namespace {

constexpr ULONGLONG kSaveMs = 60000;     // the file written at most this often
constexpr ULONGLONG kMapDelayMs = 2000;  // a map's read ahead after its first frames (the loader's reads done)
constexpr size_t kMostBytes = size_t(64) << 20;

struct Job {
    std::string gameDir, map;
    std::vector<std::string> files;
};

LONG g_running = 0;

DWORD WINAPI run(LPVOID p) {
    const std::unique_ptr<Job> job(static_cast<Job*>(p));
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);  // its disk reads the lowest too
    const ULONGLONG from = GetTickCount64();
    gamefiles::Reader reader(job->gameDir);
    size_t bytes = 0;
    int read = 0, missing = 0;
    std::vector<uint8_t> data;
    for (const std::string& f : job->files) {
        if (bytes >= kMostBytes) break;
        data.clear();
        if (reader.read(f, data)) {
            ++read;
            bytes += data.size();
        } else {
            ++missing;
        }
    }
    LOG_INFO("read ahead: %d file(s) for %s (%.1f MB) in %llu ms, before the game asks for them%s", read,
             job->map.empty() ? "the start" : job->map.c_str(), double(bytes) / 1048576.0,
             static_cast<unsigned long long>(GetTickCount64() - from),
             missing ? (", " + std::to_string(missing) + " not found").c_str() : "");
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    InterlockedExchange(&g_running, 0);
    return 0;
}

struct State {
    bool loaded = false, changed = false;
    readahead::Learned learned;
    std::set<std::string> asked;  // read ahead (or begun) this game
    std::string map = "\x01";     // not a map's name: the first frame starts the login screens' read ahead
    ULONGLONG mapAt = 0, savedAt = 0;
    bool mapPending = false;
};

State& state() {
    static State s;
    return s;
}

std::string path() { return layout::join(gameDir(), layout::kReadAhead); }

// The files of `map` not asked yet, on a thread of its own; false while one runs (the next map asks again)
bool begin(State& s, const std::string& map) {
    if (InterlockedCompareExchange(&g_running, 1, 0) != 0) return false;
    auto* job = new Job{gameDir(), map, {}};
    for (const std::string& f : s.learned.forMap(map))
        if (s.asked.insert(f).second) job->files.push_back(f);
    if (job->files.empty()) {
        delete job;
        InterlockedExchange(&g_running, 0);
        return true;
    }
    if (HANDLE t = CreateThread(nullptr, 0, &run, job, 0, nullptr)) {
        CloseHandle(t);
        return true;
    }
    for (const std::string& f : job->files) s.asked.erase(f);
    delete job;
    InterlockedExchange(&g_running, 0);
    return false;
}

}  // namespace

void frame(const std::string& map) {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (!s.loaded) {
        s.loaded = true;
        std::string text;
        if (readTextFile(path(), text)) s.learned = readahead::Learned::parse(text);
        LOG_INFO("read ahead: %zu slow file(s) of %zu map(s) kept from the games before (%s)", s.learned.size(),
                 s.learned.maps(), layout::kReadAhead);
    }
    for (const std::string& f : takeSlowReads()) {
        s.learned.note(map, f, uint64_t(std::time(nullptr)));
        s.changed = true;
    }
    if (map != s.map) {
        s.map = map;
        s.mapAt = now;
        s.mapPending = true;
    }
    if (s.mapPending && now - s.mapAt >= (map.empty() ? 0 : kMapDelayMs) && begin(s, map)) s.mapPending = false;
    if (s.changed && now - s.savedAt >= kSaveMs) {
        s.savedAt = now;
        s.changed = false;
        if (!writeFileAtomic(path(), s.learned.text())) LOG_INFO("read ahead: %s could not be written", path().c_str());
    }
}

}  // namespace uagfx::win::readaheadwin
