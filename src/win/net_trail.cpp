#include "win/net_trail.hpp"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "core/log_files.hpp"
#include "core/version.hpp"
#include "win/game_camera.hpp"
#include "win/iat.hpp"
#include "win/log.hpp"
#include "win/net_watch.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::nettrailwin {
namespace {

// uaRO.exe's CConnection: its read (FUN_00418eb0) calls [0x6EE66C] at 0x418F0E, its write (FUN_00418db0)
// [0x6EE6E4] at 0x418E2B - recv and send as GetProcAddress gave them at the game's start (FUN_00418fa0)
constexpr uint32_t kRecvSlot = 0x6EE66C, kSendSlot = 0x6EE6E4, kRecvCallAt = 0x418F0E, kSendCallAt = 0x418E2B;
const uint8_t kRecvCall[] = {0xFF, 0x15, 0x6C, 0xE6, 0x6E, 0x00};
const uint8_t kSendCall[] = {0xFF, 0x15, 0xE4, 0xE6, 0x6E, 0x00};
// who calls closesocket: the connection's connect, its close, its write, its read
constexpr uint32_t kConnectFrom = 0x418AF0, kConnectTo = 0x418C60, kCloseFrom = 0x418C90, kCloseTo = 0x418CD0,
                   kWriteFrom = 0x418DB0, kWriteTo = 0x418EB0, kReadFrom = 0x418EB0, kReadTo = 0x418FA0;
constexpr unsigned kCloseSocketOrdinal = 3;  // ws2_32.dll's closesocket
constexpr uint64_t kReportEveryMs = 60000;   // a report a minute at most from here (the app keeps its own count)
// 3.0.20-beta.64: uaRO Pad (inside the driver, pad/win/hooks.cpp) takes the same functions - closesocket's import
// and the game's recv and send pointers, each found by ws2_32.dll's own function still being there. Ours go in over
// its, never before it: the pad mounts at the DLL's load and looks at the pointers every quarter second of the game's
// message pump - so closesocket's import at the first poll (beta.60-63 took it at the load, before the pad: the pad's
// own hook found no slot and never heard of a socket closed), and the pointers once the pad has them, or after this
// long with ws2_32's own still there (no pad)
constexpr DWORD kPadGraceMs = 3000;

// ws2_32.dll's own, not imported into ddraw.dll (the game has it): a socket is a pointer's width, an error is -1
// and its number the thread's last error (WSAGetLastError is GetLastError)
using Socket = UINT_PTR;
constexpr int kSocketError = -1;
using RecvFn = int(WINAPI*)(Socket, char*, int, int);
using SendFn = int(WINAPI*)(Socket, const char*, int, int);
using CloseFn = int(WINAPI*)(Socket);

struct State {
    CRITICAL_SECTION lock;
    bool lockMade = false, sameExe = false, swapped = false, told = false, closeTried = false;
    DWORD firstLook = 0;
    nettrail::Trail trail;
    RecvFn recv = nullptr;
    SendFn send = nullptr;
    CloseFn close = nullptr;
    bool on = true, foreground = true;
    std::string map;
    uint64_t startedMs = 0, lastReportMs = 0;
    unsigned reports = 0;
    int sites = 0, sitesAll = 0;
};

State& state() {
    static State s;
    return s;
}

struct Locked {
    explicit Locked(State& s) : s_(s) {
        if (s_.lockMade) EnterCriticalSection(&s_.lock);
    }
    ~Locked() {
        if (s_.lockMade) LeaveCriticalSection(&s_.lock);
    }
    State& s_;
};

int WINAPI recvHook(Socket s, char* buffer, int size, int flags) {
    State& st = state();
    const int got = st.recv(s, buffer, size, flags);
    const int error = got == kSocketError ? int(GetLastError()) : 0;
    {
        Locked l(st);
        st.trail.received(GetTickCount(), uint32_t(s), got, error);
    }
    if (got == kSocketError) SetLastError(DWORD(error));  // the game reads it next
    return got;
}

int WINAPI sendHook(Socket s, const char* buffer, int size, int flags) {
    State& st = state();
    const int put = st.send(s, buffer, size, flags);
    const int error = put == kSocketError ? int(GetLastError()) : 0;
    {
        Locked l(st);
        st.trail.sent(GetTickCount(), uint32_t(s), put, error);
    }
    if (put == kSocketError) SetLastError(DWORD(error));
    return put;
}

int WINAPI closeHook(Socket s) {
    State& st = state();
    const auto from = uint32_t(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    const nettrail::Closer by = from >= kCloseFrom && from < kCloseTo       ? nettrail::Closer::Game
                                : from >= kReadFrom && from < kReadTo       ? nettrail::Closer::AfterRecv
                                : from >= kWriteFrom && from < kWriteTo     ? nettrail::Closer::AfterSend
                                : from >= kConnectFrom && from < kConnectTo ? nettrail::Closer::AfterConnect
                                                                            : nettrail::Closer::Other;
    {
        Locked l(st);
        st.trail.closed(GetTickCount(), uint32_t(s), by);
    }
    return st.close(s);
}

// "a.b.c.d" as its bytes, a the lowest; 0 for what is no such address
uint32_t ipOf(const char* text) {
    uint32_t ip = 0;
    int part = 0, value = 0, digits = 0;
    for (const char* p = text; p - text < 16; ++p) {
        if (*p >= '0' && *p <= '9') {
            value = value * 10 + (*p - '0');
            if (++digits > 3 || value > 255) return 0;
            continue;
        }
        if (!digits || part > 3) return 0;
        ip |= uint32_t(value) << (8 * part++);
        value = digits = 0;
        if (*p != '.') return part == 4 && !*p ? ip : 0;
    }
    return 0;
}

nettrail::Clock clockNow() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    nettrail::Clock c;
    c.ms = GetTickCount();
    c.hour = t.wHour, c.minute = t.wMinute, c.second = t.wSecond, c.milli = t.wMilliseconds;
    return c;
}

std::string trailFile() {
    const std::string log = wlog::path();
    const size_t slash = log.find_last_of("\\/");
    if (log.empty() || slash == std::string::npos) return "";
    return logfiles::slotted(log.substr(0, slash + 1) + "uaro-disconnect.txt", wlog::slot());
}

// a drop: its file written (the summary first, then the trail) and the settings app asked to send it
void tell(State& st, const nettrail::Drop& d) {
    std::vector<std::string> facts;
    char line[200];
    int x = 0, y = 0, width = 0, height = 0;
    const bool placed = !st.map.empty() && readPlayerCell(x, y, width, height);
    if (st.map.empty())
        facts.push_back("map: none (the game's login screens)");
    else if (placed)
        std::snprintf(line, sizeof line, "map: %s at %d, %d", st.map.c_str(), x, y), facts.push_back(line);
    else
        facts.push_back("map: " + st.map);
    facts.push_back(std::string("the game's window: ") + (st.foreground ? "in front" : "behind another or minimized"));
    const int rtt = netwatchwin::rttMs();
    const uint64_t loss = netwatchwin::lastLossMs(), now = GetTickCount64();
    if (rtt > 0) {
        std::snprintf(line, sizeof line, "the map server's connection by Windows before it: round trip %d ms", rtt);
        facts.push_back(line);
    }
    if (loss) {
        std::snprintf(line, sizeof line, "a packet was last lost on the way %.1f s before", double(now - loss) / 1000.0);
        facts.push_back(line);
    }
    std::snprintf(line, sizeof line, "this game runs for %.1f min; log slot %d; drops told before in it: %u",
                  double(now - st.startedMs) / 60000.0, wlog::slot(), st.reports);
    facts.push_back(line);
    const std::string said = st.trail.summary(d, st.map);
    const std::string text = st.trail.text(d, clockNow(), kVersion, facts);
    LOG_INFO("net trail: a connection lost - %s server, %s%s; its connection %.1f s old, %llu B in, %llu B out; "
             "connection %d of its kind in this game, back to the characters %d time(s) before",
             nettrail::roleName(d.role),
             d.notice           ? "the server's notice"
             : d.closedByServer ? "the server closed it"
             : d.sending        ? "a write failed"
             : d.error          ? "a read failed"
                                : "how is not known",
             d.notice ? (std::string(", code ") + std::to_string(d.noticeCode)).c_str()
                      : d.error ? (std::string(", error ") + std::to_string(d.error)).c_str() : "",
             double(d.ageMs) / 1000.0, static_cast<unsigned long long>(d.in), static_cast<unsigned long long>(d.out),
             d.ofRole, d.restarts);
    ++st.reports;
    if (!st.on) {
        LOG_INFO("net trail: not sent - [report] disconnects is off");
        return;
    }
    if (st.lastReportMs && now - st.lastReportMs < kReportEveryMs) {
        LOG_INFO("net trail: not sent - one was told less than a minute ago");
        return;
    }
    const std::string file = trailFile();
    FILE* f = file.empty() ? nullptr : std::fopen(file.c_str(), "wb");
    if (!f) {
        LOG_INFO("net trail: its file could not be written - not sent");
        return;
    }
    const std::string whole = said + "\n\n" + text;
    std::fwrite(whole.data(), 1, whole.size(), f);
    std::fclose(f);
    st.lastReportMs = now;
    startNetReport(file);
}

void check(State& st) {
    nettrail::Drop d;
    {
        Locked l(st);
        if (!st.trail.dropPending(GetTickCount())) return;
        d = st.trail.takeDrop();
    }
    tell(st, d);
}

}  // namespace

void install() {
    State& st = state();
    if (st.lockMade) return;
    InitializeCriticalSection(&st.lock);
    st.lockMade = true;
    st.startedMs = GetTickCount64();
    // this uaRO.exe's connection code where it is known to be: its read's and write's calls
    st.sameExe = std::memcmp(reinterpret_cast<const void*>(uintptr_t(kRecvCallAt)), kRecvCall, sizeof kRecvCall) == 0 &&
                 std::memcmp(reinterpret_cast<const void*>(uintptr_t(kSendCallAt)), kSendCall, sizeof kSendCall) == 0;
}

void sitesTaken(int taken, int all) {
    state().sites = taken;
    state().sitesAll = all;
}

void beforePoll() {
    State& st = state();
    if (!st.sameExe) return;
    if (!st.closeTried) {  // 3.0.20-beta.64: now, over the pad's (it took the slot at the DLL's load)
        st.closeTried = true;
        void* was = nullptr;
        if (patchSlot(importSlotByOrdinal("ws2_32.dll", kCloseSocketOrdinal), reinterpret_cast<void*>(&closeHook),
                      &was))
            st.close = reinterpret_cast<CloseFn>(was);
    }
    auto** recvAt = reinterpret_cast<void**>(uintptr_t(kRecvSlot));
    auto** sendAt = reinterpret_cast<void**>(uintptr_t(kSendSlot));
    void* const ourRecv = reinterpret_cast<void*>(&recvHook);
    void* const ourSend = reinterpret_cast<void*>(&sendHook);
    if (!*recvAt || !*sendAt) return;  // the game has not asked ws2_32.dll for them yet
    if (*recvAt == ourRecv && *sendAt == ourSend) return;
    // ws2_32's own still there: the pad's turn first (the game may also fill them anew - the pad then takes them again)
    const HMODULE ws = GetModuleHandleA("ws2_32.dll");
    void* const realRecv = ws ? reinterpret_cast<void*>(GetProcAddress(ws, "recv")) : nullptr;
    void* const realSend = ws ? reinterpret_cast<void*>(GetProcAddress(ws, "send")) : nullptr;
    const DWORD now = GetTickCount();
    if (!st.firstLook) st.firstLook = now ? now : 1;
    const bool bare = *recvAt == realRecv || *sendAt == realSend;
    if (bare && now - st.firstLook < kPadGraceMs) return;
    if (*recvAt != ourRecv) {
        st.recv = reinterpret_cast<RecvFn>(*recvAt);
        *recvAt = ourRecv;
    }
    if (*sendAt != ourSend) {
        st.send = reinterpret_cast<SendFn>(*sendAt);
        *sendAt = ourSend;
    }
    st.firstLook = 0;  // filled anew later: the pad's turn first again
    if (st.swapped) return;
    st.swapped = true;
    LOG_INFO("net trail: the game's connections watched - its reads and writes%s%s, %d of its %d call sites (the "
             "login screens' loop, the connects); a connection lost is told the developers by itself ([report] "
             "disconnects)",
             bare ? "" : " (over uaRO Pad's own)",
             st.close ? ", every closesocket" : " (closesocket's import not found: who closes is not known)",
             st.sites, st.sitesAll);
}

void connected(void* connection, const char* address, nettrail::Role role, bool ok) {
    State& st = state();
    if (!st.sameExe || !connection || !address) return;
    uint32_t socket = 0;
    uint16_t port = 0;
    std::memcpy(&socket, static_cast<const uint8_t*>(connection) + 4, sizeof socket);
    std::memcpy(&port, address + 0x10, sizeof port);
    const int error = ok ? 0 : int(GetLastError());
    const uint32_t ip = ipOf(address);
    int number = 0;
    {
        Locked l(st);
        st.trail.connected(GetTickCount(), socket, ip, port, role, ok, error);
        number = st.trail.links().back().number;
    }
    LOG_INFO("net trail: connection %d - the %s server %s:%u%s", number, nettrail::roleName(role),
             nettrail::ipText(ip).c_str(), unsigned(port), ok ? "" : ", the connect did not start");
}

void packet(nettrail::Mode mode, uint16_t id, const uint8_t* data, size_t readable) {
    State& st = state();
    if (!st.sameExe) return;
    Locked l(st);
    st.trail.packet(GetTickCount(), mode, id, data, readable);
}

void polled(nettrail::Mode mode, bool fine) {
    State& st = state();
    if (!st.sameExe) return;
    {
        Locked l(st);
        st.trail.polled(GetTickCount(), mode, fine);
    }
    if (!fine) check(st);
}

void frame(bool on, const std::string& map, bool foreground) {
    State& st = state();
    st.on = on;
    st.map = map;
    st.foreground = foreground;
    if (st.sameExe) check(st);  // a notice's drop waits for its connection's end
}

}  // namespace uagfx::win::nettrailwin
