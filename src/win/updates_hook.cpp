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
#include "win/entry_hook.hpp"
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
    const bool ok = hookEntryBytes(updates_exe::kGetData, updates_exe::kGetDataBytes, updates_exe::kPrologue,
                                   reinterpret_cast<const void*>(&getData), g_getData) &&
                    hookEntryBytes(updates_exe::kHasFile, updates_exe::kHasFileBytes, updates_exe::kPrologue,
                                   reinterpret_cast<const void*>(&hasFile), g_hasFile) &&
                    hookEntryBytes(updates_exe::kFileExists, updates_exe::kFileExistsBytes, updates_exe::kPrologue,
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
    if (!g_getData && !knownBuild) return false;
    // 3.0.18-beta.145 (the tester's crashes in cps.dll right after a map's start, R-MUC6HW, R-7FSHCZ: the game's
    // thread then stuck at 0x513220): the game reads its files under its resource manager's lock (g_resMgr 0x74BAD8,
    // its critical section at +0x40, FUN_005131f0) - the map's model loader thread FUN_004ea130 reads while the map
    // starts. Ours took the files beside that lock: two readers in the game's archives at once. Now under it too (once
    // the manager is made: 0x74BB84 bit 0).
    constexpr uintptr_t kResLock = 0x74BB18, kResMade = 0x74BB84;
    const bool made = knownBuild && (*reinterpret_cast<volatile uint32_t*>(kResMade) & 1);
    CRITICAL_SECTION* const lock = made ? reinterpret_cast<CRITICAL_SECTION*>(kResLock) : nullptr;
    // 3.0.18-beta.147 (R-STGQE9: .145's wait froze the game entering comodo - the loader holds the lock and waits for
    // the driver, the driver's frame waited for the lock): never waited - taken now or not at all; not: false, the
    // caller's own reader of the archives (readClientFile) takes the file instead
    if (lock && !TryEnterCriticalSection(lock)) {
        static int told = 0;
        if (told++ < 5)
            LOG_INFO("files: %s read by our own reader - the game's files busy (a map loading)", name.c_str());
        return false;
    }
    if (g_getData) data = getData(self, name.c_str(), &size);  // hooked: the updates, then the game's own
    else data = reinterpret_cast<GetDataFn>(uintptr_t(updates_exe::kGetData))(self, name.c_str(), &size);
    if (lock) LeaveCriticalSection(lock);
    if (!data) return false;
    out.assign(static_cast<const char*>(data), size);
    VirtualFree(data, 0, MEM_RELEASE);  // as the game's own FUN_0050d080
    return true;
}

}  // namespace uagfx::win
