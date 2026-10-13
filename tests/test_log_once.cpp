#include "core/log_once.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(logonce_reports_each_failure_once) {
    LogOnce l;
    CHECK(l.first(5, 0x887A0005u));   // Present: DXGI_ERROR_DEVICE_REMOVED
    CHECK(!l.first(5, 0x887A0005u));  // the same every frame after that: once is enough
    CHECK(l.first(5, 0x887A0001u));   // another code at the same place
    CHECK(l.first(3, 0x887A0005u));   // the same code at another place
}
