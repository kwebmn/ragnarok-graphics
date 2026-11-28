// 3.0.24-beta.21 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Решено владельцем 27.11»:
// «Продажа NPC: «обране» в продажу не попадает; вещь с картой — всегда спрашиваем; заточка +5 и выше — спрашиваем»)
#include "core/shop_guard.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(shop_guard_what_is_asked_before_selling) {
    CHECK(shopguard::askBeforeSelling(0, 0) == shopguard::Ask::None);
    CHECK(shopguard::askBeforeSelling(4, 0) == shopguard::Ask::None);
    CHECK(shopguard::askBeforeSelling(5, 0) == shopguard::Ask::Refine);  // +5 and over
    CHECK(shopguard::askBeforeSelling(0, 1) == shopguard::Ask::Card);    // a card - always
    CHECK(shopguard::askBeforeSelling(7, 2) == shopguard::Ask::Card);    // the card said first
    CHECK(shopguard::sellable(false) && !shopguard::sellable(true));     // «обране» never in the sale's list
}

// the second click on the same thing within 3 s lets it go; another thing, or later - asked again
TEST(shop_guard_asked_once_then_let_go) {
    shopguard::Confirm c;
    CHECK(c.pass(3, shopguard::Ask::None, 1000));  // nothing to ask
    CHECK(!c.pass(3, shopguard::Ask::Card, 1000));
    CHECK(c.pass(3, shopguard::Ask::Card, 2500));
    CHECK(!c.pass(3, shopguard::Ask::Card, 2600));  // let go once: asked again
    CHECK(!c.pass(4, shopguard::Ask::Refine, 3000));
    CHECK(!c.pass(3, shopguard::Ask::Card, 3100));  // another thing between: asked anew
    CHECK(!c.pass(3, shopguard::Ask::Card, 9000));  // too late: asked anew
    CHECK(c.pass(3, shopguard::Ask::Card, 9500));
}

TEST(shop_guard_the_misc_tab_is_the_items_own) {
    CHECK(shopguard::misc(3) && !shopguard::misc(0) && !shopguard::misc(4) && !shopguard::misc(6));
}

// a thing's cards as its four slots hold them: real cards alone (4001-4999) - a forged one's (255 first), a named
// one's (254 first) and an egg's (-256) slots hold no card
TEST(shop_guard_counts_real_cards_alone) {
    const int none[4] = {0, 0, 0, 0}, two[4] = {4001, 0, 4035, 0}, forged[4] = {255, 0, 150000, 1};
    const int named[4] = {254, 0, 4001, 0}, egg[4] = {-256, 0, 0, 0};
    CHECK(shopguard::cardsIn(none) == 0 && shopguard::cardsIn(two) == 2);
    CHECK(shopguard::cardsIn(forged) == 0 && shopguard::cardsIn(named) == 0 && shopguard::cardsIn(egg) == 0);
}

// 3.0.24-beta.46 (the audit 28.11, the owner's «скрывать»): the worn things out of the sale's list too - the worn
// arrows «Arrow of Wind ×365» were offered at 1 z each
TEST(shop_guard_worn_things_not_for_sale) {
    CHECK(shopguard::sellable(false, false));
    CHECK(!shopguard::sellable(false, true));  // worn (ITEM_INFO's wear place not 0)
    CHECK(!shopguard::sellable(true, false));  // «★ Обране»
}
