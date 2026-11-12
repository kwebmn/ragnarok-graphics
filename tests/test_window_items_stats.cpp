// 3.0.11-beta.42: the windows «Речі» (core/item_window) and «Характеристики» (core/stat_window) - their tabs and
// uses as the game's, the search, their places and the mouse, the stats' prices and what a point gives
#include <algorithm>
#include <cmath>
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
    d.learned[statwin::kBuffConcentration] = 10;
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

// 3.0.11-beta.298 (the owner: «делай для всех классов»): a Concentration Potion on now - the bow still found by the
// server's ASPD; the plan without it slower, with Berserk Potion faster; Two-Hand Quicken offered to a knight alone,
// Gloria to everyone; True Sight's HIT in the plan's rows
TEST(stat_window_plan_class_buffs) {
    statwin::Data d;
    const int base[6] = {1, 70, 20, 1, 60, 1};
    for (int i = 0; i < 6; ++i) d.base[i] = base[i];
    d.job = 3;  // an archer with a bow (11)
    d.baseLevel = 60;
    d.jobLevel = 1;
    d.hit = 100;
    d.buffNow[14] = true;  // Concentration Potion
    d.aspd = statmath::aspd10(statmath::amotionRated(3, 11, 70, 60, 900)) / 10;
    CHECK_EQ(statwin::weaponOf(d), 11);
    statwin::Plan p = statwin::planFrom(d);
    CHECK_EQ(statwin::aspdRate(d, 11, true, &p), 900);
    p.buffs[14] = 0;
    CHECK_EQ(statwin::aspdRate(d, 11, true, &p), 1000);
    p.buffs[16] = 1;
    CHECK_EQ(statwin::aspdRate(d, 11, true, &p), 800);
    p.buffs[15] = 1;  // the best potion alone
    CHECK_EQ(statwin::aspdRate(d, 11, true, &p), 800);
    CHECK(!statwin::planBuffOffered(d, 10) && statwin::planBuffOffered(d, 3));
    d.learned[10] = 10;
    CHECK(statwin::planBuffOffered(d, 10));
    CHECK_EQ(statwin::aspdRate(d, 11, true, &p), 800);  // Two-Hand Quicken not with a bow
    d.learned[8] = 10;  // True Sight
    p.buffs[8] = 1;
    statwin::Texts t = statTexts();
    for (const statwin::PlanRow& r : statwin::planRows(d, p, t))
        if (r.name == "HIT") CHECK_EQ(r.then, std::to_string(100 + 5 + 30));
}

// 3.0.16-beta.17 (R-PTDFCY): a pet's accessory (gear with no place on the body, 10001-10020) worn on a double click as
// the game's own window does; another gear with no place is not
TEST(items_a_pets_accessory_is_worn) {
    itemwin::Item acc;
    acc.index = 5;
    acc.type = 8;
    acc.id = 10006;
    acc.location = 0;
    CHECK(itemwin::useOf(acc) == itemwin::Use::Wear);
    acc.identified = false;
    CHECK(itemwin::useOf(acc) == itemwin::Use::None);
    itemwin::Item other = acc;
    other.identified = true;
    other.id = 2301;
    CHECK(itemwin::useOf(other) == itemwin::Use::None);
}

// 3.0.18-beta.204 (a tester: «а скрол еще не працює» - «Склад»'s bar was only drawn): the scroll bar beside the grid
// - its strip from the grid's right edge, the thumb in it as the bar draws it - taken by the mouse
TEST(item_window_scroll_bar_taken_by_the_mouse) {
    const itemwin::Texts t = itemTexts();
    const itemwin::Layout L = itemwin::layout(0, 0, 1.0f, t, itemwin::Measure(panel::wordsWidth));
    CHECK_EQ(itemwin::scrollBar(L, 40, 0).track.w, 0.0f);  // all in sight: no bar
    CHECK(itemwin::hitTest(L, 40, 0, L.grid.x + L.grid.w + 6, L.grid.y + 10).part != itemwin::Part::Scroll);
    // 100 things, 13 rows, 5 in sight: the thumb 5/13 of the grid's height, at the top, then at the bottom
    const itemwin::ScrollBar top = itemwin::scrollBar(L, 100, 0);
    CHECK(top.track.x >= L.grid.x + L.grid.w && top.track.x + top.track.w <= L.glass.x + L.glass.w);
    CHECK(top.track.y == L.grid.y && top.track.h == L.grid.h);
    CHECK(std::fabs(top.thumb.h - L.grid.h * 5 / 13) < 0.5f);
    CHECK(std::fabs(top.thumb.y - L.grid.y) < 0.5f);
    const itemwin::ScrollBar bottom = itemwin::scrollBar(L, 100, 8);
    CHECK(std::fabs(bottom.thumb.y + bottom.thumb.h - (L.grid.y + L.grid.h)) < 0.5f);
    // its strip a part of its own, no cell under it
    const itemwin::Hit h = itemwin::hitTest(L, 100, 0, top.track.x + top.track.w / 2, L.grid.y + L.grid.h / 2);
    CHECK(h.part == itemwin::Part::Scroll);
    for (const panel::Box& c : L.cells) CHECK(c.x + c.w <= top.track.x);
    // the scroll for a place along it: the thumb's top where the pointer is less where it was taken; past the ends
    const float room = L.grid.h - top.thumb.h;  // the thumb's way: 8 rows along it
    CHECK_EQ(itemwin::scrollFor(L, 100, L.grid.y - 50, 0), 0);
    CHECK_EQ(itemwin::scrollFor(L, 100, L.grid.y + L.grid.h + 50, 0), 8);
    CHECK_EQ(itemwin::scrollFor(L, 100, L.grid.y + room / 2 + 3, 3), 4);
    CHECK_EQ(itemwin::scrollFor(L, 40, L.grid.y + 100, 0), 0);  // nothing to scroll
    // pressed on the thumb: kept, then dragged along; let go
    itemwin::BarDrag drag;
    int scroll = 0;
    CHECK(drag.press(L, 100, scroll, top.thumb.y + 4));
    CHECK(drag.on);
    CHECK_EQ(scroll, 0);
    drag.move(L, 100, scroll, top.thumb.y + 4 + room / 2);
    CHECK_EQ(scroll, 4);
    drag.release();
    CHECK(!drag.on);
    drag.move(L, 100, scroll, L.grid.y);  // not held: nothing
    CHECK_EQ(scroll, 4);
    // pressed on the strip under the thumb: it jumps there, its middle under the pointer
    scroll = 0;
    CHECK(drag.press(L, 100, scroll, L.grid.y + L.grid.h - 1));
    CHECK_EQ(scroll, 8);
    drag.release();
    CHECK(!drag.press(L, 40, scroll, L.grid.y + 10));  // nothing to scroll: not taken
}

// 3.0.18-beta.224 (R-5K5NRW «при спробі вдягнути ітем, до якого не вистачає лвла - пише що успіх, але не вдягається»:
// «вдягнуто» rose over the cell at the double click, before the server answered): the word waits for the answer - the
// thing gone from the bag (or its arrows marked worn) is worn, still there after a while is refused
TEST(items_wear_answered_not_assumed) {
    itemwin::WearWatch w;
    w.asked(5, false, 1000);
    CHECK(w.check([](int) { return true; }, [](int) { return false; }, 1100) == itemwin::WearWatch::Answer::None);
    CHECK(w.check([](int) { return true; }, [](int) { return false; }, 2600) == itemwin::WearWatch::Answer::Refused);
    CHECK(w.check([](int) { return true; }, [](int) { return false; }, 2700) == itemwin::WearWatch::Answer::None);
    w.asked(7, false, 3000);  // a sash put on: gone from the bag
    CHECK(w.check([](int i) { return i != 7; }, [](int) { return false; }, 3200) == itemwin::WearWatch::Answer::Worn);
    w.asked(9, true, 4000);  // arrows: they stay in the bag, marked worn
    CHECK(w.check([](int) { return true; }, [](int) { return false; }, 4100) == itemwin::WearWatch::Answer::None);
    CHECK(w.check([](int) { return true; }, [](int i) { return i == 9; }, 4200) == itemwin::WearWatch::Answer::Worn);
}
