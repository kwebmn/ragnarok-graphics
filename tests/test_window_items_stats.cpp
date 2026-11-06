// 3.0.11-beta.42: the windows «Речі» (core/item_window) and «Характеристики» (core/stat_window) - their tabs and
// uses as the game's, the search, their places and the mouse, the stats' prices and what a point gives
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/item_window.hpp"
#include "core/stat_math.hpp"
#include "core/stat_window.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

itemwin::Texts itemTexts() {
    itemwin::Texts t;
    t.title = "Речі";
    t.hotkey = "Alt+E";
    const char* tabs[itemwin::kTabs] = {"Усе", "Витратне", "Спорядження", "Карти", "Різне", "\xE2\x98\x85"};
    for (int i = 0; i < itemwin::kTabs; ++i) t.tabs[i] = tabs[i];
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
    t.effects[4] = "HIT {1} · каст коротший: {2} → {3} %";  // 3.0.11-beta.202: from the cast now to the next
    t.effects[5] = "CRIT +0,3 %";
    t.hint = "наведіть на стат";
    t.delay = "Затримка після вміння";  // 3.0.11-beta.266
    t.regen = "Відновлення за тік";
    t.weight = "Вага";
    t.perSecond = "{1}/с";
    t.how = "Як рахується · {1}";
    t.howNone = "Наведіть на рядок";
    t.howSame = "сервер: {1} — збігається";
    t.howMore = "сервер: {1} — статами {2}, речі {3}";
    t.howWeapon = "ATK зброї: {1} · заточка: + {2}";
    t.howLevel = "рівень {1}";
    t.howAspdWeapon = "{1} із {2}: затримка {3}";
    t.howAspdUnknown = "зброю не визначено";
    t.howHits = "{1} удару на секунду";
    t.howAspdGoal = "до ASPD {1} потрібно AGI {2}";
    t.howAspdNever = "до ASPD {1} потрібно AGI {2}: більше за 99";
    t.howNext = "наступне очко {1}: {2}";
    t.howJob = "від рівнів професії: +{1}";
    t.howWeight = "70 %: {1} · 90 %: {2}";
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
    CHECK(itemwin::tabOf(0) == Tab::Use && itemwin::tabOf(2) == Tab::Use && itemwin::tabOf(0x12) == Tab::Use);
    CHECK(itemwin::tabOf(4) == Tab::Gear && itemwin::tabOf(5) == Tab::Gear && itemwin::tabOf(0xD) == Tab::Gear);
    CHECK(itemwin::tabOf(10) == Tab::Gear && itemwin::tabOf(0x11) == Tab::Gear);  // 3.0.11-beta.113: the ammo
    CHECK(itemwin::tabOf(6) == Tab::Cards && itemwin::tabOf(3) == Tab::Etc && itemwin::tabOf(7) == Tab::Etc);
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
    const std::vector<int> potions = itemwin::shownOf(items, itemwin::Tab::Use, "POTION");
    CHECK(potions.size() == 2 && potions[0] == 0 && potions[1] == 2);
    CHECK(itemwin::shownOf(items, itemwin::Tab::Use, "").size() == 2);
    CHECK(itemwin::shownOf(items, itemwin::Tab::Etc, "jell").size() == 1);
    CHECK(itemwin::shownOf(items, itemwin::Tab::Gear, "").size() == 1);
    CHECK(itemwin::lowered("ЗІЛЛЯ Red") == "зілля red");
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(110, 140, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    CHECK(L.w == 500 && L.cells.size() == 40 && L.tabs.size() == 6 && L.x == 110 && L.y == 140);
    CHECK(L.tabs[5].x + L.tabs[5].w <= L.tabsRow.x + L.tabsRow.w);  // 3.0.11-beta.113: six of them in its row
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
    const std::vector<int> shown = itemwin::shownOf(items, itemwin::Tab::Use, "");
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
    CHECK(has("Речі") && has("Alt+E") && has("Витратне") && has("38") && has("1 482 300 z"));
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

// 3.0.11-beta.115 (the windows plan's stage 6, «Склад»): the same grid as the storage - one tip for every thing (a
// double click takes it into the bag, an etc thing too), the places used of the most in the weight's place, no zeny
TEST(item_window_as_the_storage) {
    std::vector<itemwin::Item> items{item(3, 909, 3, 120, "Jellopy"), item(4, 501, 0, 5, "Red Potion")};
    itemwin::Texts t = itemTexts();
    t.title = "Склад";
    t.hotkey = "";
    t.weight = "Місця";
    t.all = "двічі клік — у сумку";
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    itemwin::Show sh;
    sh.hover = 0;
    sh.weight = 57;
    sh.weightMost = 600;
    sh.zeny = -1;
    const std::vector<int> shown = itemwin::shownOf(items, itemwin::Tab::All, "");
    const std::vector<itemwin::Words> w = itemwin::words(L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const itemwin::Words& x) { return x.text == s; });
    };
    CHECK(has("Склад") && has("Місця") && has("57 / 600 \xC2\xB7 9 %"));
    CHECK(has("двічі клік — у сумку \xC2\xB7 ПКМ — опис"));  // Jellopy: an etc thing taken too
    CHECK(std::none_of(w.begin(), w.end(), [](const itemwin::Words& x) {
        return x.text.size() >= 2 && x.text.compare(x.text.size() - 2, 2, " z") == 0;
    }));
    sh.hover = 1;  // a potion: the same words, not «використати»
    const std::vector<itemwin::Words> w2 = itemwin::words(L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
    CHECK(std::any_of(w2.begin(), w2.end(),
                      [](const itemwin::Words& x) { return x.text == "двічі клік — у сумку \xC2\xB7 ПКМ — опис"; }));
}

// 3.0.11-beta.118 (the owner: «нету стрел»; R-2YRWU2): the arrows worn stay in the bag (the game's own window lists
// them) - shown in «Усе» and «Спорядження» with a green «E», the tip saying they are worn
TEST(item_window_shows_the_worn_arrows) {
    std::vector<itemwin::Item> items{item(13, 1750, 10, 101, "Arrow"), item(14, 501, 0, 5, "Red Potion")};
    items[0].worn = true;
    CHECK(itemwin::shownOf(items, itemwin::Tab::All, "").size() == 2);
    CHECK(itemwin::shownOf(items, itemwin::Tab::Gear, "") == std::vector<int>{0});
    itemwin::Texts t = itemTexts();
    t.worn = "вдягнуто";
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    itemwin::Show sh;
    sh.hover = 0;
    const std::vector<int> shown = itemwin::shownOf(items, itemwin::Tab::All, "");
    const std::vector<itemwin::Words> w = itemwin::words(L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
    CHECK(std::any_of(w.begin(), w.end(), [](const itemwin::Words& x) { return x.text == "E" && x.rgb == 0x5FD08A; }));
    CHECK(std::any_of(w.begin(), w.end(), [](const itemwin::Words& x) {
        return x.text.rfind("вдягнуто \xC2\xB7 ", 0) == 0;  // the tip: worn first
    }));
}

// 3.0.11-beta.110 (the concept «Концепция окон и инвентаря», stage 2): a worn thing dragged out of «Спорядження» - the
// bag's grid lit in gold (a drop there takes it off)
TEST(item_window_lights_the_bag_while_a_worn_thing_is_dragged) {
    const std::vector<itemwin::Item> items{item(1, 501, 0, 38, "Red Potion")};
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    const std::vector<int> shown = itemwin::shownOf(items, itemwin::Tab::Use, "");
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

// 3.0.11-beta.112 (the concept «Концепция окон и инвентаря», question 3 «да»; the owner: «я сделал стрелы и они в
// инвентаре не появились»): a thing come into the bag marked «нове» in a gold pill over its cell's corner till it is
// pointed at, its tab's count of the new ones beside its name (the arrows in «Спорядження» since 3.0.11-beta.113)
TEST(item_window_marks_the_new_things) {
    std::vector<itemwin::Item> items{item(1, 501, 0, 38, "Red Potion"), item(2, 1750, 10, 500, "Arrow")};
    items[1].fresh = true;
    itemwin::Texts t = itemTexts();
    t.fresh = "нове";
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    itemwin::Show sh;
    sh.tab = itemwin::Tab::Gear;
    const std::vector<int> shown = itemwin::shownOf(items, itemwin::Tab::Gear, "");
    const std::vector<itemwin::Words> w = itemwin::words(L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
    const panel::Box& cell = L.cells[0];
    const panel::Box& etc = L.tabs[size_t(itemwin::Tab::Gear)];
    bool pill = false, count = false, other = false;
    for (const itemwin::Words& x : w) {
        pill = pill || (x.text == "нове" && x.box.y < cell.y + cell.h / 2 && x.box.x + x.box.w <= cell.x + cell.w + 4);
        count = count || (x.text == "1" && x.box.x >= etc.x + etc.w - 6 && x.box.x <= etc.x + etc.w + 12);
        other = other || (x.text == "1" && x.box.x < etc.x);  // no new ones in the other tabs: no count there
    }
    CHECK(pill && count && !other);
    items[1].fresh = false;  // pointed at
    const std::vector<itemwin::Words> seen =
        itemwin::words(L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
    CHECK(std::none_of(seen.begin(), seen.end(), [](const itemwin::Words& x) { return x.text == "нове"; }));
}

// 3.0.11-beta.113 (the concept «Концепция окон и инвентаря», question 9 «давай»): six tabs - «Усе», «Витратне»,
// «Спорядження» (the ammo there too), «Карти», «Різне», «★» (the starred ones); a cell's lower left corner the star:
// pressed, its thing starred or not (☆ while pointed at, ★ starred)
TEST(item_window_six_tabs_and_the_starred) {
    using itemwin::Tab;
    std::vector<itemwin::Item> items{item(1, 501, 0, 38, "Red Potion"), item(2, 1750, 10, 500, "Arrow"),
                                     item(3, 4001, 6, 1, "Poring Card"), item(4, 909, 3, 20, "Jellopy")};
    items[3].starred = true;
    CHECK(itemwin::shownOf(items, Tab::All, "").size() == 4);
    CHECK(itemwin::shownOf(items, Tab::Use, "") == std::vector<int>({0}));
    CHECK(itemwin::shownOf(items, Tab::Gear, "") == std::vector<int>({1}));
    CHECK(itemwin::shownOf(items, Tab::Cards, "") == std::vector<int>({2}));
    CHECK(itemwin::shownOf(items, Tab::Etc, "") == std::vector<int>({3}));
    CHECK(itemwin::shownOf(items, Tab::Starred, "") == std::vector<int>({3}));
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    const panel::Box& c = L.cells[3];
    const itemwin::Hit star = itemwin::hitTest(L, 4, 0, c.x + 6, c.y + c.h - 6);
    CHECK(star.part == itemwin::Part::Star && star.index == 3);
    CHECK(itemwin::hitTest(L, 4, 0, c.x + c.w / 2, c.y + c.h / 2).part == itemwin::Part::Cell);
    CHECK(itemwin::hitTest(L, 2, 0, c.x + 6, c.y + c.h - 6).part == itemwin::Part::Grid);  // no thing there: none
    itemwin::Show sh;
    sh.tab = Tab::All;
    sh.hover = 0;
    const std::vector<int> shown = itemwin::shownOf(items, Tab::All, "");
    const std::vector<itemwin::Words> w = itemwin::words(L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
    const auto at = [&](const std::string& text, const panel::Box& cell) {
        return std::any_of(w.begin(), w.end(), [&](const itemwin::Words& x) {
            return x.text == text && x.box.x >= cell.x && x.box.x < cell.x + cell.w / 2 && x.box.y > cell.y;
        });
    };
    CHECK(at("\xE2\x98\x85", L.cells[3]) && at("\xE2\x98\x86", L.cells[0]));  // ★ starred, ☆ pointed at
    CHECK(!at("\xE2\x98\x86", L.cells[1]) && !at("\xE2\x98\x85", L.cells[1]));  // neither: none
}

// 3.0.11-beta.196 (the tester: «ітему має бути можливість тицьнути зірочку - але on hover зникає занадто рано, і до
// зірочки мишку підвести не можливо»): the pointer on a cell's star still points at the cell - its ☆ stays to be
// pressed
TEST(item_window_star_keeps_its_cell_pointed) {
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    const panel::Box& c = L.cells[2];
    CHECK(itemwin::pointedCell(itemwin::hitTest(L, 4, 0, c.x + 6, c.y + c.h - 6)) == 2);  // on its star
    CHECK(itemwin::pointedCell(itemwin::hitTest(L, 4, 0, c.x + c.w / 2, c.y + c.h / 2)) == 2);  // on the cell
    CHECK(itemwin::pointedCell(itemwin::hitTest(L, 2, 0, c.x + c.w / 2, c.y + c.h / 2)) == -1);  // an empty one
    CHECK(itemwin::pointedCell(itemwin::Hit{itemwin::Part::Tab, 2}) == -1);
}

// 3.0.11-beta.113: the starred things kept per character (uaro-graphics\items-starred.txt: its name, a tab, the
// items' numbers)
TEST(item_window_starred_kept_per_character) {
    std::string text = itemwin::withStarred("", "Korel", {1750, 501});
    text = itemwin::withStarred(text, "tester", {909});
    CHECK(itemwin::starredOf(text, "Korel") == std::set<int>({501, 1750}));
    CHECK(itemwin::starredOf(text, "tester") == std::set<int>({909}));
    text = itemwin::withStarred(text, "Korel", {});  // none left: its line gone
    CHECK(itemwin::starredOf(text, "Korel").empty() && itemwin::starredOf(text, "tester").size() == 1);
    CHECK(text.find("Korel") == std::string::npos);
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
                                       "каст коротший: 56 → 56,7 %");
    // 3.0.11-beta.202 (the tester: «різний час касту при наведенні на дексу і у списку»): the list's cast as the
    // hint's - to a tenth, a whole number without its «,0»
    const auto castRow = [&](const statwin::Data& x) {
        for (const statwin::Row& r : statwin::battleRows(x, t))
            if (r.name == t.cast) return r.value;
        return std::string();
    };
    CHECK(castRow(d) == "\xE2\x88\x92" "56 %");
    d.bonus[4] = 6;
    d.base[4] = 61;  // DEX 67, as the tester's
    CHECK(castRow(d) == "\xE2\x88\x92" "44,7 %");
    CHECK(statwin::explain(d, 4, t).find("каст коротший: 44,7 → 45,3 %") != std::string::npos);
    d.base[4] = 74;
    d.bonus[4] = 10;
    CHECK(statwin::explain(d, 3, t) == "INT 99 — максимум");
    CHECK(statwin::explain(d, 0, t) == "STR 1 → 2 коштує 2, у вас 3 \xC2\xB7 ATK +1, вантаж +30");  // 11 -> 12
    d.base[0] = 9;
    d.bonus[0] = 0;
    CHECK(statwin::explain(d, 0, t).find("ATK +2,") != std::string::npos);  // 9 -> 10: 1 + 1
    CHECK(statwin::explain(d, -1, t) == "наведіть на стат");
    const std::vector<statwin::Row> rows = statwin::battleRows(d, t);
    CHECK(rows.size() == 14 && rows[3].value == "132 + 1" && rows[8].name == "Час касту");  // .266: 14
    CHECK(rows[9].name == "Затримка після вміння" && rows[13].name == "Вага");
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

// 3.0.11-beta.139 (the owner: «пишет что 60% но хп воостанавливается» - uaRO stops the natural regen at 70 %): the
// weight's bar coloured by the server's own overweight state (green none, gold no regen, red the 90 % one); not known
// yet: by the classic 50 % and 90 % as before
TEST(item_window_weight_bar_by_the_servers_state) {
    std::vector<itemwin::Item> items{item(1, 501, 0, 38, "Red Potion")};
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    const std::vector<int> shown = itemwin::shownOf(items, itemwin::Tab::All, "");
    itemwin::Show sh;
    sh.weight = 2700;
    sh.weightMost = 4500;  // 60 %
    const auto barColour = [&](int overweight) {
        sh.overweight = overweight;
        panel::Picture p;
        p.width = L.w;
        p.height = L.h;
        p.px.assign(size_t(L.w) * size_t(L.h), 0u);
        itemwin::paint(p, L, items, shown, sh, t, itemwin::Measure(panel::wordsWidth));
        return p.px[size_t(L.bar.y + L.bar.h / 2) * size_t(L.w) + size_t(L.bar.x + 4)];
    };
    const auto g = [](uint32_t c) { return int((c >> 8) & 0xFF); };
    const auto r = [](uint32_t c) { return int((c >> 16) & 0xFF); };
    const uint32_t none = barColour(0), regen = barColour(1), ninety = barColour(2), unknown = barColour(-1);
    CHECK(g(none) > r(none));      // green: the server says none at 60 %
    CHECK(r(regen) > g(regen) && g(regen) > 0x90);  // gold
    CHECK(r(ninety) > 0xD0 && g(ninety) < 0x90);    // red
    CHECK(unknown == regen);       // not known: 60 % by the classic 50 %
}

// 3.0.11-beta.266 (the «Характеристики» concept, stage 2): the panel «Як рахується» on the concept's knight - level
// 98, a two-handed sword, AGI 76 and DEX 43 - and the archer of R-VZ69Q2 (HP 610)
TEST(stat_window_explained_formulas) {
    const statwin::Texts t = statTexts();
    statwin::Data d;
    const int base[6] = {62, 70, 30, 1, 40, 1}, bonus[6] = {4, 6, 2, 0, 3, 1};
    for (int i = 0; i < 6; ++i) {
        d.base[i] = base[i];
        d.bonus[i] = bonus[i];
        d.need[i] = 2;
    }
    d.job = 7;
    d.baseLevel = 98;
    d.jobLevel = 50;
    d.aspd = 164;
    d.atk = 260;
    d.atk2 = 7;
    d.hit = 141;
    d.hpMost = 10319;
    d.weight = 1710;
    d.weightMost = 4780;
    CHECK_EQ(statwin::weaponOf(d), 3);  // ASPD 164 with these stats: the knight's two-handed sword
    const statwin::Explained aspd = statwin::explained(d, statwin::kBattle + 7, t);
    CHECK(aspd.title == "Як рахується · ASPD" && aspd.lines.size() == 4);
    CHECK(aspd.lines[0] == "Knight із Two-Handed Sword: затримка 550");
    CHECK(aspd.lines[1].find("= 190 \xE2\x86\x92 360") != std::string::npos);
    CHECK(aspd.lines[2] == "1,4 удару на секунду");
    CHECK(aspd.lines[3].rfind("до ASPD 165 потрібно AGI ", 0) == 0);
    const statwin::Explained atk = statwin::explained(d, statwin::kBattle, t);
    CHECK(atk.lines[0] == "STR 66 + (66/10)\xC2\xB2 36 + DEX 43/5 8 + LUK 2/5 0 = 110");
    CHECK(atk.lines[1] == "ATK зброї: 150 \xC2\xB7 заточка: + 7");
    CHECK(statwin::explained(d, statwin::kBattle + 2, t).lines[1] == "сервер: 141 — збігається");
    CHECK(statwin::explained(d, statwin::kBattle + 10, t).lines[1] == "сервер: 10319 — збігається");
    const statwin::Explained weight = statwin::explained(d, statwin::kBattle + 13, t);
    CHECK(weight.lines[0] == "2800 + 30 \xC3\x97 STR 62 = 4660" && weight.lines[2] == "70 %: 3346 · 90 %: 4302");
    CHECK(weight.lines[1] == "сервер: 4780 — статами 4660, речі +120");
    CHECK(statwin::explained(d, -1, t).title == "Наведіть на рядок" && statwin::explained(d, -1, t).lines.empty());
    CHECK(statwin::explained(d, 0, t).title == "Як рахується · STR");
    d.aspd = 170;  // sped up by a skill: no weapon of the knight gives it
    CHECK(statwin::weaponOf(d) == -1);
    CHECK(statwin::explained(d, statwin::kBattle + 7, t).lines[0] == "зброю не визначено");
    statwin::Show sh;
    sh.pinnedBattle = 7;
    CHECK_EQ(statwin::explainedItem(sh), statwin::kBattle + 7);
    sh.hoverRow = 2;  // the mouse over a stat tells of it, the pinned row back after
    CHECK_EQ(statwin::explainedItem(sh), 2);
    const statwin::Layout L = statwin::layout(0, 0, 1.0f, d, t, statwin::Measure(panel::wordsWidth));
    CHECK(L.battleRows.size() == 14);
    const statwin::Hit b = statwin::hitTest(L, L.battleRows[7].x + 20, L.battleRows[7].y + 5);
    CHECK(b.part == statwin::Part::Battle && b.index == 7);
    CHECK(L.line.y >= L.battle.y + L.battle.h && L.h >= int(L.line.y + L.line.h));
}

// 3.0.11-beta.267 (stage 3): «Довідка» - the knight's weapons' ASPD, the Swordman's STR levels; the tabs and the
// class's arrows pointed at
TEST(stat_window_reference_tab) {
    const std::vector<statwin::AspdRow> rows = statwin::aspdTable(7, 76, 43);
    CHECK_EQ(rows.size(), size_t(10));  // the knight's ten: the fist to the two-handed mace
    const statwin::AspdRow& sword = rows[3];
    CHECK(sword.weapon == 3 && sword.delay == 550 && sword.mine == 164);
    CHECK(sword.at[0] == 147 && sword.at[3] == 169);
    CHECK(std::string(statwin::weaponName(3)) == "Two-Handed Sword" && std::string(statwin::weaponName(99)).empty());
    CHECK(statmath::bonusLevels(1, statmath::Str) == std::vector<int>({2, 14, 33, 40, 47, 49, 50}));
    CHECK(!statmath::jobs().empty() && statmath::jobs().front() == 0);
    statwin::Texts t = statTexts();
    t.tabs[0] = "Персонаж";
    t.tabs[1] = "Довідка";
    statwin::Data d;
    d.job = 7;
    const statwin::Layout L =
        statwin::layout(0, 0, 1.0f, d, t, statwin::Measure(panel::wordsWidth), statwin::kRefTab, 1);
    CHECK(L.tab == statwin::kRefTab && L.refJob == 1 && L.rows.empty() && L.battleRows.empty());
    const statwin::Hit tab = statwin::hitTest(L, L.tabs[0].x + 5, L.tabs[0].y + 5);
    CHECK(tab.part == statwin::Part::Tab && tab.index == 0);
    const statwin::Hit next = statwin::hitTest(L, L.next.x + 5, L.next.y + 5);
    CHECK(next.part == statwin::Part::Arrow && next.index == 1);
    const statwin::Layout P = statwin::layout(0, 0, 1.0f, d, t, statwin::Measure(panel::wordsWidth));
    CHECK(P.h == L.h && P.refJob == 7 && P.rows.size() == 6);  // one height for both tabs
    bool swordsman = false;
    for (const statwin::Words& w : statwin::words(L, d, statwin::Show{}, t, statwin::Measure(panel::wordsWidth)))
        swordsman = swordsman || w.text == "Swordsman";
    CHECK(swordsman);
}

// 3.0.11-beta.268 (stage 4): «Планування» on the concept's knight - level 98 -> 99, AGI 70 -> 73, DEX 40 -> 41
TEST(stat_window_plan) {
    statwin::Texts t = statTexts();
    t.tabs[1] = "Планування";
    statwin::Data d;
    const int base[6] = {62, 70, 30, 1, 40, 1}, bonus[6] = {4, 6, 2, 0, 3, 1};
    for (int i = 0; i < 6; ++i) {
        d.base[i] = base[i];
        d.bonus[i] = bonus[i];
    }
    d.job = 7;
    d.baseLevel = 98;
    d.free = 12;
    d.aspd = 164;
    d.hit = 141;
    d.flee = 174;
    d.atk = 260;
    d.hpMost = 10319;
    d.spMost = 300;
    d.weightMost = 4660;
    statwin::Plan p = statwin::planFrom(d);
    CHECK(p.level == 98 && p.base[1] == 70);
    CHECK(statwin::planUsed(d, p) == 0 && statwin::planAvailable(d, p) == 12);
    p.level = 99;
    p.base[1] = 73;
    p.base[4] = 41;
    CHECK_EQ(statwin::planUsed(d, p), 26 + 5);
    CHECK_EQ(statwin::planAvailable(d, p), 12 + statmath::pointsAt(99) - statmath::pointsAt(98));
    const std::vector<statwin::PlanRow> rows = statwin::planRows(d, p, t);
    const auto find = [&](const std::string& name) {
        for (const statwin::PlanRow& r : rows)
            if (r.name == name) return r;
        return statwin::PlanRow{};
    };
    CHECK(find("HIT").then == "143" && find("FLEE").then == "178" && find("ASPD").then == "164,8");
    CHECK(find("ATK").changed == false && find("Max HP").then != "10319");
    const statwin::Layout L = statwin::layout(0, 0, 1.0f, d, t, statwin::Measure(panel::wordsWidth), statwin::kPlanTab);
    CHECK(L.planMinus.size() == 6 && L.rows.empty());
    const statwin::Hit plus = statwin::hitTest(L, L.planPlus[1].x + 4, L.planPlus[1].y + 4);
    CHECK(plus.part == statwin::Part::PlanPlus && plus.index == 1);
    CHECK(statwin::hitTest(L, L.levelNext.x + 4, L.levelNext.y + 4).part == statwin::Part::PlanLevel);
}

// 3.0.11-beta.269 (stage 5): the goals of the concept's knight - ASPD 165 and 170 within reach, 185 not; the cast
TEST(stat_window_plan_goals) {
    statwin::Texts t = statTexts();
    t.goalAspd = "ASPD {1}: AGI {2} — ще {3} очок";
    t.goalAspdNever = "ASPD {1}: AGI {2}, більше за 99";
    t.goalAspdNone = "ASPD: зброю не визначено";
    t.goalCast = "Без касту: DEX {1} — ще {2} очок";
    t.goalCastDone = "Без касту: уже";
    t.goalCastNever = "Без касту: DEX {1}, більше за 99 — ще {2}";
    statwin::Data d;
    const int base[6] = {62, 70, 30, 1, 40, 1}, bonus[6] = {4, 6, 2, 0, 3, 1};
    for (int i = 0; i < 6; ++i) {
        d.base[i] = base[i];
        d.bonus[i] = bonus[i];
    }
    d.job = 7;
    d.baseLevel = 98;
    d.aspd = 164;
    const statwin::Plan p = statwin::planFrom(d);
    const std::vector<std::string> g = statwin::planGoals(d, p, 165, t);
    CHECK(g.size() == 2 && g[0].rfind("ASPD 165: AGI ", 0) == 0);
    // ASPD 170: AGI 103 in all, the base 97 with the bonus of 6 - within reach; 185 past the base's 99
    CHECK(statwin::planGoals(d, p, 170, t)[0] == "ASPD 170: AGI 103 — ще 264 очок");
    CHECK(statwin::planGoals(d, p, 185, t)[0].find("більше за 99") != std::string::npos);
    CHECK(g[1] == "Без касту: DEX 150, більше за 99 — ще 48");  // DEX 3 from bonuses: 147 base needed
    d.aspd = 175;  // sped up: no weapon
    CHECK(statwin::planGoals(d, p, 180, t)[0] == "ASPD: зброю не визначено");
}

// 3.0.11-beta.296: the map's monsters now and with the plan - more DEX hits a Poring no worse, more AGI dodges more;
// a monster not of the server's is left out; the block below the goals within the window
TEST(stat_window_plan_mobs) {
    statwin::Data d;
    const int base[6] = {40, 30, 20, 1, 20, 1};
    for (int i = 0; i < 6; ++i) d.base[i] = base[i];
    d.job = 3;
    d.baseLevel = 30;
    d.hit = 50;
    d.flee = 60;
    d.flee2 = 1;
    d.atk = 60;
    d.def2 = 20;
    d.map = "prt_fild08";
    d.mapMobs = {statwin::MapMob{1002, 1, 50, "Poring"}, statwin::MapMob{999999, 5, 10, "None"}};
    statwin::Plan p = statwin::planFrom(d);
    std::vector<statwin::MobRow> rows = statwin::planMobs(d, p);
    CHECK(rows.size() == 1 && rows[0].name == "Poring" && rows[0].hitNow == rows[0].hitThen);
    CHECK(rows[0].dodgeNow == rows[0].dodgeThen && rows[0].hitsNow == rows[0].hitsThen);
    p.base[1] = 60;
    p.base[0] = 60;
    rows = statwin::planMobs(d, p);
    CHECK(rows[0].dodgeThen >= rows[0].dodgeNow && rows[0].hitsThen <= rows[0].hitsNow);
    statwin::Texts t = statTexts();
    const statwin::Layout L = statwin::layout(0, 0, 1.0f, d, t, statwin::Measure(panel::wordsWidth), statwin::kPlanTab);
    CHECK(L.mobs.y > L.goals.y + L.goals.h && L.mobs.y + L.mobs.h <= L.glass.h);
}

// 3.0.11-beta.296: the plans kept by character - written and read back, a broken line left out
TEST(stat_window_plans_kept) {
    std::map<std::string, statwin::SavedPlan> plans;
    statwin::SavedPlan a;
    a.plan.level = 77;
    const int base[6] = {90, 50, 30, 1, 40, 5};
    for (int i = 0; i < 6; ++i) a.plan.base[i] = base[i];
    a.aspdGoal = 170;
    plans["Анна"] = a;
    plans["Bob 2"] = statwin::SavedPlan{};
    plans["Bob 2"].plan.level = 10;
    for (int i = 0; i < 6; ++i) plans["Bob 2"].plan.base[i] = 1;
    const std::string text = statwin::plansText(plans);
    std::map<std::string, statwin::SavedPlan> back = statwin::parsePlans(text + "zz 1 2\nabc 5 1 1 1 1 1 1 0\n");
    CHECK(back.size() == 2 && back.count("Анна") && back.count("Bob 2"));
    CHECK(back["Анна"].plan.level == 77 && back["Анна"].plan.base[0] == 90 && back["Анна"].aspdGoal == 170);
    CHECK(statwin::plansText(back) == text);
}

// 3.0.11-beta.297 (the owner: «в планировании без учетов скилов типа impove consentration»): the plan's buffs -
// Improve Concentration on now scales with the planned AGI, off takes its part away, Blessing adds DEX; kept
TEST(stat_window_plan_buffs) {
    statwin::Data d;
    const int base[6] = {1, 60, 20, 1, 50, 1}, bonus[6] = {0, 9, 0, 0, 8, 0};
    for (int i = 0; i < 6; ++i) {
        d.base[i] = base[i];
        d.bonus[i] = bonus[i];
    }
    d.job = 3;  // an archer
    d.baseLevel = 50;
    d.jobLevel = 1;
    d.icLevel = 10;
    d.buffNow[statwin::kBuffConcentration] = true;  // 12 % of AGI 60 and DEX 50: +7, +6 in the bonus
    statwin::Plan p = statwin::planFrom(d);
    int t[6];
    statwin::planTotals(d, p, t);
    CHECK(t[1] == 69 && t[4] == 58);  // as now
    p.base[1] = 85;
    statwin::planTotals(d, p, t);
    CHECK_EQ(t[1], 85 + 2 + 10);  // the other +2 kept, 12 % of 85 = 10
    p.buffs[statwin::kBuffConcentration] = 0;
    statwin::planTotals(d, p, t);
    CHECK(t[1] == 87 && t[4] == 52);
    p.buffs[statwin::kBuffBlessing] = 1;
    statwin::planTotals(d, p, t);
    CHECK(t[4] == 62 && t[0] == 11);
    CHECK(!statwin::planBuffOffered(statwin::Data{}, statwin::kBuffConcentration));
    std::map<std::string, statwin::SavedPlan> plans;
    plans["A"] = statwin::SavedPlan{p, 0};
    const auto back = statwin::parsePlans(statwin::plansText(plans));
    CHECK(back.at("A").plan.buffs[0] == 0 && back.at("A").plan.buffs[1] == 1 && back.at("A").plan.buffs[2] == -1);
    CHECK(statwin::parsePlans("41 50 1 1 1 1 1 1 0\n").at("A").plan.buffs[0] == -1);  // .296's line
}
