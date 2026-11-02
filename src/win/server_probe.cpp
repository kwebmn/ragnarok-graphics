#include "win/server_probe.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>

#include "win/log.hpp"

namespace uagfx::win::serverprobe {
namespace {

using GetAddrInfoFn = int(WSAAPI*)(const char*, const char*, const addrinfo*, addrinfo**);
using FreeAddrInfoFn = void(WSAAPI*)(addrinfo*);
using SocketFn = SOCKET(WSAAPI*)(int, int, int);
using ConnectFn = int(WSAAPI*)(SOCKET, const sockaddr*, int);
using SelectFn = int(WSAAPI*)(int, fd_set*, fd_set*, fd_set*, const timeval*);
using CloseFn = int(WSAAPI*)(SOCKET);
using IoctlFn = int(WSAAPI*)(SOCKET, long, u_long*);

struct Calls {
    GetAddrInfoFn getaddrinfo = nullptr;
    FreeAddrInfoFn freeaddrinfo = nullptr;
    SocketFn socket = nullptr;
    ConnectFn connect = nullptr;
    SelectFn select = nullptr;
    CloseFn closesocket = nullptr;
    IoctlFn ioctlsocket = nullptr;
    bool ok() const {
        return getaddrinfo && freeaddrinfo && socket && connect && select && closesocket && ioctlsocket;
    }
};

Calls calls() {
    Calls c;
    const HMODULE ws = GetModuleHandleW(L"ws2_32.dll");
    if (!ws) return c;
    const auto get = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(ws, name)); };
    c.getaddrinfo = reinterpret_cast<GetAddrInfoFn>(get("getaddrinfo"));
    c.freeaddrinfo = reinterpret_cast<FreeAddrInfoFn>(get("freeaddrinfo"));
    c.socket = reinterpret_cast<SocketFn>(get("socket"));
    c.connect = reinterpret_cast<ConnectFn>(get("connect"));
    c.select = reinterpret_cast<SelectFn>(get("select"));
    c.closesocket = reinterpret_cast<CloseFn>(get("closesocket"));
    c.ioctlsocket = reinterpret_cast<IoctlFn>(get("ioctlsocket"));
    return c;
}

SRWLOCK g_lock = SRWLOCK_INIT;  // the host and port (MinGW here has no std::mutex)
std::string g_host;
int g_port = 0;
std::atomic<ULONGLONG> g_wantedAt{0};
std::atomic<int> g_state{int(srvstatus::State::Unknown)};
std::atomic<int> g_ping{-1};
bool g_started = false;

// One try: connected within 3 s - its milliseconds; -1 not
int probe(const Calls& c, const std::string& host, int port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    if (c.getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0 || !found) return -1;
    const SOCKET s = c.socket(found->ai_family, SOCK_STREAM, IPPROTO_TCP);
    int ms = -1;
    if (s != INVALID_SOCKET) {
        u_long nonBlocking = 1;
        c.ioctlsocket(s, FIONBIO, &nonBlocking);
        const ULONGLONG start = GetTickCount64();
        c.connect(s, found->ai_addr, int(found->ai_addrlen));
        // the sets filled by hand: FD_ISSET is a call of ws2_32 (an import the driver must not have)
        fd_set writable{}, failed{};
        writable.fd_count = failed.fd_count = 1;
        writable.fd_array[0] = failed.fd_array[0] = s;
        const timeval wait{3, 0};
        if (c.select(0, nullptr, &writable, &failed, &wait) > 0 && writable.fd_count == 1 && failed.fd_count == 0)
            ms = int(GetTickCount64() - start);
        c.closesocket(s);
    }
    c.freeaddrinfo(found);
    return ms;
}

DWORD WINAPI run(void*) {
    const Calls c = calls();
    if (!c.ok()) {
        LOG_INFO("login: the server's status not measured (ws2_32 not there)");
        return 0;
    }
    for (;;) {
        if (GetTickCount64() - g_wantedAt.load() > 3000) {  // not on the login screens: asleep
            Sleep(500);
            continue;
        }
        std::string host;
        int port = 0;
        AcquireSRWLockShared(&g_lock);
        host = g_host;
        port = g_port;
        ReleaseSRWLockShared(&g_lock);
        const int ms = host.empty() || port <= 0 ? -1 : probe(c, host, port);
        g_ping = ms;
        g_state = int(ms >= 0 ? srvstatus::State::Up : srvstatus::State::Down);
        Sleep(DWORD(srvstatus::kProbeSec * 1000));
    }
}

}  // namespace

void want(const std::string& host, int port) {
    AcquireSRWLockExclusive(&g_lock);
    g_host = host;
    g_port = port;
    ReleaseSRWLockExclusive(&g_lock);
    g_wantedAt = GetTickCount64();
    if (!g_started) {
        g_started = true;
        const HANDLE t = CreateThread(nullptr, 0, &run, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
        LOG_INFO("login: the server's status measured every %d s while the login screens show", srvstatus::kProbeSec);
    }
}

Result result() {
    Result r;
    r.state = srvstatus::State(g_state.load());
    r.pingMs = g_ping.load();
    return r;
}

}  // namespace uagfx::win::serverprobe
