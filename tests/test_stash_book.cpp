// 3.0.11-beta.286 (the owner's feature 15): where else a thing lies, across the player's characters
#include "core/stash_book.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(stash_book_finds_elsewhere_and_keeps) {
    using stashbook::Kind;
    using stashbook::Place;
    stashbook::Book b;
    CHECK(b.note(Place{Kind::Bag, "Анна Ліс"}, {{501, 3}, {909, 10}}));
    CHECK(!b.note(Place{Kind::Bag, "Анна Ліс"}, {{501, 3}, {909, 10}}));
    CHECK(b.note(Place{Kind::Storage, "2000001"}, {{501, 12}}));
    CHECK(b.note(Place{Kind::Cart, "Борис"}, {{501, 1}}));
    CHECK(!b.note(Place{Kind::Bag, ""}, {{501, 1}}));
    const auto red = [](int id) { return id == 501; };
    const auto found = b.find(red, {Place{Kind::Bag, "Борис"}});
    CHECK_EQ(found.size(), size_t(3));
    CHECK(found[0].first.kind == Kind::Storage && found[0].second == 12);
    const stashbook::Texts t{"Also: {1}", "Storage", "cart"};
    CHECK_EQ(stashbook::line(found, t),
             std::string("Also: Storage \xC3\x97" "12 \xC2\xB7 Анна Ліс \xC3\x97" "3 \xC2\xB7 "
                         "Борис, cart \xC3\x97" "1"));
    const stashbook::Book back = stashbook::Book::parse(b.text() + "junk\n");
    CHECK_EQ(back.find(red, {}).size(), size_t(3));
    CHECK_EQ(back.text(), b.text());
    CHECK(b.find(red, {Place{Kind::Bag, "Анна Ліс"}, Place{Kind::Storage, "2000001"}}).size() == 1);
    CHECK_EQ(stashbook::line({}, t), std::string(""));
}
