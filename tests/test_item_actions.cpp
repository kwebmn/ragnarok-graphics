#include "core/item_actions.hpp"

#include "test.hpp"

using namespace uagfx;
namespace ia = uagfx::itemactions;
namespace is = uagfx::itemstate;

namespace {
uint32_t amountIn(const itempackets::Bytes& b, size_t at) {
    return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
}  // namespace

// 3.0.24-beta.4 (stage 1 «Склад»; the design: «одно нажатие — одно действие»): a cell asked again while its first
// waits sends nothing
TEST(item_actions_one_press_one_packet) {
    ia::Queue q;
    itempackets::Bytes out;
    CHECK(q.ask(ia::Kind::ToStorage, 5, 3, 10, true, 0, out) == ia::Result::Sent);
    CHECK_EQ(out.size(), size_t(14));
    CHECK(q.waiting(is::Where::Bag, 5));
    itempackets::Bytes again;
    CHECK(q.ask(ia::Kind::ToStorage, 5, 3, 10, true, 10, again) == ia::Result::Busy);
    CHECK(again.empty());
    CHECK(q.ask(ia::Kind::ToStorage, 6, 1, 1, true, 10, again) == ia::Result::Sent);
}

// the server's answer ends the wait: the bag's thing out at that index (into the storage), the storage's (out of it)
TEST(item_actions_done_by_the_servers_answer) {
    ia::Queue q;
    itempackets::Bytes out;
    q.ask(ia::Kind::ToStorage, 5, 3, 10, true, 0, out);
    q.ask(ia::Kind::FromStorage, 9, 2, 2, true, 0, out);
    CHECK(q.onChange(is::Change{is::Where::Bag, 7, -1}) == ia::Outcome::None);  // another cell
    CHECK(q.onChange(is::Change{is::Where::Bag, 5, -3}) == ia::Outcome::Done);
    CHECK(!q.waiting(is::Where::Bag, 5));
    CHECK(q.onChange(is::Change{is::Where::Storage, 9, -2}) == ia::Outcome::Done);
    CHECK(!q.waiting(is::Where::Storage, 9));
}

// no answer in 2 s: the cell free again, the wait told as timed out
TEST(item_actions_timeout_frees_the_cell) {
    ia::Queue q;
    itempackets::Bytes out;
    q.ask(ia::Kind::ToStorage, 5, 3, 10, true, 1000, out);
    CHECK(q.tick(1000 + ia::kTimeoutMs - 1) == ia::Outcome::None);
    CHECK(q.tick(1000 + ia::kTimeoutMs) == ia::Outcome::TimedOut);
    CHECK(!q.waiting(is::Where::Bag, 5));
}

// never more than there is; nothing there - nothing sent; no connection - nothing sent
TEST(item_actions_never_more_than_there_is) {
    ia::Queue q;
    itempackets::Bytes out;
    CHECK(q.ask(ia::Kind::ToStorage, 5, 50, 7, true, 0, out) == ia::Result::Sent);
    CHECK_EQ(amountIn(out, 10), 7u);
    itempackets::Bytes none;
    CHECK(q.ask(ia::Kind::FromStorage, 3, 1, 0, true, 0, none) == ia::Result::NothingToMove);
    CHECK(q.ask(ia::Kind::FromStorage, 4, 1, 1, false, 0, none) == ia::Result::NotConnected);
    CHECK(none.empty());
}

// the storage closed while a take waits: the wait dropped, the cell free
TEST(item_actions_closed_storage_drops_the_waiting) {
    ia::Queue q;
    itempackets::Bytes out;
    q.ask(ia::Kind::FromStorage, 9, 2, 2, true, 0, out);
    is::Change closed;
    closed.where = is::Where::Storage;
    closed.closed = true;
    CHECK(q.onChange(closed) == ia::Outcome::Dropped);
    CHECK(!q.waiting(is::Where::Storage, 9));
}
