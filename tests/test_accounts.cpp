// 3.0.11-beta.175 (the login concept's shelf): the player's game accounts kept on this computer (core/accounts) - the
// login, the password (the file DPAPI-protected, win/accounts_store), the label, «Одразу в гру», the last character
#include <string>

#include "core/accounts.hpp"
#include "test.hpp"

using namespace uagfx;

// A login already there (any case): its password and time new, its place kept; the file's words read back whole
TEST(accounts_round_trip_and_upsert) {
    accounts::Shelf s;
    accounts::upsert(s, "kweb", "secret1", 100);
    accounts::upsert(s, "kweb_shop", "secret2", 200);
    accounts::upsert(s, "KWEB", "secret3", 300);
    CHECK(s.list.size() == 2 && s.list[0].password == "secret3" && s.list[0].usedAt == 300);
    CHECK(s.list[0].login == "kweb" && s.list[1].login == "kweb_shop");
    s.list[0].label = "Основний \"рачок\"";
    s.list[0].autoEnter = false;
    s.list[1].keepPassword = false;
    accounts::Shelf back;
    CHECK(accounts::fromJson(accounts::toJson(s), back) && back.list.size() == 2);
    CHECK(back.list[0].label == "Основний \"рачок\"" && !back.list[0].autoEnter && back.addAfterLogin);
    CHECK(back.list[0].password == "secret3" && back.list[0].usedAt == 300);
    CHECK(!back.list[1].keepPassword && back.list[1].password.empty());  // not kept: never written
}

TEST(accounts_bad_input_gives_an_empty_shelf) {
    accounts::Shelf s;
    accounts::upsert(s, "kweb", "x", 1);
    CHECK(!accounts::fromJson("{\"list\":[{\"login\":1}", s) && s.list.empty());
    CHECK(!accounts::fromJson("", s) && s.list.empty());
    CHECK(accounts::fromJson("{\"v\":1,\"list\":[{\"login\":\"\"},{\"login\":\"ok\"}]}", s));
    CHECK(s.list.size() == 1 && s.list[0].login == "ok");  // a row without its login left out
}

// Their order by hand, one taken off, the character the login entered with remembered (its place in the list kept)
TEST(accounts_move_remove_and_last_character) {
    accounts::Shelf s;
    accounts::upsert(s, "a", "1", 1);
    accounts::upsert(s, "b", "2", 2);
    accounts::upsert(s, "c", "3", 3);
    CHECK(accounts::move(s, 0, +1) && s.list[0].login == "b" && s.list[1].login == "a");
    CHECK(!accounts::move(s, 2, +1) && !accounts::move(s, 0, -1) && !accounts::move(s, 7, 1));
    CHECK(accounts::remove(s, "B") && s.list.size() == 2 && !accounts::remove(s, "b"));
    accounts::LastChar c;
    c.name = "Rachok";
    c.job = "Assassin Cross";
    c.jobId = 4013;
    c.level = 99;
    c.slot = 2;
    c.map = "prontera";
    accounts::recordChar(s, "A", c, 50);
    CHECK(s.list[0].login == "a" && s.list[0].last.name == "Rachok" && s.list[0].last.slot == 2);
    CHECK(s.list[0].last.map == "prontera" && s.list[0].usedAt == 50);
    accounts::Shelf back;
    CHECK(accounts::fromJson(accounts::toJson(s), back) && back.list[0].last.jobId == 4013 &&
          back.list[0].last.level == 99);
    accounts::recordChar(s, "nobody", c, 60);  // not on the shelf: nothing
    CHECK(s.list.size() == 2);
}

// A login's own number for the logs and the shared block - the same in any case, never the login's words
TEST(accounts_login_hash) {
    CHECK(accounts::loginHash("KWeb") == accounts::loginHash("kweb"));
    CHECK(accounts::loginHash("kweb") != accounts::loginHash("kweb2"));
    CHECK(accounts::loginHash("") != 0);
}
