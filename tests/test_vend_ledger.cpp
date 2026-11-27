// 3.0.24-beta.23 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Відкрити лавку», «Моя лавка»;
// the plan's task 9): earned as the server pays it (uAthena vending.c: z * (1 - vending_tax / 10000) - an integer
// division, so vending_tax 200 takes nothing: the whole price reaches the merchant), the last buyer by the server's
// line («%s has bought your item(s).», msg 265, buyer_name yes), the prices kept for the next shop, our own packet
#include <string>

#include "core/item_packets.hpp"
#include "core/my_shop.hpp"
#include "core/ui_chime.hpp"
#include "core/vend_prices.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(vend_ledger_earned_as_the_server_pays_and_the_buyer) {
    CHECK_EQ(myshop::netEarned(100000, 200), 100000LL);  // the server's own arithmetic: 200 / 10000 is 0
    CHECK_EQ(myshop::netEarned(100000, 10000), 0LL);     // only a tax of 10000 (all) takes anything
    CHECK_EQ(myshop::netEarned(0, 200), 0LL);
    CHECK_EQ(myshop::buyerOf("Tester has bought your item(s)."), std::string("Tester"));
    CHECK_EQ(myshop::buyerOf("Big Mike has bought your item(s)."), std::string("Big Mike"));
    CHECK_EQ(myshop::buyerOf("Guild : hello"), std::string());
    CHECK_EQ(myshop::buyerOf(" has bought your item(s)."), std::string());
}

TEST(vend_prices_kept_for_the_next_shop) {
    vendprices::Prices p;
    p.read("# remark\n501=50\n1201=12000\nbad line\n909=\n");
    CHECK_EQ(p.priceOf(501), 50LL);
    CHECK_EQ(p.priceOf(1201), 12000LL);
    CHECK_EQ(p.priceOf(909), 0LL);  // none kept
    p.remember(909, 1500);
    p.remember(501, 0);  // a nought forgets nothing
    vendprices::Prices again;
    again.read(p.text());
    CHECK_EQ(again.priceOf(909), 1500LL);
    CHECK_EQ(again.priceOf(501), 50LL);
}

TEST(vend_packets_open_the_shop) {
    const itempackets::Bytes open = itempackets::openVending("My shop", {{4, 10, 500}, {7, 1, 1200000}});
    CHECK(open.size() == 85u + 16u);
    CHECK(open[0] == 0xb2 && open[1] == 0x01 && open[2] == 101 && open[3] == 0);  // its length
    CHECK(std::string(reinterpret_cast<const char*>(open.data() + 4)) == "My shop" && open[84] == 1);
    CHECK(open[85] == 4 && open[87] == 10 && open[89] == 0xf4 && open[90] == 0x01);  // place, amount, 500
    CHECK(open[93] == 7 && open[95] == 1);
    CHECK(open[97] == 0x80 && open[98] == 0x4f && open[99] == 0x12);  // 1 200 000
    CHECK(itempackets::openVending(std::string(100, 'a'), {}).at(83) == 0);  // the sign cut at 79, its nought kept
    CHECK(!chime::wav(chime::Kind::Sale).empty());  // the soft sale's sound
}

// 3.0.24-beta.24 (the plan's task 10, [interface] trade_packets): the trade's buttons by our own packets - «Готово»
// 0x00eb, «Скасувати» 0x00ed, «Обмін» 0x00ef (each 2 bytes, packet_db's section 22)
TEST(trade_packets_the_buttons) {
    CHECK(itempackets::tradeOk() == (itempackets::Bytes{0xeb, 0x00}));
    CHECK(itempackets::tradeCancel() == (itempackets::Bytes{0xed, 0x00}));
    CHECK(itempackets::tradeCommit() == (itempackets::Bytes{0xef, 0x00}));
}
