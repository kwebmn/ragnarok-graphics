#include "win/profiler.hpp"

#include <psapi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/pe_exports.hpp"
#include "core/sample_profile.hpp"
#include "core/stall_watch.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/stall_watch.hpp"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace uagfx::win {
namespace {

constexpr size_t kStackWords = 1024;  // 3.0.2-beta.12: 4 KB of the stack, deep enough for the game's steps
constexpr size_t kFrames = 48;        // the return addresses looked at
constexpr int kStallsTold = 30;       // 3.0.11-beta.94: the stalls a game tells, at most

// Our own memory read: a place no longer mapped (a module gone meanwhile) fails instead of faulting
bool readSelf(uint32_t address, void* out, size_t size) {
    SIZE_T got = 0;
    return address &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(address)), out, size, &got) &&
           got == size;
}

struct Module {
    std::string name;
    uint32_t base = 0, size = 0;
    uint32_t codeBegin = 0, codeEnd = 0;  // its code, absolute
    bool windows = false;                 // one of Windows' own (its folder's), not the game folder's
    bool exportsRead = false;
    std::vector<pe::Export> exports;
};

// The process's modules by their places, with what a sample's classing needs (3.0.2-beta.12); uaRO.exe found by its
// handle, ours (2.5.5) by an address in it
class Modules {
public:
    void list() {
        std::map<std::string, std::vector<pe::Export>> known;  // the functions read before, by module and place
        for (Module& m : mods_)
            if (m.exportsRead) known[keyOf(m)] = std::move(m.exports);
        mods_.clear();
        calls_.clear();
        exe_ = own_ = -1;
        HMODULE own = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&readSelf), &own);
        char windows[MAX_PATH] = "";
        const UINT windowsLength = GetSystemWindowsDirectoryA(windows, MAX_PATH);
        HMODULE list[512];
        DWORD needed = 0;
        if (!K32EnumProcessModules(GetCurrentProcess(), list, sizeof list, &needed)) return;
        const HMODULE self = GetModuleHandleW(nullptr);
        std::vector<std::pair<HMODULE, Module>> found;
        for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 512; ++i) {
            MODULEINFO info{};
            char path[MAX_PATH] = "";
            if (!K32GetModuleInformation(GetCurrentProcess(), list[i], &info, sizeof info)) continue;
            K32GetModuleFileNameExA(GetCurrentProcess(), list[i], path, MAX_PATH);
            Module m;
            const char* slash = std::strrchr(path, '\\');
            m.name = slash ? slash + 1 : path;
            m.base = uint32_t(uintptr_t(info.lpBaseOfDll));
            m.size = uint32_t(info.SizeOfImage);
            m.windows = windowsLength > 0 && windowsLength < MAX_PATH &&
                        _strnicmp(path, windows, windowsLength) == 0 && path[windowsLength] == '\\';
            uint32_t begin = 0, end = 0;
            if (pe::codeOf(reader(m.base, m.size), begin, end)) {
                m.codeBegin = m.base + begin;
                m.codeEnd = m.base + std::min(end, m.size);
            }
            const auto was = known.find(keyOf(m));
            if (was != known.end()) {
                m.exports = std::move(was->second);
                m.exportsRead = true;
            }
            found.emplace_back(list[i], std::move(m));
        }
        std::sort(found.begin(), found.end(),
                  [](const auto& a, const auto& b) { return a.second.base < b.second.base; });
        for (auto& [handle, m] : found) {
            if (handle == self) exe_ = int(mods_.size());
            if (handle == own) own_ = int(mods_.size());
            mods_.push_back(std::move(m));
        }
    }

    int at(uint32_t address) const {  // the module a place is in, -1 none
        const auto it = std::upper_bound(mods_.begin(), mods_.end(), address,
                                         [](uint32_t a, const Module& m) { return a < m.base; });
        if (it == mods_.begin()) return -1;
        const Module& m = *(it - 1);
        return address - m.base < m.size ? int(it - 1 - mods_.begin()) : -1;
    }
    const Module& operator[](int i) const { return mods_[size_t(i)]; }
    int exe() const { return exe_; }

    std::vector<profile::Module> forTally() const {
        std::vector<profile::Module> out;
        for (const Module& m : mods_) out.push_back({m.name, m.base, m.size});
        return out;
    }
    size_t exeIndex() const { return exe_ >= 0 ? size_t(exe_) : 0; }
    size_t ownIndex() const { return own_ >= 0 ? size_t(own_) : size_t(-1); }
    int own() const { return own_; }
    // 3.0.6-beta.3: our module's functions by its unwind table (read once: our module stays where it is)
    const std::vector<pe::Function>& ownFunctions() {
        if (!ownRead_ && own_ >= 0) {
            ownRead_ = true;
            ownFunctions_ = pe::functionsOf(reader(mods_[size_t(own_)].base, mods_[size_t(own_)].size));
        }
        return ownFunctions_;
    }

    // Windows' function a place is in, by its module's exports (read the first time asked); "" when the nearest one
    // before it is too far to be its own (3.0.2-beta.13: 1 KB - beta.12 named the bridge's wait in ntdll's code of
    // its own after an export far before it)
    std::string functionAt(int i, uint32_t address) {
        Module& m = mods_[size_t(i)];
        if (!m.exportsRead) {
            m.exportsRead = true;
            m.exports = pe::exportsOf(reader(m.base, m.size));
        }
        const pe::Export* e = pe::exportAt(m.exports, address - m.base);
        return e && address - m.base - e->rva < 0x400 ? e->name : std::string();
    }

    // A return address: right after a call (E8 rel32; FF /2 by register, memory or [disp32]); remembered
    bool afterCall(uint32_t ret) {
        const auto it = calls_.find(ret);
        if (it != calls_.end()) return it->second;
        uint8_t b[8];  // b[7] = ret[-1]
        const bool call = readSelf(ret - 8, b, sizeof b) &&
                          (b[3] == 0xE8 || (b[6] == 0xFF && (b[7] & 0x38) == 0x10) ||
                           (b[5] == 0xFF && (b[6] & 0x38) == 0x10) || (b[2] == 0xFF && (b[3] & 0x38) == 0x10));
        if (calls_.size() > 200000) calls_.clear();
        calls_.emplace(ret, call);
        return call;
    }

private:
    static std::string keyOf(const Module& m) {
        return m.name + "@" + std::to_string(m.base) + "+" + std::to_string(m.size);
    }
    static pe::Read reader(uint32_t base, uint32_t size) {
        return [base, size](uint32_t rva, void* out, size_t n) {
            return rva <= size && n <= size - rva && readSelf(base + rva, out, n);
        };
    }
    std::vector<Module> mods_;
    int exe_ = -1, own_ = -1;
    std::unordered_map<uint32_t, bool> calls_;
    bool ownRead_ = false;
    std::vector<pe::Function> ownFunctions_;
};

// What a sample is (3.0.2-beta.12): its return addresses on the stack's copy, innermost first; outside uaRO.exe the
// uaRO.exe place that called out (0.87); in Windows' code the outermost of its functions before the game's or a
// library's ("Sleep", "WaitForSingleObject from ddraw.dll"); the game's step it is under
profile::Sample classify(Modules& mods, uint32_t at, const uint32_t* stack, size_t words, bool steps) {
    profile::Sample s;
    s.at = at;
    uint32_t frames[kFrames];
    size_t n = 0;
    for (size_t i = 0; i < words && n < kFrames; ++i) {
        const uint32_t v = stack[i];
        const int m = mods.at(v);
        if (m < 0 || v < mods[m].codeBegin + 8 || v >= mods[m].codeEnd || !mods.afterCall(v)) continue;
        frames[n++] = v;
    }
    const int here = mods.at(at);
    if (here != mods.exe())
        for (size_t k = 0; k < n; ++k)
            if (mods.at(frames[k]) == mods.exe()) {
                s.caller = frames[k];
                break;
            }
    if (here >= 0 && here == mods.own()) s.own = at;  // 3.0.6-beta.3: in our module
    if (here >= 0 && mods[here].windows) {
        int inner = here, from = -1;
        uint32_t place = at;
        for (size_t k = 0; k < n; ++k) {
            const int m = mods.at(frames[k]);
            if (!mods[m].windows) {
                from = m;
                if (m == mods.own()) s.own = frames[k];  // 3.0.6-beta.3: Windows' code our function called
                break;
            }
            inner = m;
            place = frames[k];
        }
        s.where = mods.functionAt(inner, place);
        if (s.where.empty()) s.where = mods[inner].name;
        if (from >= 0 && from != mods.exe()) s.where += " from " + mods[from].name;
    }
    if (steps) s.step = profile::stepOf(at, frames, n);
    return s;
}

// 3.0.11-beta.94: the game at a stall's start - in front or behind, its process's priority (uaRO.exe drops it to
// idle behind other windows, FUN_00654190), minimised
std::string howTheGame(HWND game) {
    const DWORD c = GetPriorityClass(GetCurrentProcess());
    const char* priority = c == IDLE_PRIORITY_CLASS           ? "idle"
                           : c == BELOW_NORMAL_PRIORITY_CLASS ? "below normal"
                           : c == NORMAL_PRIORITY_CLASS       ? "normal"
                           : c == ABOVE_NORMAL_PRIORITY_CLASS ? "above normal"
                           : c == HIGH_PRIORITY_CLASS         ? "high"
                                                              : "realtime";
    std::string how = game && GetForegroundWindow() == game ? "in front" : "behind other windows";
    if (game && IsIconic(game)) how += ", minimised";
    return how + ", its priority " + priority;
}

void say(const std::string& prefix, const std::string& head, const std::vector<std::string>& more) {
    LOG_INFO("%s: %s", prefix.c_str(), head.c_str());
    for (const std::string& line : more) LOG_INFO("%s", line.c_str());
}

}  // namespace

Profiler& Profiler::get() {
    static Profiler p;
    return p;
}

bool Profiler::start(DWORD gameThread) {
    if (thread_) return true;
    if (failed_) return false;
    game_ = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, gameThread);
    wake_ = game_ ? CreateEventW(nullptr, FALSE, FALSE, nullptr) : nullptr;
    if (wake_) {
        InitializeCriticalSection(&lock_);
        thread_ = CreateThread(nullptr, 0, &Profiler::loop, this, 0, nullptr);
    }
    if (!thread_) {
        failed_ = true;
        LOG_ERROR("profile: the game's thread cannot be sampled (%lu)", GetLastError());
        if (game_) CloseHandle(game_);
        if (wake_) CloseHandle(wake_);
        game_ = wake_ = nullptr;
        return false;
    }
    SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
    return true;
}

void Profiler::run(DWORD gameThread, bool on) {
    const LONG was = InterlockedExchange(&on_, on ? 1 : 0);
    if (!on || was || !start(gameThread)) return;
    LOG_INFO("profile: sampling the game's thread (about 1000 a second; the tally every 10 s)");
    SetEvent(wake_);
}

void Profiler::open(DWORD gameThread) {
    if (start(gameThread) && InterlockedCompareExchange(&window_, kOpen, kShut) == kShut) SetEvent(wake_);
}

// Only the game's thread opens and closes the window (the sampling thread shuts it after its report): open here
// stays open until we close it.
void Profiler::close(const std::string& prefix, const std::string& head, const std::vector<std::string>& more) {
    if (!thread_ || window_ != kOpen)  // not started, or the last one still being written when this one began
        return say(prefix, head + "; the game's thread not sampled", more);
    EnterCriticalSection(&lock_);
    prefix_ = prefix;
    head_ = head;
    more_ = more;
    LeaveCriticalSection(&lock_);
    InterlockedExchange(&window_, kReport);
    SetEvent(wake_);
}

void Profiler::drop() {
    if (thread_ && window_ == kOpen) {
        InterlockedExchange(&window_, kDrop);
        SetEvent(wake_);
    }
}

void Profiler::watch(DWORD gameThread, HWND window) {
    gameWindow_ = window;
    if (watchAsked_) return;
    watchAsked_ = true;
    uint32_t beats = 0;
    if (!stallBeats(beats)) {
        LOG_INFO("stall watch: off - uaRO.exe's PeekMessageA not hooked");
        return;
    }
    if (!start(gameThread)) return;
    setStallThread(gameThread);
    InterlockedExchange(&watching_, 1);
    LOG_INFO("stall watch: the game's thread watched - still for a second, where it stands is sampled");
    SetEvent(wake_);
}

void Profiler::stop() {
    if (!thread_) return;
    InterlockedExchange(&quit_, 1);
    SetEvent(wake_);
    WaitForSingleObject(thread_, 2000);
    CloseHandle(thread_);
    CloseHandle(game_);
    CloseHandle(wake_);
    thread_ = game_ = wake_ = nullptr;
}

DWORD WINAPI Profiler::loop(void* self) {
    static_cast<Profiler*>(self)->sampleLoop();
    return 0;
}

void Profiler::sampleLoop() {
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    Modules mods;
    profile::Tally tally;   // [debug] profile's, every 10 s
    profile::Tally window;  // 3.0.2-beta.12: a map's loading
    profile::Tally still;   // 3.0.11-beta.94: a stall of the game's thread
    stallwatch::Watch stall;
    std::string stillHow;  // the game at the stall's start
    int stallsTold = 0;
    bool wasOn = false, windowLive = false, stillLive = false;
    const bool steps = knownGameBuild();
    ULONGLONG since = GetTickCount64();
    uint32_t stack[kStackWords];
    while (!quit_) {
        const LONG w = window_;
        if (w == kReport || w == kDrop) {
            if (w == kReport) {
                EnterCriticalSection(&lock_);
                const std::string prefix = prefix_, head = head_;
                const std::vector<std::string> more = more_;
                LeaveCriticalSection(&lock_);
                char sampled[64];
                std::snprintf(sampled, sizeof sampled, "; the game's thread sampled %zu times", window.samples());
                if (!window.samples()) say(prefix, head + "; the game's thread not sampled", {});
                for (const std::string& line : window.report(prefix, head + sampled)) LOG_INFO("%s", line.c_str());
                for (const std::string& line : more) LOG_INFO("%s", line.c_str());
            }
            window.report(1.0);  // what is left: dropped
            windowLive = false;
            InterlockedExchange(&window_, kShut);
            continue;
        }
        const bool on = on_ != 0, open = w == kOpen;
        if (on != wasOn) {
            wasOn = on;
            since = GetTickCount64();
            tally.report(1.0);  // what was gathered before it went off is dropped
            if (on) {
                mods.list();
                tally.setModules(mods.forTally(), mods.exeIndex(), mods.ownIndex());
                tally.setOwnFunctions(mods.ownFunctions());
            }
        }
        if (open && !windowLive) {
            windowLive = true;
            mods.list();  // the modules of now: the map's own may have come
            window.setModules(mods.forTally(), mods.exeIndex(), mods.ownIndex());
            window.setOwnFunctions(mods.ownFunctions());
        }
        // 3.0.11-beta.94: the game's thread still for a second - sampled until it beats again
        uint32_t beats = 0;
        if (watching_ && stallBeats(beats)) {
            const stallwatch::Event e = stall.look(beats, GetTickCount64());
            if (e == stallwatch::Event::Began && stallsTold < kStallsTold && !open) {  // a map's load: its own
                mods.list();
                still.setModules(mods.forTally(), mods.exeIndex(), mods.ownIndex());
                still.setOwnFunctions(mods.ownFunctions());
                stillHow = howTheGame(gameWindow_);
                stillLive = true;
            } else if (e == stallwatch::Event::Ended && stillLive) {
                stillLive = false;
                char head[200];
                std::snprintf(head, sizeof head, "stood still %.1f s (%s); sampled %zu times",
                              double(stall.stillMs()) / 1000.0, stillHow.c_str(), still.samples());
                for (const std::string& line : still.report("the game's thread", head)) LOG_INFO("%s", line.c_str());
                still.report(1.0);  // emptied for the next one
                if (++stallsTold == kStallsTold) LOG_INFO("stall watch: %d stalls told - the rest not", kStallsTold);
            }
        }
        if (!on && !open && !stillLive) {
            WaitForSingleObject(wake_, watching_ ? 250 : 1000);
            continue;
        }
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -10000;  // 1 ms
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, 100);
        } else {
            Sleep(1);
        }
        // The game's thread stopped only for these reads: no lock, no allocation in between (it may hold the
        // heap's or the loader's).
        CONTEXT c{};
        c.ContextFlags = CONTEXT_CONTROL;
        size_t words = 0;
        bool got = false;
        if (SuspendThread(game_) != DWORD(-1)) {
            if (GetThreadContext(game_, &c)) {
                MEMORY_BASIC_INFORMATION mbi{};
                const auto* sp = reinterpret_cast<const uint8_t*>(uintptr_t(c.Esp));
                if (VirtualQuery(sp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT) {
                    const auto* end = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
                    words = std::min(kStackWords, size_t(end - sp) / 4);
                    std::memcpy(stack, sp, words * 4);
                }
                got = true;
            }
            ResumeThread(game_);
        }
        if (got) {
            const profile::Sample s = classify(mods, uint32_t(c.Eip), stack, words, steps);
            if (on) tally.add(s);
            if (open) window.add(s);
            if (stillLive) still.add(s);
        }
        if (on) {
            const ULONGLONG now = GetTickCount64();
            if (now - since >= 10000) {
                for (const std::string& line : tally.report(double(now - since) / 1000.0))
                    LOG_INFO("%s", line.c_str());
                since = now;
                mods.list();  // modules come and go (the graphics driver's): listed anew for the next tally
                tally.setModules(mods.forTally(), mods.exeIndex(), mods.ownIndex());
                tally.setOwnFunctions(mods.ownFunctions());
            }
        }
    }
    if (timer) CloseHandle(timer);
}

}  // namespace uagfx::win
