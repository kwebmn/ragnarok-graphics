#include "core/net_trail.hpp"

#include <algorithm>
#include <cstdio>

namespace uagfx::nettrail {

namespace {

std::string hexId(uint16_t id) {
    char b[12];
    std::snprintf(b, sizeof b, "0x%04X", unsigned(id));
    return b;
}

// "0.8 s", "12.5 s", "3 min 05 s", "1 h 02 min"
std::string spanEn(uint32_t ms) {
    char b[40];
    if (ms < 60000) std::snprintf(b, sizeof b, "%u.%u s", ms / 1000, ms % 1000 / 100);
    else if (ms < 3600000) std::snprintf(b, sizeof b, "%u min %02u s", ms / 60000, ms % 60000 / 1000);
    else std::snprintf(b, sizeof b, "%u h %02u min", ms / 3600000, ms % 3600000 / 60000);
    return b;
}

// the same in Ukrainian: "0,8 с", "3 хв 05 с", "1 год 02 хв"
std::string spanUk(uint32_t ms) {
    char b[48];
    if (ms < 60000) std::snprintf(b, sizeof b, "%u,%u с", ms / 1000, ms % 1000 / 100);
    else if (ms < 3600000) std::snprintf(b, sizeof b, "%u хв %02u с", ms / 60000, ms % 60000 / 1000);
    else std::snprintf(b, sizeof b, "%u год %02u хв", ms / 3600000, ms % 3600000 / 60000);
    return b;
}

const char* roleUk(Role r) {
    switch (r) {
        case Role::Account: return "сервер входу";
        case Role::Char: return "сервер персонажів";
        case Role::Zone: return "сервер карти";
        case Role::Unknown: break;
    }
    return "сервер";
}

const char* errorUk(int error) {
    switch (error) {
        case 10054: return "скинуто сервером або мережею";
        case 10053: return "перервано на цьому комп'ютері";
        case 10060: return "час очікування вийшов";
        case 10061: return "сервер відмовив у з'єднанні";
        case 10050:
        case 10051:
        case 10065: return "немає шляху до сервера";
        case 10052: return "мережа розірвала з'єднання";
        default: break;
    }
    return "";
}

const char* noticeUk(int code) {
    switch (code) {
        case 0: return "від'єднано від сервера";
        case 1: return "сервер закрито";
        case 2: return "під цим обліковим записом увійшов хтось інший";
        case 3: return "відставання від сервера: швидкість або затримка";
        case 4: return "сервер переповнений";
        case 8: return "сервер ще пам'ятає попередній вхід";
        case 9: return "забагато з'єднань із цієї адреси";
        case 15: return "від'єднав ігровий майстер";
        default: break;
    }
    return "інша причина";
}

const char* closerUk(Closer c) {
    switch (c) {
        case Closer::Game: return "закрила сама гра";
        case Closer::AfterRecv: return "закрилося після невдалого читання (сервер або мережа)";
        case Closer::AfterSend: return "закрилося після невдалого відправлення";
        case Closer::AfterConnect: return "не встановилося";
        case Closer::Other: break;
    }
    return "закрито";
}

const char* closerEn(Closer c) {
    switch (c) {
        case Closer::Game: return "closed by the game";
        case Closer::AfterRecv: return "closed by the game's connection after the failed read";
        case Closer::AfterSend: return "closed by the game's connection after the failed write";
        case Closer::AfterConnect: return "closed after the connect that did not start";
        case Closer::Other: break;
    }
    return "closed";
}

// 2 -> "2-ге" (a connection's number: «з'єднання» is neuter)
std::string ordinalUk(int n) {
    const int ones = n % 10, tens = n % 100;
    const char* tail = "-те";
    if (ones == 1 && tens != 11) tail = "-ше";
    else if (ones == 2 && tens != 12) tail = "-ге";
    else if (ones == 3 && tens != 13) tail = "-тє";
    else if ((ones == 7 || ones == 8) && tens != 17 && tens != 18) tail = "-ме";
    return std::to_string(n) + tail;
}

std::string withName(int error, const char* name) {
    std::string out = std::to_string(error);
    if (name && *name) out += std::string(" (") + name + ")";
    return out;
}

std::string timeAt(const Clock& c, uint32_t ms) {
    const int64_t day = 24ll * 3600 * 1000;
    int64_t at = ((int64_t(c.hour) * 60 + c.minute) * 60 + c.second) * 1000 + c.milli - int64_t(int32_t(c.ms - ms));
    at = ((at % day) + day) % day;
    char b[24];
    std::snprintf(b, sizeof b, "%02d:%02d:%02d.%03d", int(at / 3600000), int(at / 60000 % 60), int(at / 1000 % 60),
                  int(at % 1000));
    return b;
}

}  // namespace

const char* roleName(Role r) {
    switch (r) {
        case Role::Account: return "account";
        case Role::Char: return "character";
        case Role::Zone: return "map";
        case Role::Unknown: break;
    }
    return "unknown";
}

const char* errorName(int error) {
    switch (error) {
        case 10004: return "interrupted";
        case 10038: return "not a socket";
        case 10050: return "the network is down";
        case 10051: return "the network is unreachable";
        case 10052: return "the network dropped the connection";
        case 10053: return "aborted on this computer";
        case 10054: return "reset by the server or the network";
        case 10058: return "shut down";
        case 10060: return "timed out";
        case 10061: return "refused";
        case 10065: return "no route to the host";
        default: break;
    }
    return "";
}

const char* noticeName(int code) {
    switch (code) {
        case 0: return "disconnected from the server";
        case 1: return "the server closed";
        case 2: return "another one logged in with this account";
        case 3: return "too far behind the server (speed or lag)";
        case 4: return "the server is full";
        case 5: return "under age";
        case 6: return "not paid";
        case 8: return "the server still remembers the last login";
        case 9: return "too many connections from this address";
        case 10: return "the paid time ended";
        case 15: return "disconnected by a game master";
        default: break;
    }
    return "";
}

const char* packetName(uint16_t id) {
    switch (id) {
        case 0x0069: return "the account server's answer: its servers";
        case 0x006A: return "the account server refused";
        case 0x006B: return "the characters' list";
        case 0x006C: return "the character server refused";
        case 0x0071: return "the map server's address";
        case 0x0073: return "the map entered";
        case 0x0081: return "the server's notice";
        case 0x0091: return "another map";
        case 0x0092: return "a map on another server";
        case 0x00B3: return "back to the characters allowed";
        case 0x018B: return "leaving allowed";
        default: break;
    }
    return "";
}

std::string ipText(uint32_t ip) {
    char b[20];
    std::snprintf(b, sizeof b, "%u.%u.%u.%u", ip & 255, ip >> 8 & 255, ip >> 16 & 255, ip >> 24);
    return b;
}

bool maySend(const std::string& stamps, long long now, std::string& next) {
    std::vector<long long> kept;
    long long value = 0;
    bool digits = false;
    for (size_t i = 0; i <= stamps.size(); ++i) {
        const char ch = i < stamps.size() ? stamps[i] : '\n';
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + (ch - '0');
            digits = true;
            continue;
        }
        if (digits && value <= now && now - value < kSendDaySec) kept.push_back(value);
        value = 0;
        digits = false;
    }
    std::sort(kept.begin(), kept.end());
    const bool may = kept.size() < kSendADay && (kept.empty() || now - kept.back() >= kSendEverySec);
    if (may) kept.push_back(now);
    next.clear();
    for (const long long t : kept) next += std::to_string(t) + "\n";
    return may;
}

std::string firstLine(const std::string& file) {
    size_t end = file.find('\n');
    if (end == std::string::npos) end = file.size();
    while (end > 0 && (file[end - 1] == '\r' || file[end - 1] == ' ')) --end;
    return file.substr(0, end);
}

void Trail::note(uint32_t ms, What what, int link, uint32_t a, uint32_t b) {
    Event e;
    e.ms = ms, e.what = what, e.link = link, e.a = a, e.b = b;
    events_.push_back(e);
    while (events_.size() > kMostEvents) events_.pop_front();
}

Link* Trail::linkOf(uint32_t socket) {
    for (auto it = links_.rbegin(); it != links_.rend(); ++it)
        if (it->socket == socket) return &*it;
    return nullptr;
}

Link* Trail::openOf(Mode mode) {
    Link* any = nullptr;
    for (auto it = links_.rbegin(); it != links_.rend(); ++it) {
        if (!it->open) continue;
        if (mode == Mode::Login || it->role == Role::Zone) return &*it;
        if (!any) any = &*it;
    }
    return any;
}

void Trail::connected(uint32_t ms, uint32_t socket, uint32_t ip, uint16_t port, Role role, bool ok, int error) {
    Link l;
    l.number = ++numbers_;
    l.ofRole = ++roles_[size_t(role) & 3];
    l.socket = socket, l.ip = ip, l.port = port, l.role = role;
    l.openedMs = ms;
    l.open = l.started = ok;
    links_.push_back(l);
    if (links_.size() > kMostLinks) links_.erase(links_.begin());
    note(ms, ok ? What::Connect : What::ConnectFailed, l.number, socket, uint32_t(error));
}

void Trail::received(uint32_t ms, uint32_t socket, int result, int error) {
    Link* l = linkOf(socket);
    if (!l) return;
    if (result > 0) {
        l->in += uint64_t(result);
        l->lastInMs = ms;
        l->anyIn = true;
        return;
    }
    if (result < 0 && (error == kWouldBlock || error == kNotConnected)) return;
    l->ended = true;
    l->endError = result == 0 ? 0 : error;
    note(ms, What::RecvEnd, l->number, uint32_t(l->endError), 0);
}

void Trail::sent(uint32_t ms, uint32_t socket, int result, int error) {
    Link* l = linkOf(socket);
    if (!l) return;
    if (result > 0) {
        l->out += uint64_t(result);
        l->lastOutMs = ms;
        l->anyOut = true;
        return;
    }
    if (result >= 0 || error == kWouldBlock || error == kNotConnected) return;
    l->sendFailed = true;
    l->sendError = error;
    note(ms, What::SendFail, l->number, uint32_t(error), 0);
}

void Trail::closed(uint32_t ms, uint32_t socket, Closer by) {
    Link* found = nullptr;
    for (auto it = links_.rbegin(); it != links_.rend() && !found; ++it)
        if (it->socket == socket && (it->open || (!it->started && !it->closedMs))) found = &*it;
    if (found) {
        found->open = false;
        found->closedMs = ms;
        found->closedBy = by;
    }
    note(ms, What::Close, found ? found->number : 0, uint32_t(by), 0);
}

void Trail::packet(uint32_t ms, Mode mode, uint16_t id, const uint8_t* data, size_t size) {
    Link* l = openOf(mode);
    if (l) {
        ++l->packets;
        if (l->lastCount < kLastIds) {
            l->last[l->lastCount++] = id;
        } else {
            std::copy(l->last + 1, l->last + kLastIds, l->last);
            l->last[kLastIds - 1] = id;
        }
    }
    if (!*packetName(id)) return;
    const uint32_t value = data && size > 2 ? data[2] : 0;
    if (id == kNoticeId) {
        note(ms, What::Notice, l ? l->number : 0, value, 0);
        drop(ms, mode, l, true, int(value));
        return;
    }
    if (id == kRestartAck && value == 1) ++restarts_;
    note(ms, What::Mark, l ? l->number : 0, id, value);
}

void Trail::polled(uint32_t ms, Mode mode, bool fine) {
    const size_t m = size_t(mode) & 1;
    if (lastPolled_[m] && ms - lastPollMs_[m] >= kGapFromMs) gaps_.push_back({ms, ms - lastPollMs_[m]});
    lastPolled_[m] = true;
    lastPolled_[1 - m] = false;  // the other mode's loop stands while this one runs: no pause of its own
    lastPollMs_[m] = ms;
    while (!gaps_.empty() && ms - gaps_.front().first > kGapWindowMs) gaps_.pop_front();
    if (fine) return;
    const Link* lost = nullptr;  // the newest one shut after a read or a write failed
    for (auto it = links_.rbegin(); it != links_.rend() && !lost; ++it)
        if (!it->open && (it->closedBy == Closer::AfterRecv || it->closedBy == Closer::AfterSend)) lost = &*it;
    note(ms, What::Drop, lost ? lost->number : 0, uint32_t(mode), 0);
    drop(ms, mode, lost, false, 0);
}

void Trail::fill(Drop& d, uint32_t ms, const Link* l) const {
    d.restarts = restarts_;
    for (const auto& g : gaps_)
        if (ms - g.first <= kGapWindowMs) d.gapMs = std::max(d.gapMs, g.second);
    if (!l) return;
    d.link = l->number;
    d.role = l->role;
    d.ofRole = l->ofRole;
    d.closedByServer = l->ended && l->endError == 0;
    d.error = l->ended ? l->endError : l->sendFailed ? l->sendError : 0;
    d.sending = !l->ended && l->sendFailed;
    const uint32_t end = l->open ? ms : l->closedMs;
    d.ageMs = end - l->openedMs;
    d.idleInMs = l->anyIn ? end - l->lastInMs : d.ageMs;
    d.idleOutMs = l->anyOut ? end - l->lastOutMs : d.ageMs;
    d.in = l->in, d.out = l->out;
    d.lastIds.assign(l->last, l->last + l->lastCount);
    for (auto it = links_.rbegin(); it != links_.rend(); ++it) {
        if (it->number >= l->number || it->open) continue;
        d.hadBefore = true;
        d.beforeRole = it->role;
        d.beforeBy = it->closedBy;
        d.sinceBeforeMs = l->openedMs - it->closedMs;
        break;
    }
}

void Trail::drop(uint32_t ms, Mode mode, const Link* l, bool notice, int code) {
    if (pending_ && drop_.notice && !notice && ms - drop_.ms <= kNoticeJoinMs) {
        Drop joined;  // the notice's connection ended: one drop, the notice its cause
        joined.ms = drop_.ms, joined.mode = mode;
        joined.notice = true, joined.noticeCode = drop_.noticeCode;
        fill(joined, ms, l ? l : nullptr);
        if (!l) joined.link = drop_.link, joined.role = drop_.role;
        drop_ = joined;
        ready_ = true;
        return;
    }
    Drop d;
    d.ms = ms, d.mode = mode;
    d.notice = notice, d.noticeCode = code;
    fill(d, ms, l);
    drop_ = d;
    pending_ = true;
    ready_ = !notice;
}

bool Trail::dropPending(uint32_t ms) const { return pending_ && (ready_ || ms - drop_.ms >= kNoticeJoinMs); }

Drop Trail::takeDrop() {
    pending_ = ready_ = false;
    return drop_;
}

std::string Trail::summary(const Drop& d, const std::string& map) const {
    std::string s = "Розрив: ";
    const std::string who = roleUk(d.role);
    if (d.notice) {
        s += who + " надіслав повідомлення про від'єднання, код " + std::to_string(d.noticeCode) + " (" +
             noticeUk(d.noticeCode) + ")";
        if (d.closedByServer) s += "; після нього сервер закрив з'єднання";
        else if (d.error) s += "; після нього з'єднання обірвалося, помилка " + withName(d.error, errorUk(d.error));
    } else if (d.closedByServer) {
        s += who + " сам закрив з'єднання (читання повернуло 0)";
    } else if (d.error && d.sending) {
        s += who + ": відправлення не вдалося, помилка " + withName(d.error, errorUk(d.error));
    } else if (d.error) {
        s += who + ": читання не вдалося, помилка " + withName(d.error, errorUk(d.error));
    } else {
        s += "з'єднання втрачено, як саме - не записано";
    }
    s += ".";
    if (d.link) {
        s += " З'єднання прожило " + spanUk(d.ageMs) + "; останні дані від сервера за " + spanUk(d.idleInMs) +
             " до розриву, прийнято " + std::to_string(d.in) + " Б, відправлено " + std::to_string(d.out) + " Б.";
        s += " Це " + ordinalUk(d.ofRole) + " з'єднання з таким сервером за запуск гри";
        s += "; до цього повернень до вибору персонажа: " + std::to_string(d.restarts) + ".";
        if (d.hadBefore)
            s += " Попереднє з'єднання (" + std::string(roleUk(d.beforeRole)) + ") " + closerUk(d.beforeBy) + " за " +
                 spanUk(d.sinceBeforeMs) + " до початку цього.";
    }
    if (d.gapMs >= kGapFromMs) s += " Незадовго до розриву гра не опитувала мережу " + spanUk(d.gapMs) + ".";
    if (!d.lastIds.empty()) {
        s += " Останні пакети сервера:";
        for (const uint16_t id : d.lastIds) s += " " + hexId(id);
        s += ".";
    }
    if (!map.empty()) s += " Карта: " + map + ".";
    return s;
}

std::string Trail::text(const Drop& d, const Clock& clock, const std::string& version,
                        const std::vector<std::string>& facts) const {
    std::string t = "uaRO Graphics " + version + " - a connection of the game's lost (core/net_trail.hpp)\n";
    t += "when: " + timeAt(clock, d.ms) + " by this computer's clock, in the game's " +
         (d.mode == Mode::Game ? "game" : "login") + " mode\n";
    t += "what: ";
    if (d.notice) {
        t += "the server's notice 0x0081, code " + withName(d.noticeCode, noticeName(d.noticeCode));
        if (d.closedByServer) t += "; then the server closed the connection";
        else if (d.error) t += "; then the connection failed, error " + withName(d.error, errorName(d.error));
    } else if (d.closedByServer) {
        t += std::string("the ") + roleName(d.role) + " server closed the connection (a read returned 0)";
    } else if (d.error) {
        t += std::string(d.sending ? "a write to" : "a read from") + " the " + roleName(d.role) +
             " server failed: error " + withName(d.error, errorName(d.error));
    } else {
        t += "the connection was lost, how is not in the trail";
    }
    t += "\n";
    if (d.link) {
        t += "its connection: #" + std::to_string(d.link) + " (" + roleName(d.role) + "), " + spanEn(d.ageMs) +
             " old; the server's last bytes " + spanEn(d.idleInMs) + " before, the game's last " +
             spanEn(d.idleOutMs) + " before; in " + std::to_string(d.in) + " B, out " + std::to_string(d.out) + " B\n";
        t += "in this game: connection " + std::to_string(d.ofRole) + " to a server of this kind; back to the " +
             "characters " + std::to_string(d.restarts) + " time(s) before\n";
        if (d.hadBefore)
            t += std::string("before it: a connection to the ") + roleName(d.beforeRole) + " server, " +
                 closerEn(d.beforeBy) + " " + spanEn(d.sinceBeforeMs) + " before this one's start\n";
    }
    if (d.gapMs >= kGapFromMs)
        t += "the game's loop: its longest pause in the half minute before " + spanEn(d.gapMs) + "\n";
    if (!d.lastIds.empty()) {
        t += "the server's last packets:";
        for (const uint16_t id : d.lastIds) t += " " + hexId(id);
        t += "\n";
    }
    for (const std::string& f : facts) t += f + "\n";
    t += "\nthe game's connections, the oldest first (" + std::to_string(links_.size()) + " kept):\n";
    for (const Link& l : links_) {
        t += "  #" + std::to_string(l.number) + " " + roleName(l.role) + " " + ipText(l.ip) + ":" +
             std::to_string(l.port) + " socket " + std::to_string(l.socket) + " - " + timeAt(clock, l.openedMs);
        if (!l.started) t += ", the connect did not start";
        else if (l.open) t += ", open";
        else t += " to " + timeAt(clock, l.closedMs) + " (" + spanEn(l.closedMs - l.openedMs) + "), " +
                  closerEn(l.closedBy);
        t += "; in " + std::to_string(l.in) + " B, out " + std::to_string(l.out) + " B, " +
             std::to_string(l.packets) + " packets\n";
    }
    t += "\nthe trail, the oldest first (" + std::to_string(events_.size()) + " events kept):\n";
    for (const Event& e : events_) {
        t += "  " + timeAt(clock, e.ms) + " #" + std::to_string(e.link) + " ";
        const Link* l = nullptr;
        for (const Link& k : links_)
            if (k.number == e.link) l = &k;
        switch (e.what) {
            case What::Connect:
            case What::ConnectFailed:
                t += e.what == What::Connect ? "connect" : "the connect did not start";
                if (l) t += std::string(" to the ") + roleName(l->role) + " server " + ipText(l->ip) + ":" +
                            std::to_string(l->port);
                t += " (socket " + std::to_string(e.a) + ")";
                if (e.what == What::ConnectFailed) t += ", error " + withName(int(e.b), errorName(int(e.b)));
                break;
            case What::Close: t += closerEn(Closer(e.a)); break;
            case What::RecvEnd:
                t += e.a ? "the read failed: error " + withName(int(e.a), errorName(int(e.a)))
                         : std::string("the server closed the connection (the read returned 0)");
                break;
            case What::SendFail: t += "the write failed: error " + withName(int(e.a), errorName(int(e.a))); break;
            case What::Drop:
                t += std::string("the game's loop found the connection lost (") +
                     (Mode(e.a) == Mode::Game ? "game" : "login") + " mode)";
                break;
            case What::Notice:
                t += "the server's notice 0x0081, code " + withName(int(e.a), noticeName(int(e.a)));
                break;
            case What::Mark:
                t += "packet " + hexId(uint16_t(e.a)) + " (" + packetName(uint16_t(e.a)) + "), its first value " +
                     std::to_string(e.b);
                break;
        }
        t += "\n";
    }
    return t;
}

}  // namespace uagfx::nettrail
