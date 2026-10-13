#include "core/retry_timer.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(retry_timer_waits_a_pause_after_a_failure) {
    RetryTimer r(1000);
    CHECK(r.mayTry(0));
    r.failed(5000);
    CHECK(!r.mayTry(5000));
    CHECK(!r.mayTry(5999));
    CHECK(r.mayTry(6000));
    r.failed(6000);  // failed again: another pause
    CHECK(!r.mayTry(6500));
    r.succeeded();
    CHECK(r.mayTry(6500));
}
