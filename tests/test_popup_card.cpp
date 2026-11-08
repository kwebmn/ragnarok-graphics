// 3.0.11-beta.211 (the tester: «меню взаємодії з іншим гравцем - в старому стилі»): the game's popup menu (UI window
// 0x12: a player's, a chat room member's, a guild member's) as a card (core/popup_card) - its items as the game keeps
// them (each twice, a half row each; a separator once), the number the game's own click gives its owner
#include <algorithm>
#include <string>
#include <vector>

#include "core/popup_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

const std::string kSep = "---";

std::vector<std::string> raw() {
    return {"Deal", "Deal", "Chat", "Chat", kSep, "Friend", "Friend", kSep, "Block", "Block"};
}

}  // namespace

// Each item once, the separators kept apart
TEST(popup_card_items) {
    const std::vector<popupcard::Item> it = popupcard::items(raw(), kSep);
    CHECK_EQ(int(it.size()), 6);
    CHECK(it[0].text == "Deal" && !it[0].separator);
    CHECK(it[2].separator && it[3].text == "Friend" && it[4].separator && it[5].text == "Block");
    CHECK(popupcard::items({}, kSep).empty());
    const std::vector<popupcard::Item> odd = popupcard::items({"A", "A", "B"}, kSep);  // a lone last one still shown
    CHECK_EQ(int(odd.size()), 2);
}

// The number the game's click gives: the item's among the items (the separators not counted); none for a separator
TEST(popup_card_pick_numbers) {
    const std::vector<popupcard::Item> it = popupcard::items(raw(), kSep);
    CHECK_EQ(popupcard::pickNumber(it, 0), 0);
    CHECK_EQ(popupcard::pickNumber(it, 1), 1);
    CHECK_EQ(popupcard::pickNumber(it, 2), -1);
    CHECK_EQ(popupcard::pickNumber(it, 3), 2);
    CHECK_EQ(popupcard::pickNumber(it, 5), 3);
    CHECK_EQ(popupcard::pickNumber(it, 9), -1);
}

// At the click, inside the screen; a row per item (a separator thin); the rows found where they are drawn
TEST(popup_card_layout_and_rows) {
    const std::vector<popupcard::Item> it = popupcard::items(raw(), kSep);
    popupcard::Layout L = popupcard::layout(it, 300, 200, 1920, 1080, 1.0f, 150.0f);
    CHECK_EQ(L.x, 300);
    CHECK_EQ(L.y, 200);
    CHECK_EQ(int(L.rows.size()), 6);
    CHECK(L.rows[2].h < L.rows[0].h);
    CHECK(L.w >= 150);
    for (int i : {0, 1, 3, 5}) {
        const panel::Box& r = L.rows[size_t(i)];
        CHECK_EQ(popupcard::rowAt(L, it, r.x + r.w / 2, r.y + r.h / 2), i);
    }
    CHECK_EQ(popupcard::rowAt(L, it, L.rows[2].x + 5, L.rows[2].y + L.rows[2].h / 2), -1);
    L = popupcard::layout(it, 1900, 1070, 1920, 1080, 1.0f, 150.0f);
    CHECK(L.x + L.w <= 1920 && L.y + L.h <= 1080);
}

// The words: each item's text, the hovered one marked
TEST(popup_card_words) {
    const std::vector<popupcard::Item> it = popupcard::items(raw(), kSep);
    const popupcard::Layout L = popupcard::layout(it, 300, 200, 1920, 1080, 1.0f, 150.0f);
    const std::vector<popupcard::Words> w = popupcard::words(L, it, 3);
    CHECK_EQ(int(w.size()), 4);
    const auto f = std::find_if(w.begin(), w.end(), [](const popupcard::Words& x) { return x.text == "Friend"; });
    const auto d = std::find_if(w.begin(), w.end(), [](const popupcard::Words& x) { return x.text == "Deal"; });
    CHECK(f != w.end() && d != w.end() && f->rgb != d->rgb);
}

// 3.0.16-beta.17 (the tester: the pet's menu «без укр перекладу»): the game's pet lines in the player's language
TEST(popup_card_the_pets_menu_translated) {
    CHECK(popupcard::translated("Feed Pet", settings::Lang::Uk) == "Нагодувати");
    CHECK(popupcard::translated("Check the Pet Status", settings::Lang::Ru) == "Состояние питомца");
    CHECK(popupcard::translated("Feed Pet", settings::Lang::En) == "Feed Pet");
    CHECK(popupcard::translated("Trade", settings::Lang::Uk) == "Trade");
}
