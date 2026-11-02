#include "win/game_logins.hpp"

#include <windows.h>

#include <cstring>

#include "core/accounts.hpp"
#include "win/log.hpp"

namespace uagfx::win::gamelogins {
namespace {

namespace gl = uagfx::gamelogins;

struct Shared {
    HANDLE mapping = nullptr, lock = nullptr;
    gl::Table* table = nullptr;
};

Shared& shared() {
    static Shared s = [] {
        Shared m;
        m.mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(gl::Table), gl::kBlock);
        if (m.mapping)
            m.table = static_cast<gl::Table*>(MapViewOfFile(m.mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(gl::Table)));
        m.lock = CreateMutexW(nullptr, FALSE, L"Local\\uaro-graphics-logins-1-lock");
        if (!m.table) LOG_INFO("login: the games' shared logins block is not there - «вже в грі» not told");
        return m;
    }();
    return s;
}

// The block for a moment (another game writing it: 200 ms at most, then without it)
struct Held {
    HANDLE lock;
    bool held;
    explicit Held(HANDLE l) : lock(l), held(l && WaitForSingleObject(l, 200) != WAIT_TIMEOUT) {}
    ~Held() {
        if (held) ReleaseMutex(lock);
    }
};

bool alive(uint32_t pid) {
    if (pid == GetCurrentProcessId()) return true;
    const HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) return false;
    const bool running = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return running;
}

}  // namespace

void accepted(const std::string& login) {
    Shared& s = shared();
    if (!s.table) return;
    const Held held(s.lock);
    for (const gl::Entry& e : s.table->e)  // the games gone: their entries free
        if (e.pid && !alive(e.pid)) gl::take(*s.table, e.pid);
    const int window = wlog::slot() > 0 ? wlog::slot() : 1;
    const int at = gl::put(*s.table, GetCurrentProcessId(), window, accounts::loginHash(login));
    if (at < 0) LOG_INFO("login: the games' shared logins block is full");
}

void left() {
    Shared& s = shared();
    if (!s.table) return;
    const Held held(s.lock);
    gl::take(*s.table, GetCurrentProcessId());
}

std::vector<gl::Busy> others() {
    static std::vector<gl::Busy> last;
    static ULONGLONG lookedAt = 0;
    const ULONGLONG now = GetTickCount64();
    if (lookedAt && now - lookedAt < 1000) return last;
    lookedAt = now;
    Shared& s = shared();
    last.clear();
    if (!s.table) return last;
    gl::Table copy;
    {
        const Held held(s.lock);
        std::memcpy(&copy, s.table, sizeof copy);
    }
    std::vector<uint32_t> running;
    for (const gl::Entry& e : copy.e)
        if (e.pid && alive(e.pid)) running.push_back(e.pid);
    last = gl::busy(copy, GetCurrentProcessId(), running);
    return last;
}

}  // namespace uagfx::win::gamelogins
