// 3.0.24-beta.29 (docs/superpowers/plans/2026-11-27-site-market-prices.md, task 1; the spec's «Цены с рынка сайта»:
// «показывать «на ринку сайту від X z (N пропозицій)» по текущим лотам и среднюю — только при трёх сделках и
// больше»): the site's live.json market lines carry the deals' average and their count after the lots
// ([item, cheapest, lots, pieces, average, deals]); an old site's four fields still read
#include <string>

#include "core/reference_live.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(reference_live_reads_the_deals_average) {
    const std::string site = "{\"v\":1,\"at\":1000,\"mvp\":[],\"market\":[[2522,95000,2,3,100000,10],[501,12,1,5],"
                             "[984,0,0,0,19824,5]]}";
    reflive::Live l;
    CHECK(reflive::fromSite(site, 1000, l));
    CHECK(l.market.size() == 3u);
    CHECK(l.market[2522].average == 100000 && l.market[2522].deals == 10 && l.market[2522].lots == 2);
    CHECK(l.market[501].deals == 0 && l.market[501].average == 0 && l.market[501].cheapest == 12);  // an old line
    CHECK(l.market[984].deals == 5 && l.market[984].lots == 0);  // its deals alone: kept
    reflive::Live again;
    CHECK(reflive::read(reflive::write(l), again));
    CHECK(again.market[2522].average == 100000 && again.market[2522].deals == 10 && again.market[984].deals == 5);
}

TEST(market_line_words) {
    const std::string lots = "Ринок: від {1} z · лотів {2}", average = "середня {1} z · угод {2}";
    reflive::Market m;
    m.item = 2522, m.cheapest = 95000, m.lots = 2, m.pieces = 3;
    CHECK_EQ(reflive::marketLine(m, lots, average), std::string("Ринок: від 95 000 z · лотів 2"));
    m.average = 100000, m.deals = 10;
    CHECK_EQ(reflive::marketLine(m, lots, average),
             std::string("Ринок: від 95 000 z · лотів 2 \xC2\xB7 середня 100 000 z · угод 10"));
    m.deals = 2;  // fewer than three deals: no average
    CHECK_EQ(reflive::marketLine(m, lots, average), std::string("Ринок: від 95 000 z · лотів 2"));
    reflive::Market only;
    only.item = 984, only.average = 19824, only.deals = 3;  // no lot now: the average alone
    CHECK_EQ(reflive::marketLine(only, lots, average), std::string("середня 19 824 z · угод 3"));
    CHECK_EQ(reflive::marketLine(reflive::Market{}, lots, average), std::string());
}
