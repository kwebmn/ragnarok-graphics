// 3.0.11-beta.38: the target's card (core/target_card) - its place at the top middle, its details, the mouse over
// it, what hurts and heals a monster by the server's element table
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/reference_pages.hpp"
#include "core/target_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool inside(const panel::Box& outer, const panel::Box& inner) {
    return inner.x >= outer.x - 1e-3f && inner.y >= outer.y - 1e-3f && inner.x + inner.w <= outer.x + outer.w + 1e-3f &&
           inner.y + inner.h <= outer.y + outer.h + 1e-3f;
}

targetcard::Data zombie() {
    targetcard::Data d;
    d.mob = 1015;
    d.name = "Zombie";
    d.level = 15;
    d.element = "Undead 1";
    d.elementRgb = 0x8C7BB5;
    d.kind = "Undead \xC2\xB7 Medium";
    d.numbers = "HP 534 \xC2\xB7 Base 50 \xC2\xB7 Job 33";
    d.weak = "Holy 150 %";
    d.heals = "Poison, Dark";
    return d;
}

targetcard::Texts texts() { return targetcard::Texts{"Слабкий до: {1}", "Лікують: {1}", "Картка в довіднику"}; }

}  // namespace

TEST(target_card_weakness) {
    // the server's table: each named weak element hurts it as much as named, from 150 %, the most first; the healing
    // ones below 0
    for (const int element : {0, 3, 7, 9})
        for (int level = 1; level <= 4; ++level) {
            const targetcard::Weakness w = targetcard::weakness(element, level, 10);
            for (size_t i = 0; i < w.weak.size(); ++i) {
                CHECK(w.weak[i].second >= 150 && refpages::elementDamage(w.weak[i].first, element, level) ==
                                                     w.weak[i].second);
                if (i) CHECK(w.weak[i].second <= w.weak[i - 1].second);
            }
            for (const int h : w.heals) CHECK(refpages::elementDamage(h, element, level) < 0);
            int strong = 0, healing = 0;
            for (int a = 0; a < 10; ++a) {
                strong += refpages::elementDamage(a, element, level) >= 150;
                healing += refpages::elementDamage(a, element, level) < 0;
            }
            CHECK(int(w.weak.size()) == strong && int(w.heals.size()) == healing);
        }
    const targetcard::Weakness undead4 = targetcard::weakness(9, 4);  // the concept's Wraith: fire and holy 200 %
    CHECK(undead4.weak.size() >= 2 && undead4.weak[0].second >= 200);
    CHECK(targetcard::weakness(9, 4, 1).weak.size() == 1);  // at most `most`
}

TEST(target_card_layout) {
    const targetcard::Data d = zombie();
    const targetcard::Layout L = targetcard::layout(d, texts(), 960, 10, 1.0f, false);
    CHECK(L.y == 10 && std::abs((L.x + int(L.card.x + L.card.w / 2)) - 960) <= 1);  // its middle at the screen's
    CHECK(L.card.h >= 44 && L.card.h <= 60 && L.details.w == 0);
    CHECK(inside(L.card, L.round) && inside(L.card, L.book) && L.book.x > L.element.x + L.element.w);
    CHECK(L.name.x > L.round.x + L.round.w && L.level.x > L.name.x && L.element.x > L.level.x);
    const targetcard::Layout D = targetcard::layout(d, texts(), 960, 10, 1.0f, true);
    CHECK(D.details.w > 0 && D.details.y >= D.card.y + D.card.h && D.h > L.h);
    const targetcard::Layout L2 = targetcard::layout(d, texts(), 1920, 20, 2.0f, false);
    CHECK(std::fabs(L2.card.h - 2 * L.card.h) < 1.0f && L2.y == 20);
    // the mouse: its book, the rest of it, nothing round it
    CHECK(targetcard::hitTest(L, L.book.x + L.book.w / 2, L.book.y + L.book.h / 2) == targetcard::Part::Book);
    CHECK(targetcard::hitTest(L, L.name.x + 2, L.name.y + 2) == targetcard::Part::Card);
    CHECK(targetcard::hitTest(L, L.card.x + L.card.w + 3, L.card.y + 3) == targetcard::Part::None);
    CHECK(targetcard::hitTest(D, D.details.x + 3, D.details.y + 3) == targetcard::Part::None);
}

TEST(target_card_picture_and_words) {
    const targetcard::Data d = zombie();
    const targetcard::Layout L = targetcard::layout(d, texts(), 300, 0, 1.0f, true);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    targetcard::paint(p, L, d, false);
    const auto at = [&](float x, float y) { return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))]; };
    CHECK((at(L.card.x + L.card.w / 2, L.card.y + 3) >> 24) > 0xC0);  // the card's glass
    CHECK((at(L.details.x + 6, L.details.y + L.details.h / 2) >> 24) > 0xC0);  // the details' ground
    const uint32_t chip = at(L.element.x + 3, L.element.y + L.element.h / 2);  // the element's chip, its colour's
    CHECK(((chip >> 16) & 0xFF) > ((chip >> 8) & 0xFF));
    const std::vector<targetcard::Words> w = targetcard::words(L, d, texts(), true);
    const auto has = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const targetcard::Words& x) { return x.text == text; }) != w.end();
    };
    CHECK(has("Zombie") && has("Lv 15") && has("Undead 1") && has("Undead \xC2\xB7 Medium"));
    CHECK(has("HP 534 \xC2\xB7 Base 50 \xC2\xB7 Job 33") && has("Слабкий до: Holy 150 %") &&
          has("Лікують: Poison, Dark") && has("Картка в довіднику"));
    const std::vector<targetcard::Words> quiet = targetcard::words(targetcard::layout(d, texts(), 300, 0, 1.0f, false),
                                                                   d, texts(), false);
    const auto book = [](const targetcard::Words& x) { return x.text == "Картка в довіднику"; };
    CHECK(std::none_of(quiet.begin(), quiet.end(), book));
}

// 3.0.11-beta.68 (the concept's stage 7: «карточка цели со строкой «Ваша панель»»): the hotbar's skills on it in its
// details - after what heals it, before the click's line; none known, no line
TEST(target_card_your_bar) {
    targetcard::Data d = zombie();
    d.bar = "Fire Bolt \xC3\x97" "2 \xC2\xB7 Jupitel Thunder \xC3\x97" "1";
    targetcard::Texts t = texts();
    t.bar = "Ваша панель: {1}";
    const targetcard::Layout L = targetcard::layout(d, t, 960, 10, 1.0f, true);
    const std::vector<targetcard::Words> w = targetcard::words(L, d, t, false);
    const auto find = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const targetcard::Words& x) { return x.text == text; });
    };
    const auto bar = find("Ваша панель: " + d.bar), heals = find("Лікують: Poison, Dark"),
               book = find("Картка в довіднику");
    CHECK(bar != w.end() && heals != w.end() && book != w.end());
    if (bar == w.end() || heals == w.end() || book == w.end()) return;
    CHECK(inside(L.details, bar->box) && bar->box.y > heals->box.y && bar->box.y < book->box.y);
    CHECK(L.details.w >= targetcard::layout(zombie(), texts(), 960, 10, 1.0f, true).details.w);
    d.bar.clear();
    for (const targetcard::Words& x : targetcard::words(targetcard::layout(d, t, 960, 10, 1.0f, true), d, t, false))
        CHECK(x.text.rfind("Ваша панель", 0) != 0);
}

// 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): played with the controller - the card's end
// says its buttons: A attacks, the right stick another target
TEST(target_card_pad_hints) {
    targetcard::Data d = zombie();
    targetcard::Texts t = texts();
    t.attack = "Атака";
    t.next = "Інша ціль";
    const targetcard::Layout plain = targetcard::layout(d, t, 960, 10, 1.0f, false);
    CHECK(plain.hints.w == 0);
    d.pad = true;
    const targetcard::Layout L = targetcard::layout(d, t, 960, 10, 1.0f, false);
    CHECK(L.hints.w > 60 && L.hints.x >= L.book.x + L.book.w && L.card.w > plain.card.w);
    CHECK(L.hints.x + L.hints.w <= L.card.x + L.card.w);
    const std::vector<targetcard::Words> w = targetcard::words(L, d, t, false);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const targetcard::Words& x) { return x.text == text; });
    };
    CHECK(has("Атака") && has("Інша ціль") && has("A") && has("R"));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    targetcard::paint(p, L, d, false);
    uint32_t ring = 0;  // A's green ring at the hints' top left
    for (int y = int(L.hints.y); y < int(L.hints.y) + 3; ++y)
        ring = std::max(ring, (p.px[size_t(y) * size_t(p.width) + size_t(int(L.hints.x + 8))] >> 8) & 0xFFu);
    CHECK(ring > 0x90);
}
