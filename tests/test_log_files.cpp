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
