#include "win/updates_hook.hpp"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/grf/grf.hpp"
#include "core/layout.hpp"
#include "core/updates.hpp"
#include "core/updates_exe.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win {
namespace {

using GetDataFn = void*(__attribute__((thiscall))*)(void* self, const char* name, uint32_t* size);
using HasFileFn = bool(__attribute__((thiscall))*)(void* self, const char* name);
using ExistsFn = bool(__cdecl*)(const char* name);

uintptr_t g_getData = 0, g_hasFile = 0, g_exists = 0;  // the game's own, through their trampolines
CRITICAL_SECTION g_lock;
std::string g_dir;
std::vector<std::string> g_files;  // updates\'s GRFs, the later first
std::vector<std::unique_ptr<grf::Archive>> g_archives;
updates::Index g_index;
bool g_opened = false;
std::vector<std::string> g_lines;  // for the log, which opens at the driver's start

void say(const std::string& line) {
    g_lines.push_back(line);
    LOG_INFO("%s", line.c_str());  // the log open by now: there; else at the driver's start
}

// The archives, at the first lookup (the driver's load is in the loader's lock); under g_lock
void openOnce() {
    if (g_opened) return;
    g_opened = true;
    std::string mounted;
    for (const std::string& file : g_files) {
        auto a = std::make_unique<grf::Archive>();
        std::string why;
        if (!a->openToRead(layout::join(layout::join(g_dir, updates::kFolder), file), why)) {
            say("updates: " + file + " not mounted (" + why + ")");
            continue;
        }
        g_index.add(int(g_archives.size()), a->names());
        mounted += (mounted.empty() ? "" : ", ") + file + " (" + std::to_string(a->count()) + " files)";
        g_archives.push_back(std::move(a));
    }
    if (!mounted.empty()) say("updates: over the game's archives, the later first - " + mounted);
}

const updates::Hit* hitOf(const char* name) {  // under g_lock
    openOnce();
    return name ? g_index.find(name) : nullptr;
}

// The game's file from an update: a copy as the game's own makes it (VirtualAlloc, 8 bytes more; it frees it)
void* __attribute__((thiscall)) getData(void* self, const char* name, uint32_t* size) {
    void* copy = nullptr;
    EnterCriticalSection(&g_lock);
    try {
        if (const updates::Hit* hit = hitOf(name)) {
            std::vector<uint8_t> bytes;
            std::string why;
            if (g_archives[size_t(hit->archive)]->read(hit->name, bytes, why) &&
                (copy = VirtualAlloc(nullptr, bytes.size() + 8, MEM_COMMIT, PAGE_READWRITE)) != nullptr) {
                if (!bytes.empty()) std::memcpy(copy, bytes.data(), bytes.size());
                if (size) *size = uint32_t(bytes.size());
            }
        }
    } catch (const std::bad_alloc&) {
        copy = nullptr;
    }
    LeaveCriticalSection(&g_lock);
    return copy ? copy : reinterpret_cast<GetDataFn>(g_getData)(self, name, size);
}

bool inUpdates(const char* name) {
    EnterCriticalSection(&g_lock);
    bool found = false;
    try {
        found = hitOf(name) != nullptr;
    } catch (const std::bad_alloc&) {
    }
    LeaveCriticalSection(&g_lock);
    return found;
}

bool __attribute__((thiscall)) hasFile(void* self, const char* name) {
    return inUpdates(name) || reinterpret_cast<HasFileFn>(g_hasFile)(self, name);
}

bool __cdecl fileIsThere(const char* name) {
    return inUpdates(name) || reinterpret_cast<ExistsFn>(g_exists)(name);
}

// A function's entry, its first n bytes `expect` (whole instructions), to `hook`; those bytes run in a trampoline
// of their own before the rest of it
bool hookEntry(uint32_t entry, const uint8_t* expect, size_t n, const void* hook, uintptr_t& trampoline) {
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(entry));
    uint8_t now[16] = {};
    SIZE_T got = 0;
    if (n < 5 || n > 11 || !ReadProcessMemory(GetCurrentProcess(), at, now, n, &got) || got != n ||
        std::memcmp(now, expect, n) != 0)
        return false;
    auto* t = static_cast<uint8_t*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!t) return false;
    std::memcpy(t, now, n);
    t[n] = 0xE9;  // jmp entry + n
    const int32_t back = int32_t(uintptr_t(entry) + n - (reinterpret_cast<uintptr_t>(t) + n + 5));
    std::memcpy(t + n + 1, &back, sizeof back);
    FlushInstructionCache(GetCurrentProcess(), t, 32);
    DWORD old = 0;
    if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old)) {
        VirtualFree(t, 0, MEM_RELEASE);
        return false;
    }
    uint8_t jump[16];
    std::memset(jump, 0x90, sizeof jump);  // jmp hook, nops after it
    jump[0] = 0xE9;
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(hook) - (uintptr_t(entry) + 5));
    std::memcpy(jump + 1, &to, sizeof to);
    std::memcpy(at, jump, n);
    DWORD unused = 0;
    VirtualProtect(at, n, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, n);
    trampoline = reinterpret_cast<uintptr_t>(t);
    return true;
}

bool known(uint32_t entry, const uint8_t* expect) {
    uint8_t now[updates_exe::kPrologue] = {};
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(entry)), now, sizeof now,
                             &got) &&
           got == sizeof now && std::memcmp(now, expect, sizeof now) == 0;
}

}  // namespace

void installUpdates() {
    if (!GetModuleHandleA("dinput.dll")) return;  // the game alone (it imports DINPUT.dll), not setup.exe
    std::string text;
    readTextFile(ourFile(layout::File::Ini), text);
    if (!parseConfig(text).clientUpdates) {
        g_lines.push_back("updates: off ([client] updates = 0) - the game's own archives alone");
        return;
    }
    g_dir = gameDir();
    std::vector<std::string> names;
    WIN32_FIND_DATAA f{};
    const std::string pattern = layout::join(layout::join(g_dir, updates::kFolder), "*.grf");
    HANDLE h = FindFirstFileA(pattern.c_str(), &f);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) names.push_back(f.cFileName);
        } while (FindNextFileA(h, &f));
        FindClose(h);
    }
    g_files = updates::lookupOrder(names);
    if (g_files.empty()) return;  // nothing to put over the game's: nothing hooked
    if (!known(updates_exe::kGetData, updates_exe::kGetDataBytes) ||
        !known(updates_exe::kHasFile, updates_exe::kHasFileBytes) ||
        !known(updates_exe::kFileExists, updates_exe::kFileExistsBytes)) {
        g_lines.push_back("updates: not mounted - this build of uaRO.exe keeps its archives elsewhere");
        return;
    }
    InitializeCriticalSection(&g_lock);
    const bool ok = hookEntry(updates_exe::kGetData, updates_exe::kGetDataBytes, updates_exe::kPrologue,
                              reinterpret_cast<const void*>(&getData), g_getData) &&
                    hookEntry(updates_exe::kHasFile, updates_exe::kHasFileBytes, updates_exe::kPrologue,
                              reinterpret_cast<const void*>(&hasFile), g_hasFile) &&
                    hookEntry(updates_exe::kFileExists, updates_exe::kFileExistsBytes, updates_exe::kPrologue,
                              reinterpret_cast<const void*>(&fileIsThere), g_exists);
    char line[160];
    std::snprintf(line, sizeof line, "updates: %u archive(s) in updates\\ %s", unsigned(g_files.size()),
                  ok ? "- mounted at the first lookup" : "- the hooks failed, not mounted");
    g_lines.push_back(line);
}

void logUpdates() {
    for (const std::string& line : g_lines) LOG_INFO("%s", line.c_str());
}

bool readGameFile(const std::string& name, std::string& out) {
    constexpr uintptr_t kFileMgr = 0x74BA18;  // the game's CFileMgr (its pak list at +4, «loose first» at +0xC)
    static const bool knownBuild = known(updates_exe::kGetData, updates_exe::kGetDataBytes);
    void* const self = reinterpret_cast<void*>(kFileMgr);
    uint32_t size = 0;
    void* data = nullptr;
    if (g_getData) data = getData(self, name.c_str(), &size);  // hooked: the updates, then the game's own
    else if (knownBuild)
        data = reinterpret_cast<GetDataFn>(uintptr_t(updates_exe::kGetData))(self, name.c_str(), &size);
    else
        return false;
    if (!data) return false;
    out.assign(static_cast<const char*>(data), size);
    VirtualFree(data, 0, MEM_RELEASE);  // as the game's own FUN_0050d080
    return true;
}

}  // namespace uagfx::win
