#include "pad/win/mount.hpp"

#include <windows.h>

#include <dinput.h>

#include <string>

#include "pad/win/di_wrappers.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/iat.hpp"
#include "pad/win/log.hpp"
#include "pad/win/runtime.hpp"

namespace uapad {
namespace {

using DirectInputCreateA_t = HRESULT(WINAPI*)(HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);

DirectInputCreateA_t g_original = nullptr;
bool g_mounted = false;

HRESULT WINAPI proxyDirectInputCreateA(HINSTANCE inst, DWORD version, LPDIRECTINPUTA* out, LPUNKNOWN outer) {
    runtime::init();
    const HRESULT hr = g_original(inst, version, out, outer);
    LOG_INFO("DirectInputCreateA(version 0x%04lx) = 0x%08lx", (unsigned long)version, (unsigned long)hr);
    if (SUCCEEDED(hr) && out && *out && !outer && runtime::healthy()) *out = wrapDirectInput(*out);
    return hr;
}

// The game's dinput.dll is a 2.x uaRO Pad: not the system's, and its bytes name the pad's log.
bool oldPad(HMODULE dinput) {
    char path[MAX_PATH] = {0};
    const DWORD n = GetModuleFileNameA(dinput, path, MAX_PATH);
    char system[MAX_PATH] = {0};
    const UINT s = GetSystemDirectoryA(system, MAX_PATH);
    if (!n || (s && CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, path, int(s), system, int(s)) == CSTR_EQUAL))
        return false;
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    const DWORD size = GetFileSize(f, nullptr);
    std::string bytes(size == INVALID_FILE_SIZE || size > (16u << 20) ? 0 : size, '\0');
    DWORD got = 0;
    const bool read = !bytes.empty() && ReadFile(f, &bytes[0], DWORD(bytes.size()), &got, nullptr);
    CloseHandle(f);
    return read && bytes.find("uaro-pad.log") != std::string::npos;
}

}  // namespace

Mounted mount(bool enabled) {
    if (!enabled) return Mounted::Off;
    const HMODULE dinput = GetModuleHandleA("dinput.dll");
    if (!dinput) return Mounted::NoSlot;
    if (oldPad(dinput)) return Mounted::OldPad;
    const auto create =
        reinterpret_cast<DirectInputCreateA_t>(reinterpret_cast<void*>(GetProcAddress(dinput, "DirectInputCreateA")));
    if (!create) return Mounted::NoSlot;
    g_original = create;
    g_mounted = iat::patch(GetModuleHandleA(nullptr), reinterpret_cast<void*>(create),
                           reinterpret_cast<void*>(&proxyDirectInputCreateA)) != nullptr;
    return g_mounted ? Mounted::Yes : Mounted::NoSlot;
}

void unmount() {
    if (g_mounted) gamewindow::releaseCursor();
}

const char* mountedText(Mounted m) {
    switch (m) {
        case Mounted::Yes: return "uaRO Pad mounted ([pad] enabled = 1): DirectInput through the pad, its log "
                                  "uaro-graphics\\logs\\uaro-pad.log";
        case Mounted::Off: return "uaRO Pad off ([pad] enabled = 0)";
        case Mounted::OldPad: return "uaRO Pad: an old uaRO Pad dinput.dll is in the game's folder and runs instead "
                                     "- the settings app removes it while the game is closed";
        case Mounted::NoSlot: return "uaRO Pad not mounted: the game has no DINPUT.dll!DirectInputCreateA import";
    }
    return "?";
}

}  // namespace uapad
