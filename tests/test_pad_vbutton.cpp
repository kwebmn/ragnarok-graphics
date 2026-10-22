#include "pad/core/logic/vbutton.hpp"
#include "test.hpp"

using namespace uapad;

TEST(vbutton_edges_are_queued_once) {
    VirtualButton b;
    CHECK(!b.set(false, 10));  // already up: no edge
    CHECK(b.set(true, 20));    // press
    CHECK(!b.set(true, 30));   // still held
    CHECK(b.set(false, 40));   // release
    CHECK_EQ(b.pending(), 2u);
    CHECK(!b.reported());
}

TEST(vbutton_take_peek_and_room) {
    VirtualButton b;
    b.set(true, 1);
    b.set(false, 2);
    b.set(true, 3);
    auto peeked = b.take(10, true);  // DIGDD_PEEK leaves the events queued
    CHECK_EQ(peeked.size(), 3u);
    CHECK_EQ(b.pending(), 3u);
    auto first = b.take(2, false);  // only two fit into the client's buffer
    CHECK(first.size() == 2 && first[0].down && !first[1].down && first[1].timeMs == 2u);
    CHECK_EQ(b.pending(), 1u);
    CHECK(b.take(0, false).empty());
}

TEST(vbutton_flush_forgets_unread_events) {
    VirtualButton b;
    b.set(true, 1);  // a press the client never read
    b.flush();       // the client flushed its buffer
    CHECK_EQ(b.pending(), 0u);
    CHECK(!b.reported());   // so as far as it knows the button is up
    CHECK(b.set(true, 2));  // and the press is queued again
}

TEST(vbutton_release_only_when_held) {
    VirtualButton b;
    CHECK(!b.release(1));
    b.set(true, 2);
    b.take(10, false);  // the client saw the press
    CHECK(b.release(3));
    auto ev = b.take(10, false);
    CHECK(ev.size() == 1 && !ev[0].down);
    CHECK(!b.reported());
}

TEST(vbutton_queue_is_bounded_and_keeps_alternation) {
    VirtualButton b;
    for (uint32_t t = 0; t < 100; ++t) b.set(t % 2 == 0, t);  // a client that never reads the buffer
    CHECK(b.pending() <= 16u);
    auto ev = b.take(100, false);
    for (size_t i = 1; i < ev.size(); ++i) CHECK(ev[i].down != ev[i - 1].down);
    CHECK(!ev.empty() && ev.back().down == b.reported());
}
