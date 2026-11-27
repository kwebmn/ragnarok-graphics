// 3.0.24-beta.36 (the laptop's run, AMD Radeon 610M): the game gave the 64-bit renderer 90 s in all for its start; it
// was still compiling its shaders, telling each one done, when it was ended - the game waits while the renderer is
// heard from: 90 s of silence at most (the old whole wait), 5 minutes in all
#include <cstdint>

#include "core/start_wait.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(start_wait_while_heard_from) {
    startwait::Wait w(1000);
    CHECK_EQ(w.left(1000), uint32_t(90000));  // nothing heard: as before, 90 s
    CHECK_EQ(w.left(90000), uint32_t(1000));
    CHECK_EQ(w.left(91000), uint32_t(0));
    CHECK_EQ(w.left(200000), uint32_t(0));
    w.heard(80000);  // its progress: 90 s more from then
    CHECK_EQ(w.left(91000), uint32_t(79000));
    CHECK_EQ(w.left(170000), uint32_t(0));
    w.heard(290000);  // heard on, but 5 minutes in all
    CHECK_EQ(w.left(295000), uint32_t(6000));
    CHECK_EQ(w.left(301000), uint32_t(0));
    CHECK(startwait::kQuietMs == 90000 && startwait::kMostMs == 300000);
    // a clock that went back (it does not): no wait past the whole
    startwait::Wait back(5000);
    CHECK(back.left(4000) <= startwait::kMostMs);
}
