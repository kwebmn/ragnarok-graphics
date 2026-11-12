#include "win/net_watch.hpp"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/fight_trace.hpp"
#include "core/merc_packet.hpp"
#include "core/minimap_proto.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/guild_card.hpp"
#include "win/log.hpp"
#include "win/merc_card.hpp"
#include "win/net_predict.hpp"
#include "win/net_smooth.hpp"

namespace uagfx::win::netwatchwin {
namespace {

// uaRO.exe's: its connection object's socket (+4, FUN_00418af0 makes it), the character's own account
constexpr uint32_t kSocketAt = 4, kOwnAccount = 0x771F7C;
constexpr uint16_t kFixPos = 0x0088;  // ZC_STOPMOVE <unit>.L <x>.W <y>.W - the server puts a unit on a cell
constexpr uint16_t kMove = 0x0086;    // ZC_NOTIFY_MOVE <unit>.L <the way>.6B <its server tick>.L - another unit walks
// 3.0.11-beta.128: the server's ticks for the held packets - the character's walk (ZC_NOTIFY_PLAYERMOVE <tick>.L <the
// way>.6B), a hit (ZC_NOTIFY_ACT <from>.L <to>.L <tick>.L ...); the game's server clock: timeGetTime() less the
// session's +0x5E4 (FUN_0062dbb0 keeps it; a move more than 144 ms behind it is walked on from where it should be)
constexpr uint16_t kOwnMove = 0x0087, kAct = 0x008A;
constexpr uint16_t kParam = 0x00B0;  // 3.0.11-beta.134: ZC_PAR_CHANGE <its kind>.W <value>.L - 0 the speed (ms a cell)
constexpr uint32_t kClockOffset = 0x771148 + 0x5E4;
constexpr ULONGLONG kReadMs = 1000;

// SIO_TCP_INFO and its TCP_INFO_v0 (mstcpip.h keeps them for Windows 10 1703 builds; ours is older), MSVC's layout
constexpr DWORD kTcpInfo = _WSAIORW(IOC_VENDOR, 39);
// 3.0.11-beta.128: SIO_TCP_SET_ACK_FREQUENCY and its TCP_ACK_FREQUENCY_PARAMETERS (Windows 8 and later): 1 each
// segment acknowledged at once, 2 Windows' own (every second one, a lone one after up to 200 ms)
constexpr DWORD kAckFrequency = _WSAIOW(IOC_VENDOR, 23);
struct AckFrequency {
    UCHAR segments = 2;
};
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
using LastErrorFn = int(WSAAPI*)();
using TimeFn = DWORD(WINAPI*)();

struct State {
    netwatch::Watch watch;
    void* connection = nullptr;
    int mercTold = 0;  // 3.0.11-beta.218
    SOCKET nodelayOf = INVALID_SOCKET;  // the socket TCP_NODELAY was set on
    ULONGLONG readAt = 0;
    bool loaded = false, infoTold = false;
    SetSockOptFn setOpt = nullptr;
    IoctlFn ioctl = nullptr;
    int fixesTold = 0;
    // 3.0.11-beta.128: quick ACKs wanted, the socket and the frequency they were set to; the game's clock (winmm)
    bool quickAcks = true;
    SOCKET acksOf = INVALID_SOCKET;
    UCHAR acksSet = 0;
    LastErrorFn lastError = nullptr;
    TimeFn time = nullptr;
    int holdsTold = 0;
    ULONGLONG polledAt = 0, readGap = 0;  // 3.0.11-beta.131: the game's last read of its socket, the one before it
    ULONGLONG longGapAt = 0;              // 3.0.11-beta.134: the last long pause between reads (a backlog after it)
    fighttrace::Trace trace;              // 3.0.11-beta.247 (R-AMNV99)
    uint32_t follow = 0;                  // 3.0.11-beta.249: the unit dumped last, its shown cell followed
    ULONGLONG followFrom = 0;
    int followX = -1, followY = -1;
};

State& state() {
    static State s;
    return s;
}

int rd16(const uint8_t* p, size_t at) { return int(uint16_t(p[at] | p[at + 1] << 8)); }

// 3.0.11-beta.247: the cell the game shows a unit on (before it takes the packet)
bool shownCell(uint32_t unit, int& x, int& y) {
    float world[3] = {}, own[3] = {};
    int w = 0, h = 0, cell = 0;
    minimap::Cell c;
    if (!readActorWorld(unit, world) || !readPlayerWorld(own, w, h, cell) ||
        !minimap::cellOf(world[0], world[2], w, h, cell, c))
        return false;
    x = c.x;
    y = c.y;
    return true;
}

// 3.0.11-beta.249: the packets of a unit at +2 traced besides - one coming into sight (standing, walking: 0x0078,
// 0x007b, 0x007c and the later 0x01d8-0x01da, 0x022a-0x022c), its facing 0x009c, looks 0x0119/0x0229, a cast 0x013e,
// a class 0x01b0, an emotion 0x00c0, its name 0x0095/0x0195
bool traced(uint16_t id) {
    switch (id) {
        case 0x008A: case 0x0086: case 0x0088: case 0x0080: case 0x0078: case 0x007B: case 0x007C: case 0x01D8:
        case 0x01D9: case 0x01DA: case 0x022A: case 0x022B: case 0x022C: case 0x009C: case 0x0119: case 0x0229:
        case 0x013E: case 0x01B0: case 0x00C0: case 0x0095: case 0x0195: return true;
        default: return false;
    }
}

// 3.0.11-beta.247 (R-AMNV99 «замерал и телепортировался»): the fought units' packets kept, written out when one jumps
constexpr uint16_t kVanish = 0x0080;  // ZC_NOTIFY_VANISH <unit>.L <type>.B (1 died)
void trace(State& s, uint16_t id, const uint8_t* data, ULONGLONG now) {
    using fighttrace::Event;
    using fighttrace::Kind;
    const uint32_t me = read32(kOwnAccount);
    if (!me) return;
    uint32_t unit = 0;
    std::memcpy(&unit, data + 2, 4);
    Event e;
    e.ms = now;
    if (id == kAct) {
        uint32_t target = 0, swing = 0, flinch = 0;
        std::memcpy(&target, data + 6, 4);
        std::memcpy(&swing, data + 14, 4);
        std::memcpy(&flinch, data + 18, 4);
        if (unit == me && target && target != me) {
            e.kind = Kind::HitIt;
            e.unit = target;
        } else if (target == me && unit && unit != me) {
            e.kind = Kind::HitsMe;
            e.unit = unit;
        } else {
            return;
        }
        e.a = int(std::min<uint32_t>(swing, 100000));
        e.b = int(std::min<uint32_t>(flinch, 100000));
        e.c = int(int16_t(rd16(data, 22)));
    } else {
        // 3.0.11-beta.250: every unit's walks and fixes kept (a fight may begin after its walk did), the rest of the
        // fought ones only
        if (unit == me || !unit || (id != kMove && id != kFixPos && !s.trace.fought(unit, now))) return;
        e.unit = unit;
        if (id == kMove) {
            e.kind = Kind::Move;
            fighttrace::way(data + 6, e.a, e.b, e.c, e.d);
        } else if (id == kFixPos) {
            e.kind = Kind::Fix;
            e.a = rd16(data, 6);
            e.b = rd16(data, 8);
        } else if (id == kVanish && data[6] == 1) {
            e.kind = Kind::Died;
        } else {
            e.kind = Kind::Other;  // 3.0.11-beta.249: its other packets (the walking ones of a changed speed, ...)
            e.a = id;
        }
    }
    shownCell(e.unit, e.shownX, e.shownY);
    shownCell(me, e.ownX, e.ownY);
    s.trace.add(e);
    int away = 0;
    const char* what = nullptr;
    if (e.shownX >= 0 && e.kind == Kind::Fix) {
        away = std::max(std::abs(e.shownX - e.a), std::abs(e.shownY - e.b));
        what = "put";
    } else if (e.shownX >= 0 && e.kind == Kind::Move) {
        away = std::max(std::abs(e.shownX - e.a), std::abs(e.shownY - e.b));
        what = "walks from";
    }
    // 3.0.11-beta.248 (the owner: «моб бьет стоя гдето далеко»): a hit at the character from 2+ cells as shown
    float own[3] = {};
    int w = 0, h = 0, cell = 0;
    minimap::Cell ownAt;
    if (e.kind == Kind::HitsMe && e.shownX >= 0 && readPlayerWorld(own, w, h, cell) &&
        minimap::cellOf(own[0], own[2], w, h, cell, ownAt)) {
        away = std::max(std::abs(e.shownX - ownAt.x), std::abs(e.shownY - ownAt.y));
        what = "hits the character from";
    }
    if (!what || away < 2 || !s.trace.fought(e.unit, now)) return;
    char why[80];
    std::snprintf(why, sizeof why, "%s %d cells from where the game showed it", what, away);
    // 3.0.11-beta.249 (R-GKXJT6): the game's own list of it - two of one id, or ours a stale one
    bool listed = false;
    const int copies = actorsOfId(e.unit, actorAddress(e.unit), listed);
    Event list;
    list.ms = now;
    list.kind = Kind::Shown;
    list.unit = e.unit;
    list.a = copies;
    list.b = listed ? 1 : 0;
    s.trace.add(list);
    const std::string out = s.trace.dump(e.unit, now, why);
    if (out.empty()) return;
    LOG_INFO("%s", out.c_str());
    s.follow = e.unit;
    s.followFrom = now;
    s.followX = e.shownX;
    s.followY = e.shownY;
}

// 3.0.11-beta.249: after a dump, where the game moves the unit over the next 3 s
void followFrame(State& s, ULONGLONG now) {
    if (!s.follow) return;
    int x = -1, y = -1;
    const bool seen = shownCell(s.follow, x, y);
    if (!seen || now - s.followFrom > 3000) {
        LOG_INFO("fight trace: %u %s %u ms after", s.follow, seen ? "followed till" : "gone from the game's world",
                 unsigned(now - s.followFrom));
        s.follow = 0;
        return;
    }
    if (x == s.followX && y == s.followY) return;
    LOG_INFO("fight trace: %u shown at %d, %d %u ms after", s.follow, x, y, unsigned(now - s.followFrom));
    s.followX = x;
    s.followY = y;
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
    s.lastError = reinterpret_cast<LastErrorFn>(reinterpret_cast<void*>(GetProcAddress(ws, "WSAGetLastError")));
}

// 3.0.11-beta.128: the game's own clock (its winmm, loaded already; GetTickCount near enough without it)
DWORD gameTime(State& s) {
    if (!s.time) {
        const HMODULE winmm = GetModuleHandleA("winmm.dll");
        s.time = winmm ? reinterpret_cast<TimeFn>(reinterpret_cast<void*>(GetProcAddress(winmm, "timeGetTime")))
                       : &GetTickCount;
        if (!s.time) s.time = &GetTickCount;
    }
    return s.time();
}

// A server-ticked packet's lateness against the game's server clock (ms; < 0 its clock behind - set by it)
int lateness(State& s, const uint8_t* data, size_t tickAt) {
    uint32_t tick = 0;
    std::memcpy(&tick, data + tickAt, 4);
    const uint32_t offset = read32(kClockOffset);
    return int(int32_t(gameTime(s) - offset - tick));
}

SOCKET socketOf(void* connection) {
    SOCKET sock = INVALID_SOCKET;
    if (!connection || !readGame(uint32_t(uintptr_t(connection)) + kSocketAt, &sock, sizeof sock))
        return INVALID_SOCKET;
    return sock;
}

}  // namespace

void packet(void* connection, uint16_t id, const uint8_t* data) {
    State& s = state();
    s.connection = connection;
    // 3.0.11-beta.218 (core/merc_packet): the mercenary's ZC_MER_INIT laid out as this build of the game reads it
    if (id == mercpacket::kInit && data &&
        mercpacket::laidOut(const_cast<uint8_t*>(data), mercpacket::kInitSize) && s.mercTold++ < 3)
        LOG_INFO("merc: its 0x029b laid out for the game (HP, SP, the contract's end, loyalty as uaRO.exe reads them)");
    if (data && (id == 0x0160 || id == 0x0162 || id == 0x0163 || id == 0x0166 || id == 0x016F)) {  // .220
        uint16_t len = 182;  // 0x016f's fixed
        if (id != 0x016F) std::memcpy(&len, data + 2, 2);
        guildcardwin::packet(id, data, len);
    }
    if (id == 0x0091 || id == 0x0092) netsmoothwin::mapChange();  // 3.0.18-beta.201 (R-SHXHET): a warp - no glide on
    if (id == mercpacket::kSkills && data) {  // 3.0.11-beta.218: the mercenary's skills, for its card's list
        uint16_t len = 0;
        std::memcpy(&len, data + 2, 2);
        merccardwin::skillsSeen(data, len);
    }
    if (!inGameWorld()) return;
    const ULONGLONG now = GetTickCount64();
    s.watch.packet(now);
    if (data && traced(id)) trace(s, id, data, now);  // 3.0.11-beta.247
    if (id == kVanish && data) {  // 3.0.11-beta.251: no walk kept for a unit gone
        uint32_t unit = 0;
        std::memcpy(&unit, data + 2, 4);
        netsmoothwin::gone(unit);
    }
    if ((id == kMove || id == kOwnMove || id == kAct) && data) {  // 3.0.11-beta.128: held by a lost packet?
        const int late = lateness(s, data, id == kMove ? 12 : id == kOwnMove ? 2 : 10);
        const int way = netwatch::onTheWay(late, uint32_t(std::min<ULONGLONG>(s.readGap, 60000)));  // .131
        if (way > 0 && late < 60000 && !netwatch::inBacklog(now, s.longGapAt)) s.watch.late(way, now);  // .134
    }
    if (id == kOwnMove && data) netpredictwin::ownMove(const_cast<uint8_t*>(data));  // 3.0.11-beta.130
    if (id == kAct && data) netpredictwin::act(data);  // 3.0.11-beta.132: the server's hold on the own walk
    if (id == kParam && data && rd16(data, 2) == 0) {  // 3.0.11-beta.134: the character's speed
        uint32_t value = 0;
        std::memcpy(&value, data + 4, 4);
        netpredictwin::speed(int(value));
    }
    if (id == kMove && data) {  // 3.0.11-beta.124: where it is shown before the game walks it (its jump faded)
        uint32_t unit = 0;
        std::memcpy(&unit, data + 2, 4);
        if (unit != read32(kOwnAccount)) {
            netsmoothwin::moveSeen(unit);
            netsmoothwin::walkSeen(unit, data);  // 3.0.11-beta.251
        }
        return;
    }
    if (id != kFixPos || !data) return;
    uint32_t unit = 0;
    std::memcpy(&unit, data + 2, 4);
    const int x = rd16(data, 6), y = rd16(data, 8);
    const bool own = unit != 0 && unit == read32(kOwnAccount);
    if (own) netpredictwin::ownFix(x, y);  // 3.0.11-beta.134: for the log of a guess walked another way
    // 3.0.11-beta.123: how far it stood (each unit's), the fix glided to when near (the packet's cell its own then)
    const int cells = netsmoothwin::fix(unit, const_cast<uint8_t*>(data));
    s.watch.fix(own, std::max(cells, 0), now);
    if (own && cells >= 2 && s.fixesTold++ < 20)
        LOG_INFO("net: the server put the character %d cells from where the game showed it (to %d, %d)", cells, x, y);
}

void frame() {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (inGameWorld()) followFrame(s, now);  // 3.0.11-beta.249
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
    // 3.0.11-beta.128 ([debug] quick_acks): each of the server's packets acknowledged at once - its round trip true,
    // its lost packet sent again sooner; off: Windows' own (2)
    if (const UCHAR want = s.quickAcks ? 1 : 2; sock != s.acksOf || want != s.acksSet) {
        const bool first = sock != s.acksOf;
        s.acksOf = sock;
        s.acksSet = want;
        if (want == 1 || !first) {
            AckFrequency p;
            p.segments = want;
            DWORD none = 0;
            const int r = s.ioctl(sock, kAckFrequency, &p, sizeof p, nullptr, 0, &none, nullptr, nullptr);
            const int error = r == 0 ? 0 : s.lastError ? s.lastError() : int(GetLastError());
            if (want == 1)
                LOG_INFO("net: quick ACKs on the map server's socket %s (each of its packets acknowledged at once, "
                         "not after up to 200 ms)%s", r == 0 ? "set" : "NOT set",
                         r == 0 ? "" : (" - Windows' error " + std::to_string(error)).c_str());
            else
                LOG_INFO("net: Windows' delayed ACKs back on the map server's socket ([debug] quick_acks = 0)%s",
                         r == 0 ? "" : (" - NOT set, Windows' error " + std::to_string(error)).c_str());
        }
    }
    if (s.watch.held() && s.holdsTold++ < 40)  // 3.0.11-beta.128 (told even without TCP_INFO)
        LOG_INFO("net: the server's packets held %d ms - one of them lost on the way, the ones after it came at once "
                 "(the world stood, then jumped)", s.watch.holdMs());
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

uint32_t serverNow() { return gameTime(state()) - read32(kClockOffset); }

uint64_t lastLossMs() { return state().watch.lastLossMs(); }

void polled() {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    s.readGap = s.polledAt ? now - s.polledAt : 0;
    s.polledAt = now;
    if (s.readGap >= netwatch::kBacklogGapMs) s.longGapAt = now;  // 3.0.11-beta.134
}

void setQuickAcks(bool on) {
    State& s = state();
    if (on == s.quickAcks) return;
    s.quickAcks = on;
    LOG_INFO("net: [debug] quick_acks = %d", on ? 1 : 0);
}

std::vector<int> graph() { return state().watch.graph(); }

}  // namespace uagfx::win::netwatchwin
