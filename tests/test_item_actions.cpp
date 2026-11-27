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

// 3.0.24-beta.8 (stage 2 «Візок»): each cart move ends by its leaving side's answer
TEST(item_actions_cart_moves_end_by_their_leaving_side) {
    ia::Queue q;
    itempackets::Bytes out;
    CHECK(q.ask(ia::Kind::ToCart, 5, 2, 2, true, 0, out) == ia::Result::Sent);
    CHECK(q.waiting(is::Where::Bag, 5));
    CHECK(q.ask(ia::Kind::FromCart, 6, 1, 1, true, 0, out) == ia::Result::Sent);
    CHECK(q.waiting(is::Where::Cart, 6));
    CHECK(q.ask(ia::Kind::StorageToCart, 7, 1, 1, true, 0, out) == ia::Result::Sent);
    CHECK(q.waiting(is::Where::Storage, 7));
    CHECK(q.ask(ia::Kind::CartToStorage, 8, 1, 1, true, 0, out) == ia::Result::Sent);
    CHECK(q.waiting(is::Where::Cart, 8));
    CHECK(q.onChange(is::Change{is::Where::Cart, 6, -1}) == ia::Outcome::Done);
    CHECK(q.onChange(is::Change{is::Where::Cart, 8, -1}) == ia::Outcome::Done);
    CHECK(q.onChange(is::Change{is::Where::Bag, 5, -2}) == ia::Outcome::Done);
    CHECK(q.onChange(is::Change{is::Where::Storage, 7, -1}) == ia::Outcome::Done);
    is::Change gone;
    gone.where = is::Where::Cart;
    gone.closed = true;
    q.ask(ia::Kind::FromCart, 9, 1, 1, true, 0, out);
    CHECK(q.onChange(gone) == ia::Outcome::Dropped);  // the cart emptied: its waits end
}

// 3.0.24-beta.9 (stage 3 «Магазин»): one deal at a time; its answer ends it - 0 done, else refused with its reason
TEST(item_actions_npc_deal_one_at_a_time_and_its_answer) {
    ia::Queue q;
    itempackets::Bytes out;
    CHECK(q.askDeal(true, {{501, 10}}, true, 0, out) == ia::Result::Sent);
    CHECK_EQ(out.size(), size_t(8));
    itempackets::Bytes again;
    CHECK(q.askDeal(true, {{501, 1}}, true, 0, again) == ia::Result::Busy);
    is::Change answer;
    answer.where = is::Where::Shop;
    answer.result = 1;  // not enough zeny
    CHECK(q.onChange(answer) == ia::Outcome::Refused);
    CHECK_EQ(q.lastResult(), 1);
    CHECK(q.askDeal(false, {{7, 3}}, true, 0, out) == ia::Result::Sent);
    answer.result = 0;
    CHECK(q.onChange(answer) == ia::Outcome::Done);
    CHECK(q.askDeal(true, {}, true, 0, out) == ia::Result::NothingToMove);
    CHECK(q.askDeal(true, {{501, 1}}, false, 0, out) == ia::Result::NotConnected);
}

TEST(item_state_npc_deal_answers) {
    is::State s;
    const std::vector<uint8_t> bought{0xca, 0x00, 0x02}, sold{0xcb, 0x00, 0x00};
    auto c = s.feed(bought.data(), bought.size());
    CHECK(c && c->where == is::Where::Shop && c->result == 2);
    c = s.feed(sold.data(), sold.size());
    CHECK(c && c->where == is::Where::Shop && c->result == 0);
}

// 3.0.24-beta.18 (stage 4 «Лавка»): the purchase has no answer of its own when it goes through - the things coming
// into the bag end it; 0x0135 refuses it (1 zeny, 2 weight, 4 the shop has not so many)
TEST(item_actions_vend_purchase_ends_by_the_bag_or_its_refusal) {
    ia::Queue q;
    itempackets::Bytes out;
    CHECK(q.askVend(2000123, {{5, 3}}, true, 0, out) == ia::Result::Sent);
    CHECK_EQ(out.size(), size_t(12));
    is::Change came;
    came.where = is::Where::Bag;
    came.index = 7;
    came.delta = 3;
    CHECK(q.onChange(came) == ia::Outcome::Done);
    CHECK(q.onChange(came) == ia::Outcome::None);  // nothing waits now: a pickup is no answer
    CHECK(q.askVend(2000123, {{5, 3}}, true, 0, out) == ia::Result::Sent);
    is::Change no;
    no.where = is::Where::Shop;
    no.result = 4;
    CHECK(q.onChange(no) == ia::Outcome::Refused);
    CHECK_EQ(q.lastResult(), 4);
}

TEST(item_state_vend_refusal) {
    is::State s;
    const std::vector<uint8_t> refused{0x35, 0x01, 0x07, 0x00, 0x03, 0x00, 0x01};
    auto c = s.feed(refused.data(), refused.size());
    CHECK(c && c->where == is::Where::Shop && c->result == 1);
}
