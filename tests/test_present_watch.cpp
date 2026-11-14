// 3.0.19-beta.8 (R-T8FGMM): the slow presents of a laptop whose AMD card drew while its screen hung off its Intel
#include "core/present_watch.hpp"
#include "test.hpp"

using namespace uagfx;
using presentwatch::Step;

namespace {
presentwatch::Sample laptop() {  // the report's: 201 ms in the present, the card 11.5 ms
    presentwatch::Sample s;
    s.presentMs = 201.0;
    s.gpuMs = 11.5;
    s.cpuMs = 15.0;
    return s;
}
}  // namespace

TEST(present_watch_ten_seconds_of_slow_presents_turn_direct3d_12_to_11) {
    presentwatch::Watch w;
    for (int i = 1; i < presentwatch::kRun; ++i) CHECK(w.feed(laptop(), true, "") == Step::None);
    CHECK(w.feed(laptop(), true, "Intel(R) HD Graphics 520") == Step::ToD3d11);
    CHECK(w.taken());
    CHECK(w.feed(laptop(), true, "") == Step::None);  // once a run
}

TEST(present_watch_on_direct3d_11_the_screens_card_or_nothing) {
    presentwatch::Watch a;
    Step got = Step::None;
    for (int i = 0; i < presentwatch::kRun; ++i) got = a.feed(laptop(), false, "Intel(R) HD Graphics 520");
    CHECK(got == Step::ToScreenCard);
    presentwatch::Watch b;  // the screen on the card drawing (or not known): nothing to turn to
    for (int i = 0; i < presentwatch::kRun; ++i) got = b.feed(laptop(), false, "");
    CHECK(got == Step::None && b.taken());
}

TEST(present_watch_a_fast_half_second_or_a_busy_card_is_no_fault) {
    presentwatch::Watch w;
    presentwatch::Sample fast = laptop();
    fast.presentMs = 16.0;
    for (int i = 1; i < presentwatch::kRun; ++i) w.feed(laptop(), true, "");
    CHECK(w.feed(fast, true, "") == Step::None && w.run() == 0);  // the run starts again
    presentwatch::Sample busy = laptop();
    busy.gpuMs = 180.0;  // the card itself that slow: the present waits for it
    CHECK(!presentwatch::slow(busy));
    presentwatch::Sample unknown = laptop();
    unknown.gpuMs = -1.0;  // the card's work not measured: the present alone tells
    CHECK(presentwatch::slow(unknown));
}
