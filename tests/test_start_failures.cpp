// 3.0.17-beta.1 (the concept «простой старт»: the rollback offered after an update that does not work): the failed
// starts of the installed version counted in uaro-graphics\start-failures.txt
#include "core/start_failures.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(start_failures_offer_after_two_of_one_version) {
    std::string t;
    t = startfail::note(t, "3.0.17", true);
    CHECK(!startfail::offerRollback(t, "3.0.17"));
    t = startfail::note(t, "3.0.17", true);
    CHECK(startfail::offerRollback(t, "3.0.17"));
    CHECK(!startfail::offerRollback(t, "3.0.16"));        // another version: not its count
    CHECK(!startfail::offerRollback(startfail::note(t, "3.0.17", false), "3.0.17"));  // a good start forgets them
    CHECK(!startfail::offerRollback(startfail::note(t, "3.0.16", true), "3.0.16"));   // a new version starts at one
    CHECK(!startfail::offerRollback("garbage words", "3.0.17"));
}

TEST(start_failures_what_counts) {
    CHECK(startfail::failed("start-failed", 30));
    CHECK(startfail::failed("crash", 1));    // a crash in its first minutes
    CHECK(!startfail::failed("crash", 45));  // after a long game: not the update's
    CHECK(!startfail::failed("", 0));
    CHECK(!startfail::failed("gpu-lost", 1));
}
