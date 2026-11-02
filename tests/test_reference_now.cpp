// 3.0.11-beta.174 (the login concept's news line): the site's item now (core/reference_now) - read from its answer,
// none when it has none, its address only the site's, kept for the three languages in the watcher's file
#include <string>

#include "core/reference_now.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(reference_now_reads_an_item_or_none) {
    refnow::Now n;
    CHECK(refnow::parse(R"({"item":{"kind":"event","title":"Полювання на MVP",)"
                        R"("url":"https://uaro.kiev.ua/ua/events/7","starts_at":1762100000,"ends_at":1762110000}})",
                        n));
    CHECK(n.title == "Полювання на MVP");
    CHECK(n.url == "https://uaro.kiev.ua/ua/events/7");
    CHECK(n.startsAt == 1762100000 && n.endsAt == 1762110000);
    refnow::Now none;
    CHECK(!refnow::parse(R"({"item":null})", none));
    CHECK(!refnow::parse("<html><body>404</body></html>", none));
    CHECK(!refnow::parse(R"({"item":{"title":""}})", none));
}

// Another site's address is never opened (the words stay, the click does nothing); the title one line
TEST(reference_now_keeps_only_the_sites_addresses) {
    refnow::Now n;
    CHECK(refnow::parse(R"({"item":{"title":"X","url":"https://evil.example/uaro.kiev.ua/"}})", n));
    CHECK(n.title == "X" && n.url.empty());
    CHECK(refnow::parse(R"({"item":{"title":"X","url":"https://uaro.kiev.ua.evil.example/"}})", n));
    CHECK(n.url.empty());
    CHECK(refnow::parse(R"({"item":{"title":"a\nb\tc"}})", n));
    CHECK(n.title == "a b c");
    CHECK(refnow::parse(R"({"item":{"title":"X","url":"https://uaro.kiev.ua/uk/news/7","ends_at":100}})", n));
    CHECK(refnow::current(n, 99) && !refnow::current(n, 100));  // an event over: not shown
}

// The watcher's file: the three languages' items (none for one of them kept as none), read back; fresh a day
TEST(reference_now_file_round_trip) {
    refnow::File f;
    f.at = 1762100000;
    f.items[int(settings::Lang::Uk)] = refnow::Now{"Полювання на MVP", "https://uaro.kiev.ua/uk/news/7", 1, 2};
    f.items[int(settings::Lang::En)] = refnow::Now{"MVP \"hunt\" \\ go", "https://uaro.kiev.ua/en/news/7", 0, 0};
    refnow::File back;
    CHECK(refnow::read(refnow::write(f), back));
    CHECK(back.at == f.at);
    const auto& uk = back.items[int(settings::Lang::Uk)];
    CHECK(uk.has_value() && uk->title == "Полювання на MVP" && uk->startsAt == 1 && uk->endsAt == 2);
    const auto& en = back.items[int(settings::Lang::En)];
    CHECK(en.has_value() && en->title == "MVP \"hunt\" \\ go" && en->url == "https://uaro.kiev.ua/en/news/7");
    CHECK(!back.items[int(settings::Lang::Ru)].has_value());
    CHECK(refnow::fresh(back, f.at + 3600) && !refnow::fresh(back, f.at + 2 * 86400));
    CHECK(!refnow::read("{}", back));
}
