#include "core/item_state.hpp"

#include "test.hpp"

using namespace uagfx;
namespace is = uagfx::itemstate;

namespace {
using Bytes = std::vector<uint8_t>;
void w16(Bytes& b, size_t at, uint16_t v) { b[at] = uint8_t(v), b[at + 1] = uint8_t(v >> 8); }
void w32(Bytes& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + size_t(i)] = uint8_t(v >> (8 * i));
}
// 0x01f0 / 0x01ee: stackables, 18 bytes each from 4 - index, id, type, identified, amount, equip, cards
Bytes stackList(uint16_t cmd, const std::vector<std::pair<uint16_t, uint16_t>>& indexAmount, uint16_t id = 501) {
    Bytes b(4 + indexAmount.size() * 18, 0);
    w16(b, 0, cmd);
    w16(b, 2, uint16_t(b.size()));
    for (size_t k = 0; k < indexAmount.size(); ++k) {
        const size_t at = 4 + k * 18;
        w16(b, at, indexAmount[k].first);
        w16(b, at + 2, id);
        b[at + 5] = 1;
        w16(b, at + 6, indexAmount[k].second);
        w16(b, at + 10, 4001);  // a card
    }
    return b;
}
// 0x00a6 / 0x00a4: equipment, 20 each - index, id, type, identified, location, equipped, attribute, refine, cards
Bytes equipList(uint16_t cmd, uint16_t index, uint16_t id, uint8_t refine) {
    Bytes b(24, 0);
    w16(b, 0, cmd);
    w16(b, 2, 24);
    w16(b, 4, index);
    w16(b, 6, id);
    b[9] = 1;
    w16(b, 10, 2);
    b[15] = refine;
    return b;
}
Bytes added(uint16_t index, uint32_t amount, uint16_t id) {  // 0x00f4
    Bytes b(21, 0);
    w16(b, 0, 0x00f4);
    w16(b, 2, index);
    w32(b, 4, amount);
    w16(b, 8, id);
    b[10] = 1;
    return b;
}
Bytes removed(uint16_t index, uint32_t amount) {  // 0x00f6
    Bytes b(8, 0);
    w16(b, 0, 0x00f6);
    w16(b, 2, index);
    w32(b, 4, amount);
    return b;
}
}  // namespace

// 3.0.24-beta.4 (stage 1 «Склад»): the storage as the server tells it - its lists (stackables and equipment), its count
TEST(item_state_storage_list_and_count) {
    is::State s;
    s.feed(stackList(0x01f0, {{1, 30}, {2, 5}}).data(), 4 + 2 * 18);
    const Bytes eq = equipList(0x00a6, 3, 2101, 7);
    s.feed(eq.data(), eq.size());
    Bytes count(6, 0);
    w16(count, 0, 0x00f2), w16(count, 2, 3), w16(count, 4, 600);
    s.feed(count.data(), count.size());
    CHECK_EQ(s.storage().size(), size_t(3));
    CHECK(s.storageOpen());
    CHECK_EQ(s.storageMost(), 600);
    const is::Item* sword = s.find(is::Where::Storage, 3);
    CHECK(sword && sword->id == 2101 && sword->refine == 7 && sword->amount == 1u);
    const is::Item* first = s.find(is::Where::Storage, 1);
    CHECK(first && first->amount == 30u && first->cards[0] == 4001);
}

// a thing in, the same once more (its amount grows), out to none (gone); the storage closed
TEST(item_state_storage_add_remove_close) {
    is::State s;
    Bytes b = added(4, 5, 501);
    auto c = s.feed(b.data(), b.size());
    CHECK(c && c->where == is::Where::Storage && c->index == 4 && c->delta == 5);
    s.feed(b.data(), b.size());
    CHECK(s.find(is::Where::Storage, 4) && s.find(is::Where::Storage, 4)->amount == 10u);
    b = removed(4, 10);
    c = s.feed(b.data(), b.size());
    CHECK(c && c->delta == -10);
    CHECK(s.find(is::Where::Storage, 4) == nullptr);
    const Bytes close{0xf8, 0x00};
    c = s.feed(close.data(), close.size());
    CHECK(!s.storageOpen() && c && c->closed);
}

// the bag: a thing taken (0x00a0, its result 0), a refusal (result not 0) changes nothing, out (0x00af), used (0x01c8)
TEST(item_state_bag_add_and_delete) {
    is::State s;
    Bytes a(23, 0);
    w16(a, 0, 0x00a0), w16(a, 2, 7), w16(a, 4, 3), w16(a, 6, 512), a[8] = 1;
    s.feed(a.data(), a.size());
    CHECK(s.find(is::Where::Bag, 7) && s.find(is::Where::Bag, 7)->amount == 3u);
    a[22] = 2;  // «too heavy»
    CHECK(!s.feed(a.data(), a.size()));
    CHECK_EQ(s.find(is::Where::Bag, 7)->amount, 3u);
    Bytes d(6, 0);
    w16(d, 0, 0x00af), w16(d, 2, 7), w16(d, 4, 1);
    const auto c = s.feed(d.data(), d.size());
    CHECK(c && c->where == is::Where::Bag && c->delta == -1);
    Bytes u(13, 0);
    w16(u, 0, 0x01c8), w16(u, 2, 7), w16(u, 4, 512), w16(u, 10, 0), u[12] = 1;
    s.feed(u.data(), u.size());
    CHECK(s.find(is::Where::Bag, 7) == nullptr);  // none left
}

// a list of hundreds is read whole (the packet's own length, not a cut)
TEST(item_state_long_list_not_cut) {
    std::vector<std::pair<uint16_t, uint16_t>> many;
    for (uint16_t i = 1; i <= 300; ++i) many.push_back({i, 1});
    const Bytes b = stackList(0x01f0, many);
    CHECK_EQ(b.size(), size_t(5404));
    is::State s;
    s.feed(b.data(), b.size());
    CHECK_EQ(s.storage().size(), size_t(300));
    // a short or broken packet changes nothing
    s.feed(b.data(), 10);
    CHECK_EQ(s.storage().size(), size_t(300));
}
