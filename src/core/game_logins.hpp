#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace uagfx::gamelogins {

// 3.0.11-beta.178 (the login concept: several game windows): the logins this Windows session's game windows play now
// - a shared block Local\uaro-graphics-logins-1 of kMost entries (win/game_logins). A game puts its entry when its
// login is accepted and takes it away back at the login screen; an entry of a game no longer running counts for
// nothing. The shelf shows those cards «вже в грі (вікно N)»: a login there again would throw that window out.
constexpr int kMost = 8;
constexpr const wchar_t* kBlock = L"Local\\uaro-graphics-logins-1";
struct Entry {
    uint32_t seq;    // the game window's number (its logs' slot: 1, 2, ...)
    uint32_t pid;    // its process; 0 - a free entry
    uint64_t login;  // accounts::loginHash of its login
};
static_assert(sizeof(Entry) == 16, "the shared block's entries");
struct Table {
    Entry e[kMost];
};
struct Busy {
    uint64_t login;
    int window;
};

// The other running games' logins (this game's entry and the dead ones left out)
inline std::vector<Busy> busy(const Table& t, uint32_t selfPid, const std::vector<uint32_t>& alivePids) {
    std::vector<Busy> out;
    for (const Entry& e : t.e)
        if (e.pid && e.pid != selfPid && std::find(alivePids.begin(), alivePids.end(), e.pid) != alivePids.end())
            out.push_back(Busy{e.login, int(e.seq)});
    return out;
}

// The shelf's first login no other game plays, -1 none
inline int firstFree(const std::vector<uint64_t>& shelf, const std::vector<uint64_t>& busyLogins) {
    for (size_t i = 0; i < shelf.size(); ++i)
        if (std::find(busyLogins.begin(), busyLogins.end(), shelf[i]) == busyLogins.end()) return int(i);
    return -1;
}

// This game's entry put (its own renewed, else the first free one): its place, -1 the block full
inline int put(Table& t, uint32_t pid, int window, uint64_t login) {
    int at = -1;
    for (int i = 0; i < kMost && at < 0; ++i)
        if (t.e[i].pid == pid) at = i;
    for (int i = 0; i < kMost && at < 0; ++i)
        if (t.e[i].pid == 0) at = i;
    if (at < 0) return -1;
    t.e[at] = Entry{uint32_t(window), pid, login};
    return at;
}

inline void take(Table& t, uint32_t pid) {
    for (Entry& e : t.e)
        if (e.pid == pid) e = Entry{0, 0, 0};
}

}  // namespace uagfx::gamelogins
