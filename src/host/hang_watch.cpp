#include "host/hang_watch.hpp"

#include <windows.h>

#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/bridge_protocol.hpp"
#include "core/stack_scan.hpp"
#include "win/log.hpp"

namespace uagfx::host::hangwatch {
namespace {

constexpr size_t kWords = 8192;  // the stack's top 64 KB
constexpr DWORD kEveryMs = 250;
constexpr uint64_t kTellAtMs[] = {2000, 6000, 20000};
constexpr size_t kTells = sizeof kTellAtMs / sizeof kTellAtMs[0];
// the images' list read again so long after the watch's start (the renderer's own libraries, DLSS's some eight
// seconds in, what a map brings), then every two minutes
constexpr uint64_t kModulesAtMs[] = {3000, 12000, 40000};
constexpr uint64_t kModulesEveryMs = 120000;
constexpr size_t kMostCalls = 20, kMostNewImages = 16;  // a log's line holds 1023 characters

HANDLE g_main = nullptr, g_thread = nullptr, g_stop = nullptr;
uint64_t g_stackLow = 0, g_stackTop = 0;  // the watched thread's stack, the whole of its room
std::atomic<uint64_t> g_since{0};  // the tick its message was taken at; 0: none
std::atomic<uint32_t> g_what{0}, g_serial{0}, g_toldSerial{0};
// the watch thread's own
std::vector<stackscan::Module> g_modules;
uint64_t g_words[kWords];

// The loaded images, read while the watched thread moves: the list is the loader's (its lock) - never asked of a
// thread that may stand inside it
void readModules() {
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    std::vector<stackscan::Module> now;
    MODULEENTRY32W e{};
    e.dwSize = sizeof e;
    for (BOOL more = Module32FirstW(snap, &e); more; more = Module32NextW(snap, &e)) {
        char name[96] = {};
        WideCharToMultiByte(CP_UTF8, 0, e.szModule, -1, name, int(sizeof name) - 1, nullptr, nullptr);
        now.push_back(stackscan::Module{uint64_t(uintptr_t(e.modBaseAddr)), e.modBaseSize, name});
    }
    CloseHandle(snap);
    if (!now.empty()) g_modules.swap(now);
}

bool readSelf(void*, uint64_t at, uint8_t* out, size_t size) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), out, size, &got) &&
           got == size;
}

bool known(uint64_t address, const std::vector<stackscan::Module>& modules) {
    for (const stackscan::Module& m : modules)
        if (address >= m.base && address - m.base < m.size) return true;
    return false;
}

// An image loaded since the list was read (the thread may stand in its very loading): by Windows' word of the
// address - no loader's list, no name
void addImageOf(uint64_t address, std::vector<stackscan::Module>& modules) {
    MEMORY_BASIC_INFORMATION at{};
    if (!VirtualQuery(reinterpret_cast<const void*>(uintptr_t(address)), &at, sizeof at) || at.Type != MEM_IMAGE ||
        !at.AllocationBase)
        return;
    const uint64_t base = uint64_t(uintptr_t(at.AllocationBase));
    uint64_t end = base;
    for (int regions = 0; regions < 64; ++regions) {
        MEMORY_BASIC_INFORMATION r{};
        if (!VirtualQuery(reinterpret_cast<const void*>(uintptr_t(end)), &r, sizeof r) ||
            r.AllocationBase != at.AllocationBase || !r.RegionSize)
            break;
        end = uint64_t(uintptr_t(r.BaseAddress)) + r.RegionSize;
    }
    if (end <= base) return;
    char name[48];
    std::snprintf(name, sizeof name, "(an image at 0x%llx)", static_cast<unsigned long long>(base));
    modules.push_back(stackscan::Module{base, end - base, name});
}

const char* nameOf(uint32_t what) {
    switch (bridge::Msg(what)) {
        case bridge::Msg::Execute: return "a frame's draws";
        case bridge::Msg::Present: return "a present";
        case bridge::Msg::Map: return "a map's name";
        case bridge::Msg::SetScale: return "the picture's size";
        case bridge::Msg::SetTarget: return "the target's size";
        case bridge::Msg::SetEffects: return "the effects' settings";
        case bridge::Msg::ReadTarget: return "a read of the picture";
        default: return what ? "another message" : "the look at DLSS's start";
    }
}

void tell(uint64_t forMs, uint32_t what) {
    CONTEXT c{};
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    size_t count = 0;
    bool taken = false;
    // The thread held still for the copy alone: nothing here may wait for a lock it could hold (no memory asked,
    // no log line) - its registers and its stack's top, then it runs on
    if (SuspendThread(g_main) != DWORD(-1)) {
        if (GetThreadContext(g_main, &c)) {
            if (c.Rsp >= g_stackLow && c.Rsp < g_stackTop) {  // from its top to its start: all of it in use
                count = std::min<size_t>(size_t((g_stackTop - c.Rsp) / 8), kWords);
                std::memcpy(g_words, reinterpret_cast<const void*>(uintptr_t(c.Rsp)), count * 8);
                taken = true;
            }
        }
        ResumeThread(g_main);
    }
    if (!taken) {
        LOG_ERROR("hang: the renderer's thread has been in one message (%s, type %u) for %.1f s - its stack could "
                  "not be read", nameOf(what), unsigned(what), double(forMs) / 1000.0);
        return;
    }
    std::vector<stackscan::Module> modules = g_modules;
    size_t added = 0;
    if (!known(c.Rip, modules)) addImageOf(c.Rip, modules), ++added;
    for (size_t i = 0; i < count && added < kMostNewImages; ++i) {
        const uint64_t w = g_words[i];
        if (w < 0x10000 || w >= 0x7FFFFFFF0000ull || known(w, modules)) continue;
        const size_t before = modules.size();
        addImageOf(w, modules);
        if (modules.size() != before) ++added;
    }
    const std::vector<stackscan::Frame> frames =
        stackscan::scan(g_words, count, modules, readSelf, nullptr, kMostCalls);
    LOG_ERROR("hang: the renderer's thread has been in one message (%s, type %u) for %.1f s - it stands at %s; the "
              "calls on its stack, the newest first: %s",
              nameOf(what), unsigned(what), double(forMs) / 1000.0, stackscan::place(c.Rip, modules).c_str(),
              stackscan::calls(frames, modules).c_str());
}

DWORD WINAPI watch(void*) {
    readModules();
    const uint64_t started = GetTickCount64();
    uint64_t modulesAt = started;
    size_t reads = 0;
    uint32_t serial = 0;
    size_t told = 0;
    while (WaitForSingleObject(g_stop, kEveryMs) == WAIT_TIMEOUT) {
        const uint64_t since = g_since.load(), now = GetTickCount64();
        const uint32_t current = g_serial.load();
        if (!since || now - since < 500) {  // it moves: the images' list kept fresh
            const size_t early = sizeof kModulesAtMs / sizeof kModulesAtMs[0];
            if (reads < early ? now - started >= kModulesAtMs[reads] : now - modulesAt >= kModulesEveryMs) {
                readModules();
                modulesAt = now;
                ++reads;
            }
            continue;
        }
        if (current != serial) serial = current, told = 0;
        if (told < kTells && now - since >= kTellAtMs[told]) {
            g_toldSerial.store(serial);
            tell(now - since, g_what.load());
            ++told;
        }
    }
    return 0;
}

}  // namespace

void start() {
    if (g_thread) return;
    // the stack's room by Windows' own word (Windows 8 on; the 64-bit renderer draws by Direct3D 12)
    using LimitsFn = void(WINAPI*)(PULONG_PTR, PULONG_PTR);
    const auto limits = reinterpret_cast<LimitsFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetCurrentThreadStackLimits")));
    if (!limits) return;
    ULONG_PTR low = 0, high = 0;
    limits(&low, &high);
    if (!low || high <= low) return;
    g_stackLow = low, g_stackTop = high;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0)) {
        g_main = nullptr;
        return;
    }
    g_stop = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    g_thread = g_stop ? CreateThread(nullptr, 0, watch, nullptr, 0, nullptr) : nullptr;
    if (g_thread)
        LOG_INFO("hang watch: the renderer's thread watched - a message it stays in for over 2 s is told with where "
                 "the thread stands");
}

void busy(uint32_t what) {
    g_what.store(what);
    g_serial.fetch_add(1);
    g_since.store(GetTickCount64());
}

void idle() {
    const uint64_t since = g_since.exchange(0);
    if (since && g_toldSerial.load() == g_serial.load())
        LOG_INFO("hang: the renderer's thread moved on after %.1f s (%s)",
                 double(GetTickCount64() - since) / 1000.0, nameOf(g_what.load()));
}

void stop() {
    if (!g_thread) return;
    SetEvent(g_stop);
    WaitForSingleObject(g_thread, 1000);  // one that waits for the log's lock is left
    CloseHandle(g_thread);
    g_thread = nullptr;
}

}  // namespace uagfx::host::hangwatch
