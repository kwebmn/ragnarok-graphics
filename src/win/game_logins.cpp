#include "win/game_logins.hpp"

#include <windows.h>

#include <cstring>

#include "core/accounts.hpp"
#include "win/log.hpp"

namespace uagfx::win::gamelogins {
namespace {

namespace gl = uagfx::gamelogins;

struct Shared {
    HANDLE mapping = nullptr, lock = nullptr, takenMapping = nullptr;
    gl::Table* table = nullptr;
    gl::TakenTable* taken = nullptr;  // 3.0.21-beta.5: the logins taken over (core/game_logins)
    uint64_t mine = 0;                // this game's login as the server accepted it last
};

Shared& shared() {
    static Shared s = [] {
        Shared m;
        m.mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(gl::Table), gl::kBlock);
        if (m.mapping)
            m.table = static_cast<gl::Table*>(MapViewOfFile(m.mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(gl::Table)));
        m.lock = CreateMutexW(nullptr, FALSE, L"Local\\uaro-graphics-logins-1-lock");
        m.takenMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(gl::TakenTable),
                                            gl::kTakenBlock);
        if (m.takenMapping)
            m.taken = static_cast<gl::TakenTable*>(
                MapViewOfFile(m.takenMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(gl::TakenTable)));
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
    const uint64_t hash = accounts::loginHash(login);
    // 3.0.21-beta.5 (R-KQRYFF): another running game plays this login - the server throws it out now; a line for it
    // to read that it was the player's own other window
    int other = 0;
    for (const gl::Entry& e : s.table->e)
        if (e.pid && e.pid != GetCurrentProcessId() && e.login == hash) other = int(e.seq);
    if (other && s.taken) {
        gl::putTaken(*s.taken, hash, uint32_t(window), GetCurrentProcessId(), GetTickCount64());
        LOG_INFO("login: this login was played in game window %d - the server throws it out there (told it that "
                 "this window took it)", other);
    }
    s.mine = hash;
    const int at = gl::put(*s.table, GetCurrentProcessId(), window, hash);
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

int takenOver(unsigned long long ms, unsigned long long withinMs) {
    Shared& s = shared();
    if (!s.taken || !s.mine) return 0;
    gl::TakenTable copy;
    {
        const Held held(s.lock);
        std::memcpy(&copy, s.taken, sizeof copy);
    }
    return gl::takenBy(copy, s.mine, GetCurrentProcessId(), ms, withinMs);
}

}  // namespace uagfx::win::gamelogins
