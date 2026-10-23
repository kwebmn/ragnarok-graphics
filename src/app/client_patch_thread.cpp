#include <windows.h>

#include "app/check_notify.hpp"
#include "app/client_patch.hpp"

namespace uagfx::app {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
PatchState g_state;
std::wstring g_dir;

void set(const PatchState& s) {
    AcquireSRWLockExclusive(&g_lock);
    g_state = s;
    ReleaseSRWLockExclusive(&g_lock);
}

DWORD WINAPI ask(void*) {
    PatchState s;
    s.kind = PatchState::Done;
    s.check = checkClientPatch(g_dir);
    set(s);
    checkDone();
    return 0;
}

}  // namespace

void startClientPatchCheck(const std::wstring& dir) {
    if (clientPatchState().kind == PatchState::Asking) return;
    PatchState s;
    s.kind = PatchState::Asking;
    set(s);
    g_dir = dir;  // read by the thread only after this
    HANDLE t = CreateThread(nullptr, 0, ask, nullptr, 0, nullptr);
    if (t) {
        CloseHandle(t);
        return;
    }
    s.kind = PatchState::Done;  // no thread: no patch this time, «Грати» as always
    set(s);
}

PatchState clientPatchState() {
    AcquireSRWLockShared(&g_lock);
    const PatchState s = g_state;
    ReleaseSRWLockShared(&g_lock);
    return s;
}

}  // namespace uagfx::app
