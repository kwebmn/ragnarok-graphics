#include "win/ambience_files.hpp"

#include <windows.h>

#include <memory>
#include <utility>

#include "win/game_files.hpp"
#include "win/log.hpp"

namespace uagfx::win::ambience_files {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

struct Shared {
    CRITICAL_SECTION lock;
    HANDLE wake = nullptr, thread = nullptr;
    std::string dir, wanted;
    bool asked = false, ready = false;
    MapSounds done;
    Shared() { InitializeCriticalSection(&lock); }
};

Shared& shared() {
    static Shared s;
    return s;
}

DWORD WINAPI run(void*) {
    Shared& s = shared();
    std::unique_ptr<gamefiles::Reader> reader;
    std::string readerDir;
    for (;;) {
        WaitForSingleObject(s.wake, INFINITE);
        EnterCriticalSection(&s.lock);
        const std::string dir = s.dir, map = s.wanted;
        const bool asked = s.asked;
        s.asked = false;
        LeaveCriticalSection(&s.lock);
        if (!asked || map.empty()) continue;
        if (!reader || readerDir != dir) {
            reader = std::make_unique<gamefiles::Reader>(dir);
            readerDir = dir;
        }
        MapSounds got;
        got.map = map;
        std::vector<uint8_t> bytes;
        rsw::World w;
        std::string from, error;
        if (reader->read("data\\" + map + ".rsw", bytes, &from) && rsw::parse(bytes.data(), bytes.size(), w, error)) {
            unsigned missing = 0;
            for (const rsw::Sound& snd : w.sounds) {
                const std::string file = lower(snd.file);
                if (file.empty() || got.files.count(file)) continue;
                std::vector<uint8_t> wav;
                if (reader->read("data\\wav\\" + snd.file, wav))
                    got.files[file] = std::move(wav);
                else
                    ++missing;
            }
            got.sounds = std::move(w.sounds);
            LOG_INFO("world sounds: %s - %u sounds of %u files (%u not found)", map.c_str(),
                     unsigned(got.sounds.size()), unsigned(got.files.size()), missing);
        }
        EnterCriticalSection(&s.lock);
        if (s.wanted == map) {  // not asked for another meanwhile
            s.done = std::move(got);
            s.ready = true;
        }
        LeaveCriticalSection(&s.lock);
    }
}

}  // namespace

void ask(const std::string& gameDir, const std::string& map) {
    Shared& s = shared();
    EnterCriticalSection(&s.lock);
    s.dir = gameDir;
    s.wanted = map;
    s.asked = true;
    s.ready = false;
    LeaveCriticalSection(&s.lock);
    if (!s.wake) s.wake = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (!s.thread && s.wake) s.thread = CreateThread(nullptr, 0, &run, nullptr, 0, nullptr);
    if (s.wake) SetEvent(s.wake);
}

bool take(MapSounds& out) {
    Shared& s = shared();
    EnterCriticalSection(&s.lock);
    const bool ready = s.ready;
    if (ready) {
        out = std::move(s.done);
        s.done = MapSounds{};
        s.ready = false;
    }
    LeaveCriticalSection(&s.lock);
    return ready;
}

}  // namespace uagfx::win::ambience_files
