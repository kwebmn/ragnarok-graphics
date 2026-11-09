#include "win/own_game_key.hpp"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/client_patch.hpp"
#include "core/layout.hpp"
#include "core/settings/game_key.hpp"
#include "win/iat.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win {
namespace {

using settings::kSharedLeaf;

// 3.0.17-beta.15: DllMain runs before the log opens (real_ddraw decide): the lines wait for tellOwnGameKey
std::vector<std::string>& notes() {
    static std::vector<std::string> n;
    return n;
}

template <typename... A>
void note(const char* fmt, A... a) {
    char text[512];
    std::snprintf(text, sizeof text, fmt, a...);
    notes().push_back(text);
}

void note(const char* text) { notes().push_back(text); }

// The driver imports no advapi32 (verify4's list): kernelbase's (Windows 8+, set up before any DLL of the game's), else
// the game's advapi32 (uaRO.exe imports RegOpenKeyExA)
struct Reg {
    decltype(&RegOpenKeyExA) open = nullptr;
    decltype(&RegCreateKeyExA) create = nullptr;
    decltype(&RegCloseKey) close = nullptr;
    decltype(&RegQueryValueExA) query = nullptr;
    decltype(&RegSetValueExA) set = nullptr;
    decltype(&RegDeleteValueA) erase = nullptr;
    decltype(&RegEnumValueA) value = nullptr;
    decltype(&RegEnumKeyExA) subkey = nullptr;
    decltype(&RegOpenKeyExW) openW = nullptr;
    decltype(&RegQueryValueExW) queryW = nullptr;
    bool ready() const {
        return open && create && close && query && set && erase && value && subkey && openW && queryW;
    }
};

template <typename F>
void take(HMODULE first, HMODULE then, const char* name, F& out) {
    FARPROC f = first ? GetProcAddress(first, name) : nullptr;
    if (!f && then) f = GetProcAddress(then, name);
    out = reinterpret_cast<F>(reinterpret_cast<void*>(f));
}

const Reg& reg() {
    static Reg r = [] {
        Reg out;
        const HMODULE k = GetModuleHandleA("kernelbase.dll"), m = GetModuleHandleA("advapi32.dll");
        take(k, m, "RegOpenKeyExA", out.open);
        take(k, m, "RegCreateKeyExA", out.create);
        take(k, m, "RegCloseKey", out.close);
        take(k, m, "RegQueryValueExA", out.query);
        take(k, m, "RegSetValueExA", out.set);
        take(k, m, "RegDeleteValueA", out.erase);
        take(k, m, "RegEnumValueA", out.value);
        take(k, m, "RegEnumKeyExA", out.subkey);
        take(k, m, "RegOpenKeyExW", out.openW);
        take(k, m, "RegQueryValueExW", out.queryW);
        return out;
    }();
    return r;
}

std::string ourClient() {
    std::string text, error;
    client_patch::Manifest m;
    if (!readTextFile(layout::join(gameDir(), client_patch::kClientFile), text) || !client_patch::parse(text, m, error))
        return client_patch::kOldClient;
    return client_patch::clientName(m.client);
}

bool readValue(HKEY key, const char* name, DWORD& type, std::vector<BYTE>& data) {
    DWORD size = 0;
    if (reg().query(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS) return false;
    data.resize(size);
    return reg().query(key, name, nullptr, &type, data.empty() ? nullptr : data.data(), &size) == ERROR_SUCCESS;
}

// Every value and subkey, as the game sees them (its own process: the personal copy over the common one)
int copyTree(HKEY from, HKEY to, int depth) {
    int values = 0;
    std::vector<char> name(16384);
    std::vector<BYTE> data;
    for (DWORD i = 0;; ++i) {
        DWORD nameLen = DWORD(name.size()), type = 0, size = 0;
        LONG r = reg().value(from, i, name.data(), &nameLen, nullptr, &type, nullptr, &size);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS && r != ERROR_MORE_DATA) break;
        data.resize(size);
        nameLen = DWORD(name.size());
        r = reg().value(from, i, name.data(), &nameLen, nullptr, &type, data.empty() ? nullptr : data.data(), &size);
        if (r != ERROR_SUCCESS) continue;
        if (reg().set(to, name.data(), 0, type, data.empty() ? nullptr : data.data(), size) == ERROR_SUCCESS)
            ++values;
    }
    if (depth >= 8) return values;
    for (DWORD i = 0;; ++i) {
        char sub[256];
        DWORD subLen = sizeof sub;
        if (reg().subkey(from, i, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        HKEY a = nullptr, b = nullptr;
        if (reg().open(from, sub, 0, KEY_READ, &a) != ERROR_SUCCESS) continue;
        if (reg().create(to, sub, 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &b, nullptr) == ERROR_SUCCESS) {
            values += copyTree(a, b, depth + 1);
            reg().close(b);
        }
        reg().close(a);
    }
    return values;
}

// The shared key's personal copy (the player's VirtualStore; HKCU is never virtualised): its screen values go; when
// what the game reads then is no setup.exe's screen (WIDTH, HEIGHT, DEVICENAME), they come back as they were
void repairShared() {
    BOOL wow = FALSE;
    IsWow64Process(GetCurrentProcess(), &wow);
    const std::string path = std::string("Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\") +
                             (wow ? "WOW6432Node\\" : "") + settings::gameKeyPath(kSharedLeaf).substr(9);
    HKEY personal = nullptr;
    if (reg().open(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                      &personal) != ERROR_SUCCESS)
        return;
    struct Saved {
        std::string name;
        DWORD type = 0;
        std::vector<BYTE> data;
    };
    std::vector<Saved> saved;
    for (const std::string& name : settings::screenValueNames()) {
        Saved s{name, 0, {}};
        if (!readValue(personal, name.c_str(), s.type, s.data)) continue;
        if (reg().erase(personal, name.c_str()) == ERROR_SUCCESS) saved.push_back(std::move(s));
    }
    if (saved.empty()) {
        reg().close(personal);
        return;
    }
    bool setup = false;
    HKEY seen = nullptr;  // the game's view: with the personal values gone, the common copy's
    if (reg().open(HKEY_LOCAL_MACHINE, settings::gameKeyPath(kSharedLeaf).c_str(), 0, KEY_QUERY_VALUE, &seen) ==
        ERROR_SUCCESS) {
        DWORD type = 0;
        std::vector<BYTE> data;
        setup = readValue(seen, "WIDTH", type, data) && readValue(seen, "HEIGHT", type, data) &&
                readValue(seen, "DEVICENAME", type, data);
        reg().close(seen);
    }
    if (!setup)
        for (const Saved& s : saved)
            reg().set(personal, s.name.c_str(), 0, s.type, s.data.empty() ? nullptr : s.data.data(),
                           DWORD(s.data.size()));
    reg().close(personal);
    note(setup ? "game key: the shared key's screen is its setup.exe's again (%zu values of uaRO HD's gone)"
               : "game key: the shared key keeps its screen (no setup.exe screen under it, %zu values)",
         saved.size());
}

// 3.0.17-beta.14: the old client has uaRO Graphics of its own - its Apps entry (app/registry writeAppsEntry, HKCU)
// in another folder, with its ddraw.dll there: it takes any screen, the one its app chose stays
bool oldClientHasOurs() {
    const std::wstring path = std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") +
                              std::wstring(settings::kOldAppsEntry, settings::kOldAppsEntry +
                                                                        std::strlen(settings::kOldAppsEntry));
    HKEY key = nullptr;
    if (reg().openW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return false;
    wchar_t dir[MAX_PATH + 1] = {};
    DWORD type = 0, size = sizeof dir - sizeof(wchar_t);
    const bool read = reg().queryW(key, L"InstallLocation", nullptr, &type, reinterpret_cast<BYTE*>(dir), &size) ==
                          ERROR_SUCCESS &&
                      type == REG_SZ;
    reg().close(key);
    if (!read || !dir[0]) return false;
    wchar_t ours[MAX_PATH + 1] = {};
    const std::string game = gameDir();
    MultiByteToWideChar(CP_ACP, 0, game.c_str(), -1, ours, MAX_PATH);
    if (lstrcmpiW(dir, ours) == 0) return false;
    const std::wstring dll = std::wstring(dir) + L"\\ddraw.dll";
    return GetFileAttributesW(dll.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// 3.0.17-beta.14 (the owner: «каждый раз ... смотрели не сломан ли старый клиент и чинили его»): at every start
void checkOldClient() {
    if (oldClientHasOurs()) {
        note("game key: the old client has uaRO Graphics of its own - the shared key's screen stays");
        return;
    }
    repairShared();
}


// The game's own key as the game opens it (HKLM, KEY_QUERY_VALUE: its process's view - the VirtualStore when
// Windows virtualises it), with a screen in it (WIDTH): else the game would run setup.exe and wait for it
bool ownReady(const std::string& own) {
    HKEY key = nullptr;
    if (reg().open(HKEY_LOCAL_MACHINE, own.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    DWORD type = 0;
    std::vector<BYTE> data;
    const bool screen = readValue(key, "WIDTH", type, data);
    reg().close(key);
    return screen;
}

void remember(const std::string& leaf) {
    writeFileAtomic(layout::join(gameDir(), layout::kGameKeyUsed), leaf + "\r\n");
}

}  // namespace

namespace {

// 3.0.17-beta.16: the work at the game's first registry call, not in DllMain (beta.13-.15 stopped the game before
// its log on the owner's PC: registry work under the loader's lock) - the string is changed before that call goes on
void setUp() {
    const std::string client = ourClient();
    const std::string leaf = settings::gameKeyLeaf(client);
    if (leaf == kSharedLeaf) return;
    if (!reg().ready()) {
        note("game key: the shared one (the registry's functions not found)");
        remember(kSharedLeaf);
        return;
    }
    const std::string shared = settings::gameKeyPath(kSharedLeaf);
    const std::string own = settings::gameKeyPath(leaf);
    auto* at = reinterpret_cast<char*>(uintptr_t(settings::kGameKeyAt));
    char now[settings::kGameKeyRoom + 1] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), at, now, sizeof now, &got) || got != sizeof now ||
        std::memcmp(now, shared.c_str(), shared.size() + 1) != 0) {
        note("game key: the shared one (this build of uaRO.exe has no \"%s\" where it is looked for)", shared.c_str());
        remember(kSharedLeaf);
        return;
    }
    // 3.0.17-beta.15: the game is pointed at its own key only when it can read it there - never worse than before
    bool copied = false;
    if (!ownReady(own)) {
        HKEY from = nullptr, key = nullptr;
        if (reg().open(HKEY_LOCAL_MACHINE, shared.c_str(), 0, KEY_READ, &from) == ERROR_SUCCESS) {
            const LONG r = reg().create(HKEY_LOCAL_MACHINE, own.c_str(), 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr,
                                        &key, nullptr);
            if (r == ERROR_SUCCESS) {
                note("game key: %d values of the shared key copied into %s", copyTree(from, key, 0), own.c_str());
                reg().close(key);
                copied = true;
            } else {
                note("game key: %s not made (error %ld)", own.c_str(), long(r));
            }
            reg().close(from);
        }
    }
    if (!ownReady(own)) {
        note("game key: %s stays on the shared key - its own %s cannot be read the game's way (no screen there)",
             client.c_str(), own.c_str());
        remember(kSharedLeaf);
        return;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, sizeof now, PAGE_READWRITE, &old)) {
        note("game key: the shared one (the game's string cannot be changed, error %lu)", GetLastError());
        remember(kSharedLeaf);
        return;
    }
    std::memset(at, 0, sizeof now);
    std::memcpy(at, own.c_str(), own.size());
    DWORD unused = 0;
    VirtualProtect(at, sizeof now, old, &unused);
    remember(leaf);
    note("game key: %s's own, %s%s", client.c_str(), own.c_str(), copied ? " (new)" : "");
    checkOldClient();
}

// The game's own registry imports: the first call of any of them sets the key up, then each goes on as it was
decltype(&RegOpenKeyExA) g_open = nullptr;
decltype(&RegCreateKeyExA) g_create = nullptr;
decltype(&RegDeleteKeyA) g_delete = nullptr;
LONG g_done = 0;

void once() {
    if (InterlockedCompareExchange(&g_done, 1, 0) == 0) setUp();
}

LONG WINAPI openHook(HKEY root, LPCSTR sub, DWORD options, REGSAM sam, PHKEY out) {
    once();
    return g_open(root, sub, options, sam, out);
}

LONG WINAPI createHook(HKEY root, LPCSTR sub, DWORD reserved, LPSTR cls, DWORD options, REGSAM sam,
                       const LPSECURITY_ATTRIBUTES security, PHKEY out, LPDWORD disposition) {
    once();
    return g_create(root, sub, reserved, cls, options, sam, security, out, disposition);
}

LONG WINAPI deleteHook(HKEY root, LPCSTR sub) {
    once();
    return g_delete(root, sub);
}

}  // namespace

void installOwnGameKey() {
    // DllMain: only the game's three import slots - nothing read, nothing of the registry touched here
    bool any = false;
    any = hookImport("advapi32.dll", "RegOpenKeyExA", &openHook, g_open) || any;
    any = hookImport("advapi32.dll", "RegCreateKeyExA", &createHook, g_create) || any;
    any = hookImport("advapi32.dll", "RegDeleteKeyA", &deleteHook, g_delete) || any;
    if (!any) note("game key: the shared one (uaRO.exe's registry imports not found)");
}

void tellOwnGameKey() {
    for (const std::string& line : notes()) LOG_INFO("%s", line.c_str());
    notes().clear();
}

}  // namespace uagfx::win
