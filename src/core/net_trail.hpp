#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

// 3.0.20-beta.60 (the owner: «есть какой-то глюк, в котором клиент выбрасывает с дисконнектом, надо его отловить ...
// когда такое случается, отправлять отчёт на сервер, чтобы выяснить, при каких обстоятельствах это происходит»; the
// server's keeper: «раньше это происходило при перелогине/смене чара ... старый сокет закрывается и тянет за собой
// новый ... тебя после первого входа выкинуло или после перезахода? и сколько времени прошло после выхода и
// повторного входа»): the trail of the game's connections - each one the game opens (the account, the character and
// the map server's), what came and went over it, who closed it and how (the game itself, the server, an error), the
// servers' own notices and the turns of a session (the character chosen, the map entered, back to the characters) -
// kept as the game runs; a connection lost is a drop: what it was, with its story, for a report by itself
// (win/net_trail.cpp writes and sends it). uaRO.exe's own: CConnection (FUN_00418af0 connect, FUN_00418c60 poll,
// FUN_00418eb0 read, FUN_00418db0 write, FUN_00418c90 close).
namespace uagfx::nettrail {

enum class Role : uint8_t { Unknown, Account, Char, Zone };  // whose server: by the game's own connect
enum class Mode : uint8_t { Login, Game };                   // the game's mode whose loop reads the connection
// who shut the socket: the game by its own will (a server chosen, a map on another server, leaving), or its
// connection's code after a read or a write failed, or after a connect that did not start
enum class Closer : uint8_t { Game, AfterRecv, AfterSend, AfterConnect, Other };
enum class What : uint8_t { Connect, ConnectFailed, Close, RecvEnd, SendFail, Drop, Notice, Mark };

constexpr int kWouldBlock = 10035, kNotConnected = 10057;  // Winsock's errors that are no end (the game's own rule)
constexpr size_t kMostEvents = 400, kMostLinks = 64, kLastIds = 8;
constexpr uint32_t kNoticeJoinMs = 3000;  // a notice and the connection's end after it are one drop
constexpr uint32_t kGapFromMs = 1000, kGapWindowMs = 30000;  // the loop's pauses worth telling, how long kept
constexpr uint16_t kNoticeId = 0x0081, kRestartAck = 0x00B3, kQuitAck = 0x018B;
// 3.0.21-beta.5 (R-KQRYFF): the notice «another one logged in with this account» - the player's own other game
// window when it took the login (core/game_logins takenBy) within kOwnWindowMs of it: not a drop to report
constexpr int kNoticeOtherLogin = 2;
constexpr uint32_t kOwnWindowMs = 15000;

struct Event {
    uint32_t ms = 0;  // the game's clock (GetTickCount)
    What what = What::Connect;
    int link = 0;  // the connection's number in this game (from 1; 0: none known)
    // Connect/ConnectFailed: the socket, the error; Close: the Closer; RecvEnd: 0 closed by the server or the error;
    // SendFail: the error; Drop: the Mode; Notice: its code; Mark: the packet's id, its first value
    uint32_t a = 0, b = 0;
};

struct Link {
    int number = 0;
    int ofRole = 0;  // which connection to a server of its kind (1 the first)
    uint32_t socket = 0, ip = 0;  // ip as the bytes of its text (a.b.c.d: a the lowest)
    uint16_t port = 0;
    Role role = Role::Unknown;
    uint32_t openedMs = 0, closedMs = 0;
    bool open = false, started = false;  // started: the connect went off
    Closer closedBy = Closer::Other;
    uint64_t in = 0, out = 0;
    uint32_t lastInMs = 0, lastOutMs = 0;
    bool anyIn = false, anyOut = false;
    bool ended = false;  // a read said so: endError 0 the server closed it, else the error
    int endError = 0;
    bool sendFailed = false;
    int sendError = 0;
    uint32_t packets = 0;
    uint16_t last[kLastIds] = {};  // the newest packets' ids, the newest last
    size_t lastCount = 0;
};

// a connection lost, or the server's notice of it
struct Drop {
    uint32_t ms = 0;
    Mode mode = Mode::Game;
    int link = 0;  // 0: none known (a notice with no connection open)
    Role role = Role::Unknown;
    bool notice = false;
    int noticeCode = 0;
    bool closedByServer = false;  // a read returned 0
    int error = 0;                // a read's or a write's error; 0 none
    bool sending = false;         // the error was a write's
    uint32_t ageMs = 0;           // the connection's life
    uint32_t idleInMs = 0;        // since the server's last bytes (the life when none came)
    uint32_t idleOutMs = 0;
    uint64_t in = 0, out = 0;
    int ofRole = 0;               // which connection to this server's kind it was in this game (1 the first)
    int restarts = 0;             // times the game went back to the characters before it
    bool hadBefore = false;       // another connection was closed before it
    Role beforeRole = Role::Unknown;
    Closer beforeBy = Closer::Other;
    uint32_t sinceBeforeMs = 0;   // from that close to this connection's start
    uint32_t gapMs = 0;           // the game's loop's longest pause in the half minute before
    std::vector<uint16_t> lastIds;
};

// the wall clock at a moment of the game's clock - the trail's times are told by it
struct Clock {
    uint32_t ms = 0;
    int hour = 0, minute = 0, second = 0, milli = 0;
};

class Trail {
public:
    // the game's connect: `ok` it went off (the socket does not block: its end comes with the first read or write)
    void connected(uint32_t ms, uint32_t socket, uint32_t ip, uint16_t port, Role role, bool ok, int error);
    void received(uint32_t ms, uint32_t socket, int result, int error);  // recv's answer (error with -1)
    void sent(uint32_t ms, uint32_t socket, int result, int error);
    void closed(uint32_t ms, uint32_t socket, Closer by);
    // a packet the game takes in `mode`: counted on the open connection; a notice and the session's turns marked
    void packet(uint32_t ms, Mode mode, uint16_t id, const uint8_t* data, size_t size);
    // the mode's loop asked its connection: false - lost (the game shows «disconnected» now)
    void polled(uint32_t ms, Mode mode, bool fine);
    bool dropPending(uint32_t ms) const;  // a drop to tell: at once, a notice once its connection's end had its time
    Drop takeDrop();
    const std::vector<Link>& links() const { return links_; }
    const std::deque<Event>& events() const { return events_; }
    // the drop in Ukrainian for the report's first words: what, how, when, what was before
    // `sinceGameMs`: how long before the drop the game before this one ended (sinceBefore; -1 not known)
    std::string summary(const Drop& d, const std::string& map, int sinceGameMs = -1) const;
    // the report's file: the drop in English, `facts` (the caller's lines), every connection, the trail's events
    std::string text(const Drop& d, const Clock& clock, const std::string& version,
                     const std::vector<std::string>& facts) const;

private:
    Link* linkOf(uint32_t socket);
    Link* openOf(Mode mode);
    void note(uint32_t ms, What what, int link, uint32_t a, uint32_t b);
    void drop(uint32_t ms, Mode mode, const Link* l, bool notice, int code);
    void fill(Drop& d, uint32_t ms, const Link* l) const;
    std::vector<Link> links_;
    std::deque<Event> events_;
    int numbers_ = 0, restarts_ = 0;
    int roles_[4] = {0, 0, 0, 0};
    bool pending_ = false, ready_ = false, lastPolled_[2] = {false, false};
    Drop drop_;
    uint32_t lastPollMs_[2] = {0, 0};
    std::deque<std::pair<uint32_t, uint32_t>> gaps_;  // when, how long
};

// the settings app's count of the trails it sent - `stamps` its file (a time a line, seconds), `now`: true when
// one more may go (kSendEverySec after the last, kSendADay in a day at most), `next` the file with it
constexpr long long kSendEverySec = 600, kSendDaySec = 86400;
constexpr size_t kSendADay = 6;
bool maySend(const std::string& stamps, long long now, std::string& next);
// the trail's file: its first line the summary for the report's words (the driver writes it so), "" none
std::string firstLine(const std::string& file);

// 3.0.20-beta.66 (R-BTF3ZP: the notice 8 six seconds after a game was closed on a map and started again): the game
// before this one on this computer, from the end of its log (the driver keeps it as .prev.log) - its last line's
// time and its last connection's line; a drop within kBeforeMostMs of that end is told with it
struct Before {
    bool known = false;
    int endMs = -1;  // of the day
    std::string endTime, lastLink, linkTime;
};
constexpr int kBeforeMostMs = 10 * 60 * 1000;
Before beforeOf(const std::string& logsEnd);
// milliseconds from that game's end to the drop (the drop's moment `dropMs` by the clock given); -1: not known, or
// more than kBeforeMostMs - another sitting
int sinceBefore(const Before& b, const Clock& clock, uint32_t dropMs);
std::string beforeFact(const Before& b, const Clock& clock, uint32_t dropMs);  // the file's line; "" none

const char* roleName(Role r);        // "account", "character", "map"
const char* errorName(int error);    // Winsock's: "reset by the server or the network"; "" unknown
const char* noticeName(int code);    // the server's notice 0x0081
const char* packetName(uint16_t id); // the marked ones; "" others
std::string ipText(uint32_t ip);

}  // namespace uagfx::nettrail
