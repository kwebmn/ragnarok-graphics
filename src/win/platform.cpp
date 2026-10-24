#include "win/platform.hpp"

#include <windows.h>

namespace uagfx::win {
namespace {

bool envSet(const char* name, const char* value = nullptr) {
    char buf[260] = {};
    const DWORD n = GetEnvironmentVariableA(name, buf, sizeof buf);
    if (n == 0 || n >= sizeof buf) return n >= sizeof buf && !value;
    return !value || lstrcmpiA(buf, value) == 0;
}

}  // namespace

const platform::Facts& platformFacts() {
    static const platform::Facts f = [] {
        platform::Facts r;
        using VersionFn = const char*(__cdecl*)();
        const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        const auto version =
            ntdll ? reinterpret_cast<VersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "wine_get_version")))
                  : nullptr;
        if (version) {
            r.wine = true;
            const char* v = version();
            r.wineVersion = v ? v : "";
        }
        r.proton = r.wine && (envSet("STEAM_COMPAT_DATA_PATH") || envSet("STEAM_COMPAT_CLIENT_INSTALL_PATH"));
        r.steamDeck = r.wine && (envSet("SteamDeck", "1") || envSet("SteamOS", "1"));
        return r;
    }();
    return f;
}

}  // namespace uagfx::win
