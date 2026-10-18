#include "win/profiler.hpp"

#include <psapi.h>

#include <cstring>
#include <string>
#include <vector>

#include "core/sample_profile.hpp"
#include "win/log.hpp"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace uagfx::win {
namespace {

// The process's modules now (uaRO.exe first found by its handle; 2.5.5: ours by an address in it).
std::vector<profile::Module> modules(size_t& exe, size_t& ours) {
    std::vector<profile::Module> out;
    exe = 0;
    ours = size_t(-1);
    HMODULE own = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&modules), &own);
    HMODULE list[512];
    DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), list, sizeof list, &needed)) return out;
    const HMODULE self = GetModuleHandleW(nullptr);
    for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 512; ++i) {
        MODULEINFO info{};
        char name[MAX_PATH] = "";
        if (!K32GetModuleInformation(GetCurrentProcess(), list[i], &info, sizeof info)) continue;
        K32GetModuleBaseNameA(GetCurrentProcess(), list[i], name, MAX_PATH);
        if (list[i] == self) exe = out.size();
        if (list[i] == own) ours = out.size();
        out.push_back(profile::Module{name, uint32_t(uintptr_t(info.lpBaseOfDll)), uint32_t(info.SizeOfImage)});
    }
    return out;
}

// A return address into uaRO.exe: right after a call (E8 rel32; FF /2 by register, memory or [disp32]).
bool afterCall(uint32_t ret, uint32_t lo, uint32_t hi) {
    if (ret < lo + 8 || ret >= hi) return false;
    const auto* b = reinterpret_cast<const uint8_t*>(uintptr_t(ret));
    if (b[-5] == 0xE8) return true;
    if (b[-2] == 0xFF && (b[-1] & 0x38) == 0x10) return true;  // call reg / [reg]
    if (b[-3] == 0xFF && (b[-2] & 0x38) == 0x10) return true;  // call [reg+disp8]
    if (b[-6] == 0xFF && (b[-5] & 0x38) == 0x10) return true;  // call [disp32] / [reg+disp32]
    return false;
}

}  // namespace

Profiler& Profiler::get() {
    static Profiler p;
    return p;
}

void Profiler::run(DWORD gameThread, bool on) {
    InterlockedExchange(&on_, on ? 1 : 0);
    if (!on || thread_) return;
    game_ = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, gameThread);
    if (!game_) {
        LOG_ERROR("profile: the game's thread cannot be sampled (%lu)", GetLastError());
        return;
    }
    thread_ = CreateThread(nullptr, 0, &Profiler::loop, this, 0, nullptr);
    if (thread_) SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
    LOG_INFO("profile: sampling the game's thread (about 1000 a second; the tally every 10 s)");
}

void Profiler::stop() {
    if (!thread_) return;
    InterlockedExchange(&quit_, 1);
    WaitForSingleObject(thread_, 2000);
    CloseHandle(thread_);
    CloseHandle(game_);
    thread_ = game_ = nullptr;
}

DWORD WINAPI Profiler::loop(void* self) {
    static_cast<Profiler*>(self)->sampleLoop();
    return 0;
}

void Profiler::sampleLoop() {
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    profile::Tally tally;
    size_t exe = 0, own = 0;
    std::vector<profile::Module> mods = modules(exe, own);
    tally.setModules(mods, exe, own);
    const uint32_t lo = exe < mods.size() ? mods[exe].base : 0, hi = exe < mods.size() ? lo + mods[exe].size : 0;
    ULONGLONG since = GetTickCount64();
    while (!quit_) {
        if (!on_) {
            Sleep(200);
            since = GetTickCount64();
            tally.report(1.0);  // what was gathered before it went off is dropped
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
        uint32_t stack[64];
        bool got = false;
        if (SuspendThread(game_) != DWORD(-1)) {
            if (GetThreadContext(game_, &c)) {
                MEMORY_BASIC_INFORMATION mbi{};
                const auto sp = reinterpret_cast<const uint8_t*>(uintptr_t(c.Esp));
                got = VirtualQuery(sp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT &&
                      sp + sizeof stack <= static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
                if (got) std::memcpy(stack, sp, sizeof stack);
                else std::memset(stack, 0, sizeof stack);
                got = true;
            }
            ResumeThread(game_);
        }
        if (got) {
            const uint32_t at = uint32_t(c.Eip);
            uint32_t caller = 0;
            if (at - lo >= hi - lo)
                for (uint32_t v : stack)
                    if (afterCall(v, lo, hi)) {
                        caller = v;
                        break;
                    }
            tally.add(at, caller);
        }
        const ULONGLONG now = GetTickCount64();
        if (now - since >= 10000) {
            for (const std::string& line : tally.report(double(now - since) / 1000.0)) LOG_INFO("%s", line.c_str());
            since = now;
            mods = modules(exe, own);  // modules come and go (the graphics driver's): listed anew for the next tally
            tally.setModules(mods, exe, own);
        }
    }
    if (timer) CloseHandle(timer);
}

}  // namespace uagfx::win
