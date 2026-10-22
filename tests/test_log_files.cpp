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
