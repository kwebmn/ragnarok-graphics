// 3.0.22-beta.3 (the owner's «ок» to ideas 1 and 2: «Disconnected from Server.» says what happened; after a drop
// the game logs in again by itself with a countdown and Esc to cancel): a drop's cause for the player and whether it is
// worth logging in again (core/net_reconnect)
#include <string>

#include "core/net_reconnect.hpp"
#include "core/net_trail.hpp"
#include "core/settings/strings.hpp"
#include "test.hpp"

using namespace uagfx;
using reconnect::Cause;

namespace {
nettrail::Drop mapDrop() {
    nettrail::Drop d;
    d.mode = nettrail::Mode::Game;
    d.role = nettrail::Role::Zone;
    d.link = 3;
    d.ageMs = 600000;
    return d;
}
}  // namespace

TEST(net_reconnect_the_drops_cause) {
    nettrail::Drop d = mapDrop();
    d.notice = true;
    d.noticeCode = nettrail::kNoticeOtherLogin;
    CHECK(reconnect::causeOf(d) == Cause::OtherLogin);
    d.noticeCode = 15;
    CHECK(reconnect::causeOf(d) == Cause::Gm);
    d.noticeCode = 8;
    CHECK(reconnect::causeOf(d) == Cause::StillOnline);
    d.noticeCode = 0;
    CHECK(reconnect::causeOf(d) == Cause::ServerNotice);
    d = mapDrop();
    d.closedByServer = true;
    CHECK(reconnect::causeOf(d) == Cause::ServerClosed);
    d = mapDrop();
    d.error = 10054;
    CHECK(reconnect::causeOf(d) == Cause::Network);
    CHECK(reconnect::causeOf(mapDrop()) == Cause::Unknown);
}

TEST(net_reconnect_worth_logging_in_again) {
    nettrail::Drop d = mapDrop();
    d.error = 10054;
    CHECK(reconnect::worthy(d));  // the network: yes
    d = mapDrop();
    d.closedByServer = true;
    CHECK(reconnect::worthy(d));  // closed with no notice (a relay, a router, the server's own close)
    d.notice = true;
    d.noticeCode = nettrail::kNoticeOtherLogin;
    CHECK(!reconnect::worthy(d));  // another device took the account: it would take it back
    d.noticeCode = 15;
    CHECK(!reconnect::worthy(d));  // a game master's
    d = mapDrop();
    d.error = 10054;
    d.role = nettrail::Role::Char;
    CHECK(!reconnect::worthy(d));  // not on a map
    d.role = nettrail::Role::Zone;
    d.mode = nettrail::Mode::Login;
    CHECK(!reconnect::worthy(d));
    d = mapDrop();
    d.error = 10054;
    d.ageMs = 5000;  // dropped within seconds of entering: the same again - not by itself
    CHECK(!reconnect::worthy(d));
}

TEST(net_reconnect_three_tries_in_ten_minutes) {
    reconnect::Tries t;
    CHECK(t.allow(0));
    t.tried(1000);
    t.tried(2000);
    t.tried(3000);
    CHECK(!t.allow(4000));
    CHECK(t.allow(3000 + reconnect::kTriesWindowMs + 1));  // the first one past the window
}

TEST(net_reconnect_its_words) {
    using settings::Lang;
    nettrail::Drop d = mapDrop();
    d.notice = true;
    d.noticeCode = nettrail::kNoticeOtherLogin;
    CHECK_EQ(reconnect::words(d, Lang::Uk),
             std::string("Під цим обліковим записом увійшли з іншого місця — сервер від'єднує того, хто вже грав."));
    d.noticeCode = 3;
    CHECK_EQ(reconnect::words(d, Lang::Uk), std::string("Сервер завершив з'єднання (код 3)."));
    d = mapDrop();
    d.error = 10054;
    CHECK_EQ(reconnect::words(d, Lang::Uk), std::string("Зв'язок із сервером обірвався (мережа, помилка 10054)."));
    CHECK_EQ(reconnect::countdown(7, Lang::Uk), std::string("Повторний вхід через 7 с · Esc — скасувати"));
    CHECK_EQ(reconnect::countdown(7, Lang::En), std::string("Logging in again in 7 s · Esc to cancel"));
}
