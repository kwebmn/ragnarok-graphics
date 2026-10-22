#include <windows.h>

#include "app/launcher.hpp"

namespace uagfx::app {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
LauncherState g_state;
settings::Lang g_lang = settings::Lang::En;

void set(const LauncherState& s) {
    AcquireSRWLockExclusive(&g_lock);
    g_state = s;
    ReleaseSRWLockExclusive(&g_lock);
}

DWORD WINAPI ask(void*) {
    set(launcherNow(g_lang));
    return 0;
}

}  // namespace

void startLauncherCheck(settings::Lang l) {
    if (launcherState().kind == LauncherState::Asking) return;
    LauncherState s;
    s.kind = LauncherState::Asking;
    set(s);
    g_lang = l;
    HANDLE t = CreateThread(nullptr, 0, ask, nullptr, 0, nullptr);
    if (t) {
        CloseHandle(t);
        return;
    }
    s.kind = LauncherState::Failed;
    s.error = "no thread for it";
    set(s);
}

LauncherState launcherState() {
    AcquireSRWLockShared(&g_lock);
    const LauncherState s = g_state;
    ReleaseSRWLockShared(&g_lock);
    return s;
}

}  // namespace uagfx::app
