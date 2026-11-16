// 3.0.20-beta.60 (the owner: «есть какой-то глюк, в котором клиент выбрасывает с дисконнектом, надо его отловить»;
// the server's keeper: «раньше это происходило при перелогине/смене чара ... после первого входа выкинуло или после
// перезахода? и сколько времени прошло после выхода и повторного входа»): the trail of the game's connections tells a
// drop's story - which server, how it ended, how long it lived, what was closed before it and by whom
#include <string>
#include <vector>

#include "core/net_trail.hpp"
#include "test.hpp"

using namespace uagfx;
using nettrail::Closer;
using nettrail::Mode;
using nettrail::Role;

namespace {

constexpr uint32_t kIp = 0x0100007Fu;  // 127.0.0.1

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

// a login as the game makes it: the account server asked and left, the character server, then the map's
void login(nettrail::Trail& t, uint32_t at, uint32_t firstSocket) {
    t.connected(at, firstSocket, kIp, 6900, Role::Account, true, 0);
    t.received(at + 100, firstSocket, 79, 0);
    const uint8_t list[4] = {0x69, 0x00, 79, 0};
    t.packet(at + 100, Mode::Login, 0x0069, list, sizeof list);
    t.closed(at + 120, firstSocket, Closer::Game);
    t.connected(at + 130, firstSocket + 4, kIp, 6121, Role::Char, true, 0);
    t.received(at + 300, firstSocket + 4, 500, 0);
    const uint8_t zone[4] = {0x71, 0x00, 1, 0};
    t.packet(at + 900, Mode::Login, 0x0071, zone, sizeof zone);
    t.closed(at + 910, firstSocket + 4, Closer::Game);
    t.connected(at + 920, firstSocket + 8, kIp, 5121, Role::Zone, true, 0);
}

}  // namespace

TEST(trail_the_server_closing_after_a_relogin_tells_the_whole_story) {
    nettrail::Trail t;
    login(t, 1000, 100);
    t.received(2000, 108, 11, 0);
    const uint8_t entered[4] = {0x73, 0x00, 1, 0};
    t.packet(2000, Mode::Game, 0x0073, entered, sizeof entered);
    for (uint32_t ms = 2100; ms < 60000; ms += 100) t.polled(ms, Mode::Game, true);
    // back to the characters: the server's leave, the game shuts the map's connection itself
    const uint8_t restart[3] = {0xB3, 0x00, 1};
    t.packet(60000, Mode::Game, 0x00B3, restart, sizeof restart);
    t.closed(60010, 108, Closer::Game);
    CHECK(!t.dropPending(60010));
    t.connected(60020, 112, kIp, 6121, Role::Char, true, 0);
    t.received(60200, 112, 500, 0);
    t.closed(65700, 112, Closer::Game);
    t.connected(65710, 116, kIp, 5121, Role::Zone, true, 0);
    t.sent(65720, 116, 19, 0);
    t.received(66000, 116, 11, 0);
    t.packet(66000, Mode::Game, 0x0073, entered, sizeof entered);
    t.polled(66100, Mode::Game, true);
    // 800 ms after the second entry the server closes it: the read returns 0, the game's connection shuts the socket
    t.received(66510, 116, 0, 0);
    t.closed(66510, 116, Closer::AfterRecv);
    t.polled(66510, Mode::Game, false);
    CHECK(t.dropPending(66510));
    const nettrail::Drop d = t.takeDrop();
    CHECK(!t.dropPending(66510));
    CHECK_EQ(int(d.role), int(Role::Zone));
    CHECK_EQ(int(d.mode), int(Mode::Game));
    CHECK(d.closedByServer);
    CHECK(!d.notice);
    CHECK_EQ(d.error, 0);
    CHECK_EQ(d.ageMs, 800u);
    CHECK_EQ(d.idleInMs, 510u);
    CHECK_EQ(d.ofRole, 2);
    CHECK_EQ(d.restarts, 1);
    CHECK(d.hadBefore);
    CHECK_EQ(int(d.beforeRole), int(Role::Char));
    CHECK_EQ(int(d.beforeBy), int(Closer::Game));
    CHECK_EQ(d.sinceBeforeMs, 10u);
    CHECK_EQ(d.in, uint64_t(11));
    CHECK_EQ(d.out, uint64_t(19));
    CHECK_EQ(d.lastIds.size(), size_t(1));
    CHECK_EQ(int(d.lastIds[0]), 0x0073);
    const std::string said = t.summary(d, "prontera");
    CHECK(has(said, "сервер карти"));
    CHECK(has(said, "закрив з'єднання"));
    CHECK(has(said, "2-ге"));
    CHECK(has(said, "prontera"));
    CHECK(has(said, "повернень до вибору персонажа: 1"));
}

TEST(trail_the_servers_notice_is_a_drop_with_its_code) {
    nettrail::Trail t;
    login(t, 0, 100);
    t.received(1000, 108, 3, 0);
    const uint8_t notice[3] = {0x81, 0x00, 8};
    t.packet(1000, Mode::Game, 0x0081, notice, sizeof notice);
    CHECK(!t.dropPending(1000));  // its connection's end may follow at once: one drop of the two
    t.closed(1005, 108, Closer::Game);
    t.polled(1100, Mode::Game, true);
    CHECK(!t.dropPending(1100));
    CHECK(t.dropPending(1000 + nettrail::kNoticeJoinMs));
    const nettrail::Drop d = t.takeDrop();
    CHECK(d.notice);
    CHECK_EQ(d.noticeCode, 8);
    CHECK_EQ(int(d.role), int(Role::Zone));
    CHECK(has(t.summary(d, ""), "код 8"));
    CHECK(has(t.summary(d, ""), "пам'ятає попередній вхід"));
}

TEST(trail_a_notice_and_the_end_after_it_are_one_drop) {
    nettrail::Trail t;
    login(t, 0, 100);
    const uint8_t notice[3] = {0x81, 0x00, 2};
    t.packet(1000, Mode::Game, 0x0081, notice, sizeof notice);
    t.received(1200, 108, 0, 0);
    t.closed(1200, 108, Closer::AfterRecv);
    t.polled(1200, Mode::Game, false);
    CHECK(t.dropPending(1200));
    const nettrail::Drop d = t.takeDrop();
    CHECK(d.notice);
    CHECK_EQ(d.noticeCode, 2);
    CHECK(d.closedByServer);
    CHECK(!t.dropPending(1200 + nettrail::kNoticeJoinMs));
}

TEST(trail_the_players_own_leaving_is_no_drop) {
    nettrail::Trail t;
    login(t, 0, 100);
    const uint8_t quit[4] = {0x8B, 0x01, 0, 0};
    t.packet(5000, Mode::Game, 0x018B, quit, sizeof quit);
    t.closed(5010, 108, Closer::Game);
    t.polled(5100, Mode::Game, true);
    CHECK(!t.dropPending(5100 + nettrail::kNoticeJoinMs));
}

TEST(trail_a_read_that_would_block_ends_nothing) {
    nettrail::Trail t;
    login(t, 0, 100);
    t.received(1000, 108, -1, nettrail::kWouldBlock);
    t.received(1010, 108, -1, nettrail::kNotConnected);
    t.sent(1020, 108, -1, nettrail::kWouldBlock);
    t.polled(1030, Mode::Game, true);
    CHECK(!t.dropPending(1030));
    CHECK(!t.links().back().ended);
    CHECK(!t.links().back().sendFailed);
}

TEST(trail_a_reset_names_its_error) {
    nettrail::Trail t;
    login(t, 0, 100);
    t.received(1000, 108, 50, 0);
    t.received(9000, 108, -1, 10054);
    t.closed(9000, 108, Closer::AfterRecv);
    t.polled(9000, Mode::Game, false);
    const nettrail::Drop d = t.takeDrop();
    CHECK(!d.closedByServer);
    CHECK_EQ(d.error, 10054);
    CHECK(!d.sending);
    CHECK_EQ(d.idleInMs, 8000u);
    CHECK(has(t.summary(d, "payon"), "10054"));
    CHECK(has(t.summary(d, "payon"), "скинуто"));
}

TEST(trail_a_write_that_fails_drops_too) {
    nettrail::Trail t;
    login(t, 0, 100);
    t.sent(4000, 108, -1, 10053);
    t.closed(4000, 108, Closer::AfterSend);
    t.polled(4000, Mode::Game, false);
    const nettrail::Drop d = t.takeDrop();
    CHECK(d.sending);
    CHECK_EQ(d.error, 10053);
    CHECK_EQ(d.ofRole, 1);
    CHECK_EQ(d.restarts, 0);
}

TEST(trail_a_drop_in_the_login_mode_is_the_character_servers) {
    nettrail::Trail t;
    t.connected(0, 100, kIp, 6900, Role::Account, true, 0);
    t.closed(100, 100, Closer::Game);
    t.connected(110, 104, kIp, 6121, Role::Char, true, 0);
    t.received(400, 104, 0, 0);
    t.closed(400, 104, Closer::AfterRecv);
    t.polled(400, Mode::Login, false);
    const nettrail::Drop d = t.takeDrop();
    CHECK_EQ(int(d.role), int(Role::Char));
    CHECK_EQ(int(d.mode), int(Mode::Login));
    CHECK(d.hadBefore);
    CHECK_EQ(int(d.beforeRole), int(Role::Account));
    CHECK(has(t.summary(d, ""), "сервер персонажів"));
}

TEST(trail_a_connect_that_does_not_start_is_no_drop) {
    nettrail::Trail t;
    t.connected(0, 100, kIp, 6900, Role::Account, false, 10061);
    t.closed(0, 100, Closer::AfterConnect);
    t.polled(10, Mode::Login, true);
    CHECK(!t.dropPending(10 + nettrail::kNoticeJoinMs));
    CHECK_EQ(int(t.events().front().what), int(nettrail::What::ConnectFailed));
}

TEST(trail_the_loops_long_pause_before_a_drop_is_told) {
    nettrail::Trail t;
    login(t, 0, 100);
    for (uint32_t ms = 1000; ms <= 3000; ms += 100) t.polled(ms, Mode::Game, true);
    t.polled(9500, Mode::Game, true);  // the game stood for 6.5 s
    t.polled(9600, Mode::Game, true);
    t.received(9700, 108, 0, 0);
    t.closed(9700, 108, Closer::AfterRecv);
    t.polled(9700, Mode::Game, false);
    const nettrail::Drop d = t.takeDrop();
    CHECK_EQ(d.gapMs, 6500u);
    CHECK(has(t.summary(d, ""), "не опитувала мережу"));
    // a pause long ago is not this drop's
    nettrail::Trail u;
    login(u, 0, 100);
    u.polled(1000, Mode::Game, true);
    u.polled(8000, Mode::Game, true);
    for (uint32_t ms = 8100; ms <= 60000; ms += 100) u.polled(ms, Mode::Game, true);
    u.received(60100, 108, 0, 0);
    u.closed(60100, 108, Closer::AfterRecv);
    u.polled(60100, Mode::Game, false);
    CHECK_EQ(u.takeDrop().gapMs, 0u);
}

TEST(trail_keeps_its_newest_events_and_connections) {
    nettrail::Trail t;
    for (uint32_t k = 0; k < 300; ++k) {
        t.connected(k * 10, 100 + k, kIp, 5121, Role::Zone, true, 0);
        t.closed(k * 10 + 5, 100 + k, Closer::Game);
    }
    CHECK(t.events().size() <= nettrail::kMostEvents);
    CHECK(t.links().size() <= nettrail::kMostLinks);
    CHECK_EQ(t.links().back().number, 300);
    CHECK_EQ(int(t.events().back().what), int(nettrail::What::Close));
}

TEST(trail_its_text_lists_the_connections_and_the_events_by_the_clock) {
    nettrail::Trail t;
    login(t, 1000, 100);
    t.received(2000, 108, 11, 0);
    t.received(4000, 108, -1, 10060);
    t.closed(4000, 108, Closer::AfterRecv);
    t.polled(4000, Mode::Game, false);
    const nettrail::Drop d = t.takeDrop();
    nettrail::Clock clock;
    clock.ms = 4000, clock.hour = 12, clock.minute = 0, clock.second = 4, clock.milli = 0;
    const std::string text = t.text(d, clock, "3.0.20-beta.60", {"map: prontera at 150, 180"});
    CHECK(has(text, "uaRO Graphics 3.0.20-beta.60"));
    CHECK(has(text, "map: prontera at 150, 180"));
    CHECK(has(text, "#1 account 127.0.0.1:6900"));
    CHECK(has(text, "#3 map 127.0.0.1:5121"));
    CHECK(has(text, "12:00:01.000 #1 connect"));  // 3 s before the drop's 12:00:04
    CHECK(has(text, "12:00:04.000 #3 the read failed: error 10060"));
    CHECK(has(text, "closed by the game"));
    CHECK(has(text, "0x0069"));
}

TEST(trail_the_names_it_tells) {
    CHECK_EQ(std::string(nettrail::roleName(Role::Zone)), std::string("map"));
    CHECK_EQ(std::string(nettrail::roleName(Role::Char)), std::string("character"));
    CHECK(std::string(nettrail::errorName(10054)).find("reset") != std::string::npos);
    CHECK_EQ(std::string(nettrail::errorName(1)), std::string(""));
    CHECK(std::string(nettrail::noticeName(2)).find("another") != std::string::npos);
    CHECK_EQ(nettrail::ipText(kIp), std::string("127.0.0.1"));
    CHECK(std::string(nettrail::packetName(0x00B3)).size() > 0);
}

TEST(trail_the_apps_count_lets_one_go_ten_minutes_apart_six_a_day) {
    std::string next;
    CHECK(nettrail::maySend("", 100000, next));
    CHECK_EQ(next, std::string("100000\n"));
    std::string after;
    CHECK(!nettrail::maySend(next, 100000 + 599, after));
    CHECK_EQ(after, next);  // nothing added
    CHECK(nettrail::maySend(next, 100000 + 600, after));
    CHECK_EQ(after, std::string("100000\n100600\n"));
    // six in a day at most; a day later they are forgotten
    std::string day;
    for (int k = 0; k < 6; ++k) day += std::to_string(200000 + k * 1000) + "\n";
    CHECK(!nettrail::maySend(day, 200000 + 5 * 1000 + 3600, after));
    CHECK(nettrail::maySend(day, 200000 + 86400 + 1, after));
    CHECK(nettrail::maySend("rubbish\n\n", 5000, after));  // a broken file counts nothing
    CHECK_EQ(after, std::string("5000\n"));
}

TEST(trail_the_files_first_line_is_the_reports_words) {
    CHECK_EQ(nettrail::firstLine("Розрив: сервер карти.\r\n\nuaRO Graphics"), std::string("Розрив: сервер карти."));
    CHECK_EQ(nettrail::firstLine("one line"), std::string("one line"));
    CHECK_EQ(nettrail::firstLine(""), std::string(""));
}
