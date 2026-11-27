#include "core/item_packets.hpp"

#include "test.hpp"

using namespace uagfx;

namespace {
uint16_t rd16(const itempackets::Bytes& b, size_t at) { return uint16_t(b[at] | (b[at + 1] << 8)); }
uint32_t rd32(const itempackets::Bytes& b, size_t at) {
    return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
}  // namespace

// 3.0.24-beta.4 (stage 1 «Склад»): the storage's requests as uAthena reads them for the 2007-05-07 client
TEST(item_packets_storage_requests_as_the_server_reads_them) {
    const itempackets::Bytes put = itempackets::toStorage(5, 3);
    CHECK_EQ(put.size(), size_t(14));
    CHECK_EQ(rd16(put, 0), 0x0094);
    CHECK_EQ(rd16(put, 7), 5);
    CHECK_EQ(rd32(put, 10), 3u);
    const itempackets::Bytes take = itempackets::fromStorage(12, 40);
    CHECK_EQ(take.size(), size_t(22));
    CHECK_EQ(rd16(take, 0), 0x00f7);
    CHECK_EQ(rd16(take, 14), 12);
    CHECK_EQ(rd32(take, 18), 40u);
    CHECK(itempackets::closeStorage() == (itempackets::Bytes{0x93, 0x01}));
    CHECK_EQ(itempackets::serverLength(0x01f0), -1);
    CHECK_EQ(itempackets::serverLength(0x00f6), 8);
    CHECK_EQ(itempackets::serverLength(0x00f4), 21);
    CHECK_EQ(itempackets::serverLength(0x1234), 0);
}

// 3.0.24-beta.8 (stage 2 «Візок»): the cart's requests - putitemtocart 0x0126, getitemfromcart 0x0127,
// movefromkafratocart 0x0128, movetokafrafromcart 0x0129: 8 bytes, the index at 2, the amount at 4
TEST(item_packets_cart_requests_as_the_server_reads_them) {
    const struct {
        itempackets::Bytes b;
        uint16_t id;
    } all[] = {{itempackets::toCart(5, 3), 0x0126}, {itempackets::fromCart(6, 4), 0x0127},
               {itempackets::storageToCart(7, 5), 0x0128}, {itempackets::cartToStorage(8, 6), 0x0129}};
    uint16_t index = 5;
    uint32_t amount = 3;
    for (const auto& x : all) {
        CHECK_EQ(x.b.size(), size_t(8));
        CHECK_EQ(rd16(x.b, 0), x.id);
        CHECK_EQ(rd16(x.b, 2), index++);
        CHECK_EQ(rd32(x.b, 4), amount++);
    }
    CHECK_EQ(itempackets::serverLength(0x0124), 21);
    CHECK_EQ(itempackets::serverLength(0x0125), 8);
    CHECK_EQ(itempackets::serverLength(0x01ef), -1);
    CHECK_EQ(itempackets::serverLength(0x0122), -1);
    CHECK_EQ(itempackets::serverLength(0x0121), 14);
    CHECK_EQ(itempackets::serverLength(0x012b), 2);
}

// 3.0.24-beta.9 (stage 3 «Магазин»): the NPC deal - npcbuylistsend 0x00c8 {amount.W, item.W}..., npcselllistsend
// 0x00c9 {index.W, amount.W}..., the length at 2; the answers 0x00ca / 0x00cb, 3 bytes
TEST(item_packets_npc_deal_lists) {
    const itempackets::Bytes buy = itempackets::buyList({{501, 10}, {502, 2}});
    CHECK_EQ(buy.size(), size_t(12));
    CHECK_EQ(rd16(buy, 0), 0x00c8);
    CHECK_EQ(rd16(buy, 2), 12);
    CHECK_EQ(rd16(buy, 4), 10);  // the amount first
    CHECK_EQ(rd16(buy, 6), 501);
    CHECK_EQ(rd16(buy, 8), 2);
    CHECK_EQ(rd16(buy, 10), 502);
    const itempackets::Bytes sell = itempackets::sellList({{7, 3}});
    CHECK_EQ(sell.size(), size_t(8));
    CHECK_EQ(rd16(sell, 0), 0x00c9);
    CHECK_EQ(rd16(sell, 4), 7);  // the index first
    CHECK_EQ(rd16(sell, 6), 3);
    CHECK_EQ(itempackets::serverLength(0x00ca), 3);
    CHECK_EQ(itempackets::serverLength(0x00cb), 3);
}

// 3.0.24-beta.18 (stage 4 «Лавка»): purchasereq 0x0134 - the length at 2, the merchant's account at 4, then
// {amount.W, place.W} (the place as the shop's list 0x0133 gave it: the cart's index + 2); its refusal 0x0135, 7 bytes
TEST(item_packets_vend_purchase) {
    const itempackets::Bytes b = itempackets::vendBuy(2000123, {{5, 3}, {9, 1}});
    CHECK_EQ(b.size(), size_t(16));
    CHECK_EQ(rd16(b, 0), 0x0134);
    CHECK_EQ(rd16(b, 2), 16);
    CHECK_EQ(uint32_t(rd16(b, 4) | (rd16(b, 6) << 16)), 2000123u);
    CHECK_EQ(rd16(b, 8), 3);  // the amount first
    CHECK_EQ(rd16(b, 10), 5);
    CHECK_EQ(rd16(b, 12), 1);
    CHECK_EQ(rd16(b, 14), 9);
    CHECK_EQ(itempackets::serverLength(0x0135), 7);
}
