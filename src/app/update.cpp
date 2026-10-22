#include "app/update.hpp"

#include <windows.h>

#include "app/files.hpp"
#include "app/text.hpp"
#include "app/update_run.hpp"

namespace uagfx::app {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
UpdateState g_state;

void set(const UpdateState& s) {
    AcquireSRWLockExclusive(&g_lock);
    g_state = s;
    ReleaseSRWLockExclusive(&g_lock);
}

DWORD WINAPI check(void*) {
    const CheckResult c = checkNow(exeFolder());
    UpdateState s;
    s.kind = c.ok ? UpdateState::Done : UpdateState::Failed;
    s.latest = c.branch.package.version;
    s.offer = c.offer;
    s.channel = c.channel;
    s.branch = c.branch;
    s.error = c.error;
    set(s);
    return 0;
}

}  // namespace

void startUpdateCheck() {
    if (updateState().kind == UpdateState::Checking) return;
    UpdateState s;
    s.kind = UpdateState::Checking;
    set(s);
    HANDLE t = CreateThread(nullptr, 0, check, nullptr, 0, nullptr);
    if (t) {
        CloseHandle(t);
        return;
    }
    s.kind = UpdateState::Failed;
    s.error = "no thread for it";
    set(s);
}

UpdateState updateState() {
    AcquireSRWLockShared(&g_lock);
    const UpdateState s = g_state;
    ReleaseSRWLockShared(&g_lock);
    return s;
}

}  // namespace uagfx::app
