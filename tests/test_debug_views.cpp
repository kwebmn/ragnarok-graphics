// 3.0.23-beta.5 (a tester's coloured strips on prt_fild06 for days: [debug] seams_view 2 left on since 18.11, R-FPT4TT
// «чому з'явились артифакти»): a debug view on is told on the screen at each map, with where to turn it off
#include "core/config.hpp"
#include "core/debug_views.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(debug_views_on_are_told) {
    Config c;
    CHECK(debugviews::note(c, settings::Lang::Uk).empty());  // none on: nothing said
    c.seamsView = 2;
    const std::string one = debugviews::note(c, settings::Lang::Uk);
    CHECK(one.find("Стики землі: перегляд") != std::string::npos);
    CHECK(one.find("Ctrl+~") != std::string::npos);
    c.xray = true;
    const std::string two = debugviews::note(c, settings::Lang::En);
    CHECK(two.find("Ground seams view") != std::string::npos && two.find("X-ray") != std::string::npos);
}
