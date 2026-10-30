// 3.0.11-beta.56: the anonymous mark of an install (core/install_ping) - its number, when it goes, what it says
#include <string>

#include "core/install_ping.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(install_ping) {
    CHECK(ping::due(1000, 0) && !ping::due(1000 + 3600, 1000) && ping::due(1000 + 12 * 3600, 1000));
    CHECK(ping::due(500, 1000));  // the clock went back: told again
    const std::string a = ping::newId(1), b = ping::newId(2);
    CHECK(ping::validId(a) && ping::validId(b) && a != b && a == ping::newId(1));
    CHECK(!ping::validId("0123") && !ping::validId("0123456789ABCDEF") && !ping::validId("0123456789abcdeg"));
    CHECK(ping::idOf("0123456789abcdef\r\n") == "0123456789abcdef" && ping::idOf("junk").empty());
    CHECK(ping::idOf("").empty() && ping::idOf("0123456789abcdef  ") == "0123456789abcdef");
    CHECK(ping::body("0123456789abcdef", "3.0.11-beta.56", "beta") ==
          "{\"id\":\"0123456789abcdef\",\"version\":\"3.0.11-beta.56\",\"channel\":\"beta\"}");
    CHECK(ping::body("0123456789abcdef", "3.0.10\"}", "other") ==
          "{\"id\":\"0123456789abcdef\",\"version\":\"3.0.10\",\"channel\":\"release\"}");  // nothing but its words
    CHECK(ping::body("bad\"", "1", "release").find("\"id\":\"\"") != std::string::npos);
    // 3.0.11-beta.61 (the autopilot): the first game's outcome rides along once (core/first_game's object)
    CHECK(ping::body("0123456789abcdef", "3.0.11-beta.61", "beta", "{\"class\":\"L1 d3d11\"}") ==
          "{\"id\":\"0123456789abcdef\",\"version\":\"3.0.11-beta.61\",\"channel\":\"beta\",\"first\":"
          "{\"class\":\"L1 d3d11\"}}");
    CHECK(ping::body("0123456789abcdef", "3.0.11-beta.61", "beta", "") ==
          ping::body("0123456789abcdef", "3.0.11-beta.61", "beta"));
    CHECK(ping::body("0123456789abcdef", "1", "beta", "junk").find("first") == std::string::npos);  // no object
}
