#include "win/game_screen_hook.hpp"

#include <windows.h>

#include <cstring>
#include <optional>
#include <string>

#include "core/layout.hpp"
#include "core/settings/game_screen.hpp"
#include "win/iat.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win {
namespace {

using settings::GameScreen;
using settings::RegValue;

decltype(&RegQueryValueExA) g_query = nullptr;
decltype(&RegSetValueExA) g_set = nullptr;
CRITICAL_SECTION g_lock;
bool g_read = false;
std::optional<GameScreen> g_screen;
int g_answered = 0, g_taken = 0;

std::string file() { return layout::join(gameDir(), layout::kGameScreen); }

// Under g_lock: the file read once (outside DllMain - the game's first query)
const std::optional<GameScreen>& screen() {
    if (!g_read) {
        g_read = true;
        std::string text;
        if (readTextFile(file(), text)) g_screen = settings::parseGameScreen(text);
    }
    return g_screen;
}

LONG WINAPI queryHook(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size) {
    RegValue v;
    bool ours = false;
    if (name) {
        EnterCriticalSection(&g_lock);
        ours = screen() && settings::screenValue(*g_screen, name, v);
        if (ours) ++g_answered;
        LeaveCriticalSection(&g_lock);
    }
    if (!ours) return g_query(key, name, reserved, type, data, size);
    const std::string bytes = v.isSz() ? v.text + std::string(1, '\0') : std::string(v.bytes.begin(), v.bytes.end());
    const DWORD need = DWORD(bytes.size());
    if (type) *type = v.type;
    if (!data) {
        if (size) *size = need;
        return ERROR_SUCCESS;
    }
    if (!size) return ERROR_INVALID_PARAMETER;
    if (*size < need) {
        *size = need;
        return ERROR_MORE_DATA;
    }
    std::memcpy(data, bytes.data(), need);
    *size = need;
    return ERROR_SUCCESS;
}

LONG WINAPI setHook(HKEY key, LPCSTR name, DWORD reserved, DWORD type, const BYTE* data, DWORD size) {
    if (name && data) {
        RegValue v;
        v.type = type;
        const char* text = reinterpret_cast<const char*>(data);
        if (type == REG_SZ)
            v = RegValue::sz(std::string(text, strnlen(text, size)));
        else
            v.bytes.assign(data, data + size);
        EnterCriticalSection(&g_lock);
        bool taken = false;
        if (screen()) {
            GameScreen s = *g_screen;
            if (settings::takeScreenWrite(s, name, v)) {
                taken = true;
                ++g_taken;
                if (s.width != g_screen->width || s.height != g_screen->height ||
                    s.fullscreen != g_screen->fullscreen) {
                    g_screen = s;
                    writeFileAtomic(file(), settings::gameScreenText(s));
                }
            }
        }
        LeaveCriticalSection(&g_lock);
        if (taken) return ERROR_SUCCESS;
    }
    return g_set(key, name, reserved, type, data, size);
}

}  // namespace

void installGameScreen() {
    InitializeCriticalSection(&g_lock);
    hookImport("advapi32.dll", "RegQueryValueExA", &queryHook, g_query);
    hookImport("advapi32.dll", "RegSetValueExA", &setHook, g_set);
}

void tellGameScreen() {
    EnterCriticalSection(&g_lock);
    const std::optional<GameScreen> s = g_read ? g_screen : std::nullopt;
    const bool read = g_read;
    const int answered = g_answered;
    LeaveCriticalSection(&g_lock);
    if (!g_query)
        LOG_INFO("game screen: the registry's (uaRO.exe's RegQueryValueExA not found)");
    else if (!read || !s)
        LOG_INFO("game screen: the registry's (no %s)", layout::kGameScreen);
    else
        LOG_INFO("game screen: uaRO HD's own - %ux%u, %s (%s; %d of the game's reads answered), the shared registry "
                 "key left to the old client", s->width, s->height, s->fullscreen ? "full screen" : "a window",
                 layout::kGameScreen, answered);
}

}  // namespace uagfx::win
