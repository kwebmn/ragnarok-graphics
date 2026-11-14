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
// 3.0.18-beta.251 (R-NGSWJM, core/updates.hpp HookPlan): the game's archive reads one at a time - its map loader
// thread and the game's thread (a castle's 3D models) each moved the archive's one window under the other's inflate
CRITICAL_SECTION g_turn;
bool g_inTurn = false;     // g_turn made (the game's GetData known)
bool g_updates = false;    // the folder's GRFs mounted over the game's
volatile LONG g_waits = 0;  // the reads that waited for another thread's
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

// 3.0.18-beta.251: the game's own GetData (`fn`: its trampoline when hooked) with no other thread reading its
// archives meanwhile - its whole read (the window mapped, the entry deciphered and inflated into its copy) one at a
// time. Nothing else is taken inside (no resource lock, no driver's wait): its map loader thread may hold the game's
// resource lock while it waits here, never the other way round. A wait told in the log (core/updates tellWait).
void* readInTurn(GetDataFn fn, void* self, const char* name, uint32_t* size) {
    if (!g_inTurn) return fn(self, name, size);
    if (!TryEnterCriticalSection(&g_turn)) {
        LARGE_INTEGER a{}, b{}, hz{};
        QueryPerformanceCounter(&a);
        EnterCriticalSection(&g_turn);
        QueryPerformanceCounter(&b);
        QueryPerformanceFrequency(&hz);
        const unsigned n = unsigned(InterlockedIncrement(&g_waits));
        const double ms = hz.QuadPart > 0 ? double(b.QuadPart - a.QuadPart) * 1000.0 / double(hz.QuadPart) : 0.0;
        if (updates::tellWait(n))
            LOG_INFO("files: %s waited %.2f ms for another thread's read of the game's archives (thread %lu; %u "
                     "reads waited so far) - read in turn, as a castle's 3D models beside the map's loader",
                     name ? name : "?", ms, GetCurrentThreadId(), n);
    }
    void* const data = fn(self, name, size);
    LeaveCriticalSection(&g_turn);
    return data;
}

// 3.0.19-beta.1 (the testers: the old client - «CPaletteRes :: Cannot find File : palette\몸\프리스트_남_4.pal», then
// uaRO_New.exe at 0x417B94 on every account's character select: the game draws the sprite through the palette it did
// not find): a dye's palette none of the game's archives has - the sprite's own palette in its place (the sprite's own
// colours, as the game draws such a dye in the world), a copy as the game's own makes it; null: no such sprite either
void* paletteStandIn(void* self, const char* name, uint32_t* size) {
    const std::string sprite = updates::paletteSprite(name);
    if (sprite.empty()) return nullptr;
    uint32_t got = 0;
    void* const spr = readInTurn(reinterpret_cast<GetDataFn>(g_getData), self, sprite.c_str(), &got);
    if (!spr) return nullptr;
    std::vector<uint8_t> palette;
    const bool has =
        updates::spritePalette(std::vector<uint8_t>(static_cast<uint8_t*>(spr), static_cast<uint8_t*>(spr) + got),
                               palette);
    VirtualFree(spr, 0, MEM_RELEASE);  // as the game's own FUN_0050d080
    void* const copy = has ? VirtualAlloc(nullptr, palette.size() + 8, MEM_COMMIT, PAGE_READWRITE) : nullptr;
    if (!copy) return nullptr;
    std::memcpy(copy, palette.data(), palette.size());
    if (size) *size = uint32_t(palette.size());
    static volatile LONG told = 0;
    if (InterlockedIncrement(&told) <= 20)
        LOG_INFO("files: %s is in none of the client's archives - its sprite's own palette (%s) in its place (the "
                 "game would crash drawing it: its Palettes.grf misses that dye)",
                 name, sprite.c_str());
    return copy;
}

// The game's file from an update: a copy as the game's own makes it (VirtualAlloc, 8 bytes more; it frees it); else
// the game's own, its reads one at a time (3.0.18-beta.251)
void* __attribute__((thiscall)) getData(void* self, const char* name, uint32_t* size) {
    void* copy = nullptr;
    if (g_updates) {
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
    }
    if (copy) return copy;
    void* const data = readInTurn(reinterpret_cast<GetDataFn>(g_getData), self, name, size);
    return data || !name ? data : paletteStandIn(self, name, size);  // 3.0.19-beta.1
}

bool inUpdates(const char* name) {
    if (!g_updates) return false;
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
    InitializeCriticalSection(&g_lock);
    InitializeCriticalSection(&g_turn);
    std::string text;
    readTextFile(ourFile(layout::File::Ini), text);
    const bool on = parseConfig(text).clientUpdates;
    if (!on) g_lines.push_back("updates: off ([client] updates = 0) - the game's own archives alone");
    g_dir = gameDir();
    if (on) {
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
    }
    // 3.0.18-beta.251 (R-NGSWJM): GetData hooked whatever the updates - its reads one at a time
    const bool lookupsKnown = known(updates_exe::kHasFile, updates_exe::kHasFileBytes) &&
                              known(updates_exe::kFileExists, updates_exe::kFileExistsBytes);
    const updates::HookPlan plan = updates::hookPlan(known(updates_exe::kGetData, updates_exe::kGetDataBytes), on,
                                                     g_files.size(), lookupsKnown);
    if (!plan.inTurn) {
        g_lines.push_back("files: this build of uaRO.exe keeps its archives elsewhere - its reads not taken in turn"
                          ", no updates mounted");
        return;
    }
    if (!hookEntryBytes(updates_exe::kGetData, updates_exe::kGetDataBytes, updates_exe::kPrologue,
                        reinterpret_cast<const void*>(&getData), g_getData)) {
        g_lines.push_back("files: the game's GetData not hooked - its archives read as it reads them, no updates");
        return;
    }
    g_inTurn = true;
    g_lines.push_back("files: the game's archive reads one at a time (its CFileMgr::GetData hooked) - a castle's 3D "
                      "models on the game's thread and the map's loader thread no longer move each other's window "
                      "of the archive");
    if (!g_files.empty() && !plan.updates) {
        g_lines.push_back("updates: not mounted - this build of uaRO.exe keeps its archives elsewhere");
        return;
    }
    if (!plan.updates) return;  // nothing to put over the game's
    const bool ok = hookEntryBytes(updates_exe::kHasFile, updates_exe::kHasFileBytes, updates_exe::kPrologue,
                                   reinterpret_cast<const void*>(&hasFile), g_hasFile) &&
                    hookEntryBytes(updates_exe::kFileExists, updates_exe::kFileExistsBytes, updates_exe::kPrologue,
                                   reinterpret_cast<const void*>(&fileIsThere), g_exists);
    g_updates = ok;
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
    if (g_getData) data = getData(self, name.c_str(), &size);  // hooked: the updates, then the game's own in turn
    else data = readInTurn(reinterpret_cast<GetDataFn>(uintptr_t(updates_exe::kGetData)), self, name.c_str(), &size);
    if (lock) LeaveCriticalSection(lock);
    if (!data) return false;
    out.assign(static_cast<const char*>(data), size);
    VirtualFree(data, 0, MEM_RELEASE);  // as the game's own FUN_0050d080
    return true;
}

}  // namespace uagfx::win
