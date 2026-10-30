// 3.0.11-beta.61 (the autopilot): the first game's outcome on this install - sent once with the anonymous install ping,
// so that a new player's first start is seen: did it get to a map, at what frames, did it close by an error
#include <string>

#include "core/first_game.hpp"
#include "core/log_files.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(first_game_round_trip) {
    firstgame::Outcome o;
    o.version = "3.0.11-beta.61";
    o.cls = "L1 d3d11";
    o.safe = o.map = true;
    o.fps = 58;
    o.level = 1;
    o.end = "fine";
    o.minutes = 12;
    firstgame::Outcome back;
    CHECK(firstgame::parse(firstgame::format(o), back));
    CHECK(back.version == o.version && back.cls == o.cls && back.safe && back.map);
    CHECK(back.fps == 58 && back.level == 1 && back.end == "fine" && back.minutes == 12);
    CHECK(firstgame::complete(back));
    firstgame::Outcome started;  // as the driver writes it at the start: the version and the class
    started.version = "3.0.11-beta.61";
    started.cls = "L3 d3d12";
    CHECK(firstgame::parse(firstgame::format(started), back));
    CHECK(!back.map && back.fps == -1 && back.level == -1 && back.end.empty() && back.minutes == -1);
    CHECK(!firstgame::complete(back));
    CHECK(!firstgame::parse("class=L1 d3d11\n", back));  // no version
    CHECK(!firstgame::parse("", back));
}

TEST(first_game_end_and_minutes_of_a_log) {
    const std::string start = "10:00:00.000 INFO  uaRO Graphics 3.0.11\n10:00:01.000 INFO  (uaRO renderer)\n";
    const std::string closed = "10:12:30.000 INFO  DirectDraw released, renderer closed\n";
    CHECK_EQ(firstgame::endOfLog(start + closed), std::string("fine"));
    CHECK_EQ(firstgame::endOfLog(start + "10:03:00.000 ERROR crash: exception 0xc0000005\n"), std::string("crash"));
    CHECK_EQ(firstgame::endOfLog(""), std::string(""));
    CHECK_EQ(firstgame::minutesOfLog(start + closed), 12);
    CHECK_EQ(firstgame::minutesOfLog("23:50:00.000 INFO  a\n00:20:59.000 INFO  b\n"), 30);  // past midnight
    CHECK_EQ(firstgame::minutesOfLog("no times here\n"), -1);
    CHECK_EQ(logfiles::secondsAt("09:05:07.123 INFO  x\n", 3), 9 * 3600 + 5 * 60 + 7);
    CHECK_EQ(logfiles::secondsAt("a\n09:05:07.123 INFO  x\n", 4), 9 * 3600 + 5 * 60 + 7);
    CHECK_EQ(logfiles::secondsAt("x 09:05:07\n", 2), -1);
}

TEST(first_game_json_clamped) {
    firstgame::Outcome o;
    o.version = "3.0.11-beta.61";
    o.cls = "L2 d3d12\"}";
    o.map = true;
    o.fps = 5000;
    o.level = 2;
    o.end = "crash";
    o.minutes = 123456;
    CHECK_EQ(firstgame::json(o), std::string("{\"class\":\"L2 d3d12\",\"safe\":0,\"map\":1,\"fps\":999,\"level\":2,"
                                             "\"end\":\"crash\",\"minutes\":99999}"));
    firstgame::Outcome none;  // nothing known but the class
    none.version = "x";
    none.cls = "L1 d3d11";
    CHECK_EQ(firstgame::json(none), std::string("{\"class\":\"L1 d3d11\",\"safe\":0,\"map\":0,\"fps\":null,"
                                                "\"level\":null,\"end\":null,\"minutes\":null}"));
}

TEST(first_game_cut_before_a_map) {
    // a crash at the login: no map, its end known - complete, so sent
    const std::string log = "10:00:00.000 INFO  (uaRO renderer)\n10:00:40.000 ERROR crash: exception 0xc0000005\n";
    firstgame::Outcome o;
    o.version = "3.0.11-beta.61";
    o.cls = "L1 d3d11";
    o.end = firstgame::endOfLog(log);
    o.minutes = firstgame::minutesOfLog(log);
    CHECK(firstgame::complete(o) && !o.map && o.end == "crash" && o.minutes == 0);
}
