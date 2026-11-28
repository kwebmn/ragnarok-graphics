// 3.0.24-beta.57 (docs/superpowers/plans/2026-11-28-full-check-probe-tour.md, task 2): the full check's windows'
// tour - each window opened by the game's own keys, checked, its pad ring stepped, closed by Esc; a stop that opens
// nothing presses no Esc (it would open the game's menu); «here» checks what a scene left open
#include <set>
#include <string>
#include <vector>

#include "core/check_tour.hpp"
#include "core/mcp.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::checktour;

TEST(check_tour_stops_take_keys) {
    CHECK(stops().size() >= 17);
    std::set<std::string> names;
    for (const Stop& s : stops()) {
        mcp::KeyCombo k;
        std::string why;
        CHECK(mcp::keyCombo(s.keys, k, why));
        CHECK(names.insert(s.name).second);
    }
    CHECK(names.count("items") && names.count("settings") && names.count("menu"));
}

TEST(check_tour_appeared) {
    const std::vector<std::string> got = appeared({"a", "b"}, {"b", "c", "a", "d"});
    CHECK_EQ(got.size(), size_t(2));
    CHECK_EQ(got[0], std::string("c"));
    CHECK_EQ(got[1], std::string("d"));
    CHECK(appeared({"a"}, {"a"}).empty());
}

TEST(check_tour_inside) {
    CHECK(inside(0, 0, 100, 100, 800, 600));
    CHECK(inside(700, 500, 100, 100, 800, 600));
    CHECK(!inside(750, 0, 100, 100, 800, 600));
    CHECK(!inside(-1, 0, 10, 10, 800, 600));
    CHECK(!inside(0, 0, 0, 10, 800, 600));
}

namespace {

Seen seen(uint64_t ms, std::vector<std::string> docs, int padFocus = -2) {
    Seen s;
    s.ms = ms, s.docs = std::move(docs), s.padFocus = padFocus;
    return s;
}

}  // namespace

TEST(check_tour_steps_a_window) {
    Job j;
    j.only = {"items"};
    CHECK(step(j, seen(1000, {"hotbar"})) == Do::Keys);
    CHECK(step(j, seen(1016, {"hotbar"})) == Do::Nothing);
    CHECK(step(j, seen(1032, {"hotbar", "items"})) == Do::Check);
    CHECK_EQ(j.row.opened.size(), size_t(1));
    CHECK(step(j, seen(1048, {"hotbar", "items"}, -1)) == Do::PadStep);
    CHECK(j.row.padRing);
    CHECK(step(j, seen(1064, {"hotbar", "items"}, 0)) == Do::Esc);
    CHECK(j.row.padMoves);
    CHECK(step(j, seen(1080, {"hotbar", "items"}, 0)) == Do::Nothing);
    CHECK(step(j, seen(1096, {"hotbar"})) == Do::Record);
    CHECK(j.row.closed);
    CHECK_EQ(j.row.stop, std::string("items"));
    CHECK(step(j, seen(1112, {"hotbar"})) == Do::Finish);
    const json::Value r = rowJson(j.row);
    CHECK_EQ(r["check"].asString(), std::string("window"));
    CHECK_EQ(r["pad"].asString(), std::string("moves"));
}

TEST(check_tour_nothing_opened_presses_no_esc) {
    Job j;
    j.only = {"pet"};
    CHECK(step(j, seen(1000, {"hotbar"})) == Do::Keys);
    Do last = Do::Nothing;
    for (uint64_t ms = 1016; ms < 3200; ms += 16) {
        last = step(j, seen(ms, {"hotbar"}));
        CHECK(last != Do::Esc && last != Do::Check);
        if (last == Do::Record) break;
    }
    CHECK(last == Do::Record);
    CHECK_EQ(j.row.error, std::string("nothing opened"));
    CHECK(!j.row.closed);
}

TEST(check_tour_esc_that_does_not_close) {
    Job j;
    j.only = {"skills"};
    step(j, seen(1000, {}));
    CHECK(step(j, seen(1016, {"skills"})) == Do::Check);
    CHECK(step(j, seen(1032, {"skills"})) == Do::Esc);  // no pad ring: straight to Esc
    CHECK(!j.row.padRing);
    CHECK(step(j, seen(2000, {"skills"})) == Do::Nothing);
    CHECK(step(j, seen(3100, {"skills"})) == Do::Record);
    CHECK(!j.row.closed);
    CHECK_EQ(j.row.error, std::string("Esc did not close it"));
}

TEST(check_tour_here_checks_what_is_open) {
    Job j;
    j.here = true;
    CHECK(step(j, seen(1000, {"npc", "shop"})) == Do::Check);
    CHECK_EQ(j.row.opened.size(), size_t(2));
    CHECK(step(j, seen(1016, {"npc", "shop"}, 3)) == Do::PadStep);
    CHECK(step(j, seen(1032, {"npc", "shop"}, 3)) == Do::Record);  // no Esc: the scene's helper closes them
    CHECK(!j.row.padMoves);
    CHECK(step(j, seen(1048, {"npc", "shop"})) == Do::Finish);
    Job none;
    none.here = true;
    CHECK(step(none, seen(1000, {})) == Do::Record);
    CHECK_EQ(none.row.error, std::string("nothing open"));
}

TEST(check_tour_status_and_only) {
    Job j;
    j.only = {"items", "no such stop", "skills"};
    step(j, seen(1000, {}));
    const json::Value s = status(j);
    CHECK_EQ(s["of"].asInt(), int64_t(2));  // the unknown name left out
    CHECK_EQ(s["stop"].asString(), std::string("items"));
}
