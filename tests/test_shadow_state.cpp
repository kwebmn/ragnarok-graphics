#include "core/shadow_state.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(shadow_untouched_lock_reads_nothing) {
    ShadowState s;  // the client locks the target every frame and never touches the memory
    s.lock();
    CHECK(s.locked());
    CHECK(!s.unlock());
    CHECK(!s.dirty());
    CHECK(s.stale());  // nothing was read, so the next touch still reads back
}

TEST(shadow_first_read_reads_back_once) {
    ShadowState s;
    s.lock();
    ShadowState::Touch t = s.touch(false);
    CHECK(t.readBack);
    CHECK(!t.writable);
    CHECK(!s.stale());
    t = s.touch(false);
    CHECK(!t.readBack);
    CHECK(!s.unlock());
}

TEST(shadow_write_marks_dirty_and_unlock_asks_for_an_upload) {
    ShadowState s;
    s.lock();
    ShadowState::Touch t = s.touch(true);  // the wave effect, Bink
    CHECK(t.readBack);
    CHECK(t.writable);
    CHECK(s.dirty());
    CHECK(s.unlock());
    CHECK(!s.locked());
    CHECK(!s.stale());  // our own upload: the copy and the GPU target agree
}

TEST(shadow_gpu_change_makes_the_next_lock_read_again) {
    ShadowState s;
    s.lock();
    s.touch(false);
    s.unlock();
    s.lock();
    CHECK(!s.touch(false).readBack);  // nothing drawn in between
    s.unlock();
    s.gpuChanged();
    s.lock();
    CHECK(s.touch(false).readBack);
}

TEST(shadow_read_then_write_needs_one_readback) {
    ShadowState s;
    s.lock();
    CHECK(s.touch(false).readBack);
    const ShadowState::Touch t = s.touch(true);
    CHECK(!t.readBack);
    CHECK(t.writable);
    CHECK(s.unlock());
}
