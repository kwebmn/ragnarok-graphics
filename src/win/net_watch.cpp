#include "win/net_watch.hpp"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/net_smooth.hpp"

namespace uagfx::win::netwatchwin {
namespace {

// uaRO.exe's: its connection object's socket (+4, FUN_00418af0 makes it), the character's own account
constexpr uint32_t kSocketAt = 4, kOwnAccount = 0x771F7C;
constexpr uint16_t kFixPos = 0x0088;  // ZC_STOPMOVE <unit>.L <x>.W <y>.W - the server puts a unit on a cell
constexpr ULONGLONG kReadMs = 1000;

// SIO_TCP_INFO and its TCP_INFO_v0 (mstcpip.h keeps them for Windows 10 1703 builds; ours is older), MSVC's layout
constexpr DWORD kTcpInfo = _WSAIORW(IOC_VENDOR, 39);
struct TcpInfo {
    int state = 0;
    ULONG mss = 0;
    alignas(8) ULONG64 connectionTimeMs = 0;
    BOOLEAN timestampsEnabled = 0;
    ULONG rttUs = 0, minRttUs = 0, bytesInFlight = 0, cwnd = 0, sndWnd = 0, rcvWnd = 0, rcvBuf = 0;
    alignas(8) ULONG64 bytesOut = 0;
    ULONG64 bytesIn = 0;
    ULONG bytesReordered = 0, bytesRetrans = 0, fastRetrans = 0, dupAcksIn = 0, timeoutEpisodes = 0;
    UCHAR synRetrans = 0;
};
static_assert(offsetof(TcpInfo, rttUs) == 20 && offsetof(TcpInfo, bytesOut) == 48, "TCP_INFO_v0's layout");
static_assert(offsetof(TcpInfo, timeoutEpisodes) == 80 && sizeof(TcpInfo) == 88, "TCP_INFO_v0's layout");

using SetSockOptFn = int(WSAAPI*)(SOCKET, int, int, const char*, int);
using IoctlFn = int(WSAAPI*)(SOCKET, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPWSAOVERLAPPED,
                             LPWSAOVERLAPPED_COMPLETION_ROUTINE);

struct State {
    netwatch::Watch watch;
    void* connection = nullptr;
    SOCKET nodelayOf = INVALID_SOCKET;  // the socket TCP_NODELAY was set on
    ULONGLONG readAt = 0;
    bool loaded = false, infoTold = false;
    SetSockOptFn setOpt = nullptr;
    IoctlFn ioctl = nullptr;
    int fixesTold = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

// ws2_32's own two, without importing it into ddraw.dll (the game loaded it already)
void load(State& s) {
    if (s.loaded) return;
    s.loaded = true;
    HMODULE ws = GetModuleHandleA("ws2_32.dll");
    if (!ws) ws = LoadLibraryA("ws2_32.dll");
    if (!ws) return;
    s.setOpt = reinterpret_cast<SetSockOptFn>(reinterpret_cast<void*>(GetProcAddress(ws, "setsockopt")));
    s.ioctl = reinterpret_cast<IoctlFn>(reinterpret_cast<void*>(GetProcAddress(ws, "WSAIoctl")));
}

SOCKET socketOf(void* connection) {
    SOCKET sock = INVALID_SOCKET;
    if (!connection || !readGame(uint32_t(uintptr_t(connection)) + kSocketAt, &sock, sizeof sock))
        return INVALID_SOCKET;
    return sock;
}

int rd16(const uint8_t* p, size_t at) { return int(uint16_t(p[at] | p[at + 1] << 8)); }

}  // namespace

void packet(void* connection, uint16_t id, const uint8_t* data) {
    State& s = state();
    s.connection = connection;
    if (!inGameWorld()) return;
    const ULONGLONG now = GetTickCount64();
    s.watch.packet(now);
    if (id != kFixPos || !data) return;
    uint32_t unit = 0;
    std::memcpy(&unit, data + 2, 4);
    const int x = rd16(data, 6), y = rd16(data, 8);
    const bool own = unit != 0 && unit == read32(kOwnAccount);
    // 3.0.11-beta.123: how far it stood (each unit's), the fix glided to when near (the packet's cell its own then)
    const int cells = netsmoothwin::fix(unit, const_cast<uint8_t*>(data));
    s.watch.fix(own, std::max(cells, 0), now);
    if (own && cells >= 2 && s.fixesTold++ < 20)
        LOG_INFO("net: the server put the character %d cells from where the game showed it (to %d, %d)", cells, x, y);
}

void frame() {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (!inGameWorld() || now - s.readAt < kReadMs) return;
    s.readAt = now;
    load(s);
    const SOCKET sock = socketOf(s.connection);
    if (sock == INVALID_SOCKET || sock == 0) return;
    if (sock != s.nodelayOf && s.setOpt) {  // 2: Nagle's algorithm off - a small command goes out at once
        s.nodelayOf = sock;
        const BOOL yes = TRUE;
        const int r = s.setOpt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof yes);
        LOG_INFO("net: TCP_NODELAY on the map server's socket %s - the game's commands go out at once, not waiting "
                 "for Nagle's algorithm", r == 0 ? "set" : "NOT set");
    }
    if (!s.ioctl) return;
    DWORD version = 0, got = 0;
    TcpInfo info;
    if (s.ioctl(sock, kTcpInfo, &version, sizeof version, &info, sizeof info, &got, nullptr, nullptr) != 0) {
        if (!s.infoTold) {
            s.infoTold = true;
            LOG_INFO("net: Windows gives no TCP_INFO here (older than Windows 10 1703?) - the round trip not known");
        }
        return;
    }
    netwatch::Tcp t;
    t.rttUs = info.rttUs;
    t.minRttUs = info.minRttUs;
    t.bytesRetrans = info.bytesRetrans;
    t.timeouts = info.timeoutEpisodes;
    s.watch.tcp(t, now);
    if (s.watch.stalled())
        LOG_INFO("net: a stall - TCP waited for a lost packet (time-outs %lu, round trip %d ms): the world jumps "
                 "after it", info.timeoutEpisodes, s.watch.rttMs());
    if (s.watch.due(now)) LOG_INFO("net: %s", s.watch.line(now).c_str());
}

netwatch::Level level() { return state().watch.level(GetTickCount64()); }

int rttMs() { return state().watch.rttMs(); }

}  // namespace uagfx::win::netwatchwin
