// 3.0.11-beta.42: the windows «Речі» (core/item_window) and «Характеристики» (core/stat_window) - their tabs and
// uses as the game's, the search, their places and the mouse, the stats' prices and what a point gives
#include <algorithm>
#include <string>
#include <vector>

#include "core/item_window.hpp"
#include "core/stat_window.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

itemwin::Texts itemTexts() {
    itemwin::Texts t;
    t.title = "Речі";
    t.hotkey = "Alt+E";
    t.tabs[0] = "Предмети";
    t.tabs[1] = "Спорядження";
    t.tabs[2] = "Інше";
    t.search = "Пошук за назвою";
    t.weight = "Вага";
    t.use = "двічі клік — використати";
    t.wear = "двічі клік — вдягнути";
    t.card = "двічі клік — у слот";
    t.info = "ПКМ — опис";
    return t;
}

statwin::Texts statTexts() {
    statwin::Texts t;
    t.title = "Характеристики";
    t.hotkey = "Alt+A";
    t.free = "Вільні очки: {1}";
    const char* cols[5] = {"Стат", "База", "Бонус", "Разом", "Ціна"};
    for (int i = 0; i < 5; ++i) t.columns[i] = cols[i];
    t.battle = "Бій";
    t.cast = "Час касту";
    t.maxed = "макс.";
    t.cost = "{1} {2} → {3} коштує {4}, у вас {5}";
    t.shortBy = "бракує {1}";
    t.top = "{1} {2} — максимум";
    t.effects[0] = "ATK +{1}, вантаж +30";
    t.effects[1] = "FLEE {1}";
    t.effects[2] = "Max HP +1 %";
    t.effects[3] = "MATK +{1} ~ +{2}";
    t.effects[4] = "HIT {1} · каст коротший на {2} %";
    t.effects[5] = "CRIT +0,3 %";
    t.hint = "наведіть на стат";
    return t;
}

itemwin::Item item(int index, int id, int type, int count, const std::string& name) {
    itemwin::Item it;
    it.index = index;
    it.id = id;
    it.type = type;
    it.count = count;
    it.name = name;
    return it;
}

}  // namespace

TEST(item_window_tabs_and_uses_as_the_game) {
    using itemwin::Tab;
    using itemwin::Use;
    CHECK(itemwin::tabOf(0) == Tab::Items && itemwin::tabOf(2) == Tab::Items && itemwin::tabOf(0x12) == Tab::Items);
    CHECK(itemwin::tabOf(4) == Tab::Gear && itemwin::tabOf(5) == Tab::Gear && itemwin::tabOf(0xD) == Tab::Gear);
    CHECK(itemwin::tabOf(3) == Tab::Etc && itemwin::tabOf(6) == Tab::Etc && itemwin::tabOf(10) == Tab::Etc);
    itemwin::Item potion = item(5, 501, 0, 38, "Red Potion");
    CHECK(itemwin::useOf(potion) == Use::Use);
    itemwin::Item sword = item(7, 1101, 4, 1, "Sword");
    sword.location = 2;
    CHECK(itemwin::useOf(sword) == Use::Wear);
    sword.identified = false;
    CHECK(itemwin::useOf(sword) == Use::None);  // the game wears identified ones only
    CHECK(itemwin::useOf(item(8, 4001, 6, 1, "Poring Card")) == Use::Card);
    CHECK(itemwin::useOf(item(9, 1750, 10, 500, "Arrow")) == Use::Ammo);
    CHECK(itemwin::useOf(item(10, 12210, 0x12, 1, "Bubble Gum")) == Use::Confirm);
    CHECK(itemwin::useOf(item(11, 909, 3, 20, "Jellopy")) == Use::None);
    CHECK(itemwin::useOf(item(0, 501, 0, 1, "none")) == Use::None);
}

TEST(item_window_search_and_places) {
    std::vector<itemwin::Item> items{item(1, 501, 0, 38, "Red Potion"), item(2, 909, 3, 20, "Jellopy"),
                                     item(3, 502, 0, 5, "Orange Potion"), item(4, 1101, 4, 1, "Sword")};
    const std::vector<int> potions = itemwin::shownOf(items, itemwin::Tab::Items, "POTION");
    CHECK(potions.size() == 2 && potions[0] == 0 && potions[1] == 2);
    CHECK(itemwin::shownOf(items, itemwin::Tab::Items, "").size() == 2);
    CHECK(itemwin::shownOf(items, itemwin::Tab::Etc, "jell").size() == 1);
    CHECK(itemwin::shownOf(items, itemwin::Tab::Gear, "").size() == 1);
    CHECK(itemwin::lowered("ЗІЛЛЯ Red") == "зілля red");
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(110, 140, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    CHECK(L.w == 500 && L.cells.size() == 40 && L.tabs.size() == 3 && L.x == 110 && L.y == 140);
    CHECK(L.cells[0].w == 48 && L.cells[1].x - L.cells[0].x == 56 && L.cells[8].y - L.cells[0].y == 56);
    CHECK(L.search.y > L.tabsRow.y && L.grid.y > L.search.y + L.search.h && L.footer.y > L.grid.y + L.grid.h);
    CHECK(itemwin::mostScroll(L, 40) == 0 && itemwin::mostScroll(L, 41) == 1 && itemwin::mostScroll(L, 100) == 8);
    const itemwin::Hit cell = itemwin::hitTest(L, 30, 1, L.cells[3].x + 4, L.cells[3].y + 4);
    CHECK(cell.part == itemwin::Part::Cell && cell.index == 11);  // a row down
    CHECK(itemwin::hitTest(L, 2, 0, L.cells[5].x + 4, L.cells[5].y + 4).part == itemwin::Part::Grid);
    CHECK(itemwin::hitTest(L, 2, 0, L.close.x + 3, L.close.y + 3).part == itemwin::Part::Close);
    const itemwin::Hit tab = itemwin::hitTest(L, 2, 0, L.tabs[2].x + 3, L.tabs[2].y + 3);
    CHECK(tab.part == itemwin::Part::Tab && tab.index == 2);
    CHECK(itemwin::hitTest(L, 2, 0, L.search.x + 50, L.search.y + 5).part == itemwin::Part::Search);
    CHECK(itemwin::hitTest(L, 2, 0, 5, 5).part == itemwin::Part::Title);
    CHECK(itemwin::hitTest(L, 2, 0, -1, -1).part == itemwin::Part::None);
}

TEST(item_window_picture_and_words) {
    std::vector<itemwin::Item> items{item(1, 501, 0, 38, "Red Potion"), item(2, 1101, 4, 1, "Sword")};
    items[1].refine = 7;
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    itemwin::Show sh;
    sh.hover = 0;
    sh.weight = 920;
    sh.weightMost = 2430;
    sh.zeny = 1482300;
    const std::vector<int> shown = itemwin::shownOf(items, itemwin::Tab::Items, "");
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    itemwin::paint(p, L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + 4] >> 24) > 0xC0);  // the glass
    const std::vector<itemwin::Words> w = itemwin::words(L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
    const auto has = [&](const std::string& s) {
        for (const itemwin::Words& x : w)
            if (x.text == s) return true;
        return false;
    };
    CHECK(has("Речі") && has("Alt+E") && has("Предмети") && has("38") && has("1 482 300 z"));
    CHECK(has("Red Potion \xC3\x97" "38") && has("двічі клік — використати \xC2\xB7 ПКМ — опис"));
    CHECK(has("920 / 2 430 \xC2\xB7 37 %") && has("Пошук за назвою"));
    CHECK(itemwin::nameOf(items[1]) == "+7 Sword");
    sh.tip = false;  // 3.0.11-beta.51: pointed at, its 0.4 s not over - no tip yet
    const std::vector<itemwin::Words> early =
        itemwin::words(L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
    CHECK(std::none_of(early.begin(), early.end(),
                       [](const itemwin::Words& x) { return x.text == "Red Potion \xC3\x97" "38"; }));
    CHECK(std::any_of(early.begin(), early.end(), [](const itemwin::Words& x) { return x.text == "38"; }));
}

// 3.0.11-beta.110 (the concept «Концепция окон и инвентаря», stage 2): a worn thing dragged out of «Спорядження» - the
// bag's grid lit in gold (a drop there takes it off)
TEST(item_window_lights_the_bag_while_a_worn_thing_is_dragged) {
    const std::vector<itemwin::Item> items{item(1, 501, 0, 38, "Red Potion")};
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    const std::vector<int> shown = itemwin::shownOf(items, itemwin::Tab::Items, "");
    const auto painted = [&](const itemwin::Show& show) {
        panel::Picture p;
        p.width = L.w;
        p.height = L.h;
        p.px.assign(size_t(L.w) * size_t(L.h), 0u);
        itemwin::paint(p, L, items, shown, show, t, itemwin::Measure(panel::wordsWidth));
        return p;
    };
    itemwin::Show sh;
    const panel::Picture plain = painted(sh);
    sh.dropping = true;
    const panel::Picture lit = painted(sh);
    const size_t at = size_t(int(L.grid.y + L.grid.h / 2)) * size_t(L.w) + size_t(int(L.grid.x) + 1);
    CHECK(((lit.px[at] >> 16) & 0xFF) > ((plain.px[at] >> 16) & 0xFF) + 20);
}

TEST(stat_window_prices_and_points) {
    const statwin::Texts t = statTexts();
    statwin::Data d;
    const int base[6] = {1, 48, 32, 99, 74, 1}, bonus[6] = {10, 12, 0, 10, 10, 0}, need[6] = {2, 6, 5, 0, 9, 2};
    for (int i = 0; i < 6; ++i) {
        d.base[i] = base[i];
        d.bonus[i] = bonus[i];
        d.need[i] = need[i];
    }
    d.free = 3;
    d.hit = 156;
    d.flee = 132;
    d.flee2 = 1;
    d.aspd = 161;
    CHECK(statwin::canRaise(d, 0) && !statwin::canRaise(d, 1) && !statwin::canRaise(d, 3) && statwin::canRaise(d, 5));
    CHECK(statwin::explain(d, 4, t) == "DEX 74 → 75 коштує 9, у вас 3 \xE2\x80\x94 бракує 6 \xC2\xB7 HIT 157 \xC2\xB7 "
                                       "каст коротший на 56,7 %");
    CHECK(statwin::explain(d, 3, t) == "INT 99 — максимум");
    CHECK(statwin::explain(d, 0, t) == "STR 1 → 2 коштує 2, у вас 3 \xC2\xB7 ATK +1, вантаж +30");  // 11 -> 12
    d.base[0] = 9;
    d.bonus[0] = 0;
    CHECK(statwin::explain(d, 0, t).find("ATK +2,") != std::string::npos);  // 9 -> 10: 1 + 1
    CHECK(statwin::explain(d, -1, t) == "наведіть на стат");
    const std::vector<statwin::Row> rows = statwin::battleRows(d, t);
    CHECK(rows.size() == 9 && rows[3].value == "132 + 1" && rows[8].name == "Час касту");
    CHECK(rows[8].value == "\xE2\x88\x92" "56 %");
}

TEST(stat_window_layout_and_mouse) {
    const statwin::Texts t = statTexts();
    statwin::Data d;
    d.free = 3;
    const statwin::Layout L = statwin::layout(660, 140, 1.0f, d, t, statwin::Measure(panel::wordsWidth));
    CHECK(L.w == 760 && L.rows.size() == 6 && L.pluses.size() == 6 && L.x == 660);
    CHECK(L.battle.x > L.table.x + L.table.w && L.line.y >= L.table.y + L.table.h);
    CHECK(L.freeChip.x + L.freeChip.w < L.close.x);
    const statwin::Hit plus = statwin::hitTest(L, L.pluses[4].x + 3, L.pluses[4].y + 3);
    CHECK(plus.part == statwin::Part::Plus && plus.index == 4);
    const statwin::Hit row = statwin::hitTest(L, L.rows[2].x + 10, L.rows[2].y + 10);
    CHECK(row.part == statwin::Part::Row && row.index == 2);
    CHECK(statwin::hitTest(L, L.close.x + 3, L.close.y + 3).part == statwin::Part::Close);
    CHECK(statwin::hitTest(L, 5, 5).part == statwin::Part::Title);
    statwin::Show sh;
    sh.hoverRow = 4;
    const std::vector<statwin::Words> w = statwin::words(L, d, sh, t, statwin::Measure(panel::wordsWidth));
    bool free = false, dex = false;
    for (const statwin::Words& x : w) {
        free = free || x.text == "Вільні очки: 3";
        dex = dex || x.text == "DEX";
    }
    CHECK(free && dex);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    statwin::paint(p, L, d, sh);
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + 4] >> 24) > 0xC0);
}

// 3.0.11-beta.44 (the owner: «когда использую какую-то вещь, она просто пропадает. нету никакой анимации»): a used
// item's cell answers - a gold ring opening out, its icon fading where the last one went, «−1» rising
TEST(item_window_use_flash) {
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    panel::Picture icon;
    icon.width = icon.height = 32;
    icon.px.assign(32 * 32, 0xFFD04040u);
    std::vector<itemwin::Item> items;  // the last one used: none left in its cell
    itemwin::Show sh;
    itemwin::Flash f;
    f.cell = L.cells[0];
    f.icon = &icon;
    f.t = 0.25f;
    f.label = "\xE2\x88\x92" "1";  // −1
    sh.flashes.push_back(f);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    itemwin::paint(p, L, items, {}, sh, t, itemwin::Measure(panel::wordsWidth));
    const panel::Box& c = L.cells[0];
    const uint32_t mid = p.px[size_t(int(c.y + c.h / 2)) * size_t(L.w) + size_t(int(c.x + c.w / 2))];
    CHECK(((mid >> 16) & 0xFF) > 0x70);  // the icon still there, fading
    sh.flashes[0].t = 0.95f;
    panel::Picture q = p;
    q.px.assign(q.px.size(), 0u);
    itemwin::paint(q, L, items, {}, sh, t, itemwin::Measure(panel::wordsWidth));
    const uint32_t late = q.px[size_t(int(c.y + c.h / 2)) * size_t(L.w) + size_t(int(c.x + c.w / 2))];
    CHECK(((late >> 16) & 0xFF) < ((mid >> 16) & 0xFF));  // gone by its end
    sh.flashes[0].t = 0.25f;
    bool label = false;
    for (const itemwin::Words& w : itemwin::words(L, items, {}, sh, t, itemwin::Measure(panel::wordsWidth)))
        label = label || (w.text == "\xE2\x88\x92" "1" && w.box.y < c.y);
    CHECK(label);
}
