// 2.8: the logs keep the previous game's and start a new part at 32 MiB (core/log_files, win/log.cpp).
#include <string>

#include "core/log_files.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(log_files_previous_and_part_names) {
    const std::string log = "C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics.log";
    CHECK_EQ(logfiles::previous(log), std::string("C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics.prev.log"));
    CHECK_EQ(logfiles::part(log), std::string("C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics.1.log"));
    CHECK_EQ(logfiles::previous("C:\\a"), std::string("C:\\a.prev"));
    CHECK_EQ(logfiles::folderOf(log), std::string("C:\\uaRO\\uaro-graphics\\logs"));
    CHECK_EQ(logfiles::folderOf("uaro.log"), std::string());
    CHECK(logfiles::kCap == 32ull << 20);
}

// 3.0.3-beta.1: a second game window at once writes its own logs - the first free of a.log, a-2.log ... a-8.log
TEST(log_files_slot_names) {
    const std::string log = "C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics.log";
    CHECK_EQ(logfiles::slotted(log, 1), log);
    CHECK_EQ(logfiles::slotted(log, 0), log);
    CHECK_EQ(logfiles::slotted(log, 2), std::string("C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics-2.log"));
    CHECK_EQ(logfiles::previous(logfiles::slotted(log, 3)),
             std::string("C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics-3.prev.log"));
    CHECK_EQ(logfiles::part(logfiles::slotted(log, 8)),
             std::string("C:\\uaRO\\uaro-graphics\\logs\\uaro-graphics-8.1.log"));
    CHECK_EQ(logfiles::slotted("C:\\a.b\\pad", 2), std::string("C:\\a.b\\pad-2"));  // no ".log": at the end
    CHECK(logfiles::kSlots == 8);
}

// 3.0.16-beta.15 (R-TZUA5M): the watched game's log after it ends - another game started meanwhile moved it to .prev
TEST(log_files_the_watched_games_log) {
    const std::string a = "12:25:27.816 INFO  uaRO Graphics 3.0.16-beta.14 in D:\\uaRO.exe\r\nmore\r\n";
    const std::string b = "12:26:55.440 INFO  uaRO Graphics 3.0.16-beta.14 in D:\\uaRO.exe\r\nrenderer open\r\n";
    CHECK(logfiles::firstLine(a) == "12:25:27.816 INFO  uaRO Graphics 3.0.16-beta.14 in D:\\uaRO.exe");
    const std::string start = logfiles::firstLine(a);
    CHECK(logfiles::whichLog(start, a, "", "") == logfiles::Which::Now);
    CHECK(logfiles::whichLog(start, b, "", a) == logfiles::Which::Previous);  // started again: its log is .prev
    CHECK(logfiles::whichLog(start, "later lines\r\n", a, "") == logfiles::Which::Now);  // a long one's start aside
    CHECK(logfiles::whichLog(start, b, "", b) == logfiles::Which::None);  // gone: nothing sent
    CHECK(logfiles::whichLog("", b, "", "") == logfiles::Which::Now);
}
