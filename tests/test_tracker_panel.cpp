// 3.0.11-beta.70 (the concept's «держать перед глазами»: «Отслеживание — что закрепить (квест, путь, вещь для сбора),
// выбирает игрок; окно стоит левее столбика баффов»): the tracker panel (core/tracker_panel) - its entries down the
// panel, folded to their names, what does not fit, the mouse over it, its words
#include <algorithm>
#include <string>
#include <vector>

#include "core/tracker_panel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool inside(const panel::Box& outer, const panel::Box& inner) {
    return inner.x >= outer.x - 1e-3f && inner.y >= outer.y - 1e-3f && inner.x + inner.w <= outer.x + outer.w + 1e-3f &&
           inner.y + inner.h <= outer.y + outer.h + 1e-3f;
}

std::vector<tracker::Entry> sample() {
    tracker::Entry quest{tracker::Kind::Quest, "Bunny Band", "квест", {}};
    quest.lines = {{"Feather", "\xC3\x97" "100", false}, {"Four Leaf Clover", "\xC3\x97" "1", false},
                   {"Здати: Kafra Corp. Rep. \xC2\xB7 Alberta", "", true}};
    tracker::Entry route{tracker::Kind::Route, "Morroc", "крок 1 з 3", {}};
    route.lines = {{"Зараз: північний вихід 170, 378 \xE2\x86\x92 Prontera", "", false}};
    tracker::Entry item{tracker::Kind::Item, "Fabric", "збір", {}};
    item.lines = {{"Wraith", "58,2 %", false}};
    return {quest, route, item};
}

const tracker::Texts kTexts{"Відстеження", "ще {1}"};

}  // namespace

TEST(tracker_panel_layout) {
    const std::vector<tracker::Entry> e = sample();
    const tracker::Layout L = tracker::layout(e, false, 1700, 300, 1000, 1.0f);
    CHECK(L.x + L.w == 1700 && L.y == 300 && L.h <= 700);
    CHECK_EQ(L.entries.size(), size_t(3));
    CHECK(L.hidden == 0 && L.more.w == 0);
    CHECK(L.title.w > 0 && L.fold.w > 0 && L.hide.w > 0 && L.hide.x > L.fold.x);  // its header: the name, ▾, ✕
    for (size_t i = 0; i < L.entries.size(); ++i) {
        const tracker::Placed& p = L.entries[i];
        CHECK(inside(L.glass, p.box) && inside(p.box, p.head) && inside(p.box, p.close));
        CHECK_EQ(p.lines.size(), e[i].lines.size());
        for (const panel::Box& b : p.lines) CHECK(inside(p.box, b) && b.y >= p.head.y + p.head.h);
        if (i) CHECK(p.box.y >= L.entries[i - 1].box.y + L.entries[i - 1].box.h);  // down the panel
    }
    // folded: their names alone
    const tracker::Layout F = tracker::layout(e, true, 1700, 300, 1000, 1.0f);
    CHECK(F.entries.size() == 3 && F.h < L.h);
    for (const tracker::Placed& p : F.entries) CHECK(p.lines.empty());
    // twice the scale: twice the width
    CHECK_EQ(tracker::layout(e, false, 1700, 300, 2000, 2.0f).w, L.w * 2);
}

TEST(tracker_panel_what_does_not_fit) {
    std::vector<tracker::Entry> e;
    for (int i = 0; i < 8; ++i) e.push_back(sample()[0]);
    const tracker::Layout L = tracker::layout(e, false, 1700, 300, 700, 1.0f);
    CHECK(L.y + L.h <= 700 && L.hidden > 0 && L.more.w > 0);
    CHECK_EQ(L.entries.size() + size_t(L.hidden), e.size());
    CHECK(L.more.y >= L.entries.back().box.y + L.entries.back().box.h);
    // one entry taller than the room: shown, its lines cut
    const tracker::Layout one = tracker::layout({sample()[0]}, false, 1700, 300, 330, 1.0f);
    CHECK(one.entries.size() == 1 && one.hidden == 0);
}

TEST(tracker_panel_mouse_and_words) {
    const std::vector<tracker::Entry> e = sample();
    const tracker::Layout L = tracker::layout(e, false, 1700, 300, 1000, 1.0f);
    const auto at = [&](const panel::Box& b) { return tracker::hitTest(L, b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(at(L.fold).part == tracker::Part::Fold && at(L.hide).part == tracker::Part::Hide);
    const tracker::Hit head = at(L.entries[1].head), close = at(L.entries[2].close);
    CHECK(head.part == tracker::Part::Head && head.entry == 1);
    CHECK(close.part == tracker::Part::Close && close.entry == 2);
    CHECK(at(L.entries[0].lines[0]).part == tracker::Part::Panel);
    CHECK(tracker::hitTest(L, -5, -5).part == tracker::Part::None);
    const std::vector<tracker::Words> w = tracker::words(L, e, kTexts);
    const auto has = [&](const std::string& t) {
        return std::find_if(w.begin(), w.end(), [&](const tracker::Words& x) { return x.text == t; }) != w.end();
    };
    CHECK(has("Відстеження") && has("Bunny Band") && has("квест") && has("Feather") && has("\xC3\x97" "100"));
    CHECK(has("Morroc") && has("крок 1 з 3") && has("Fabric") && has("58,2 %"));
    std::vector<tracker::Entry> many;
    for (int i = 0; i < 8; ++i) many.push_back(sample()[0]);
    const tracker::Layout M = tracker::layout(many, false, 1700, 300, 700, 1.0f);
    const std::vector<tracker::Words> mw = tracker::words(M, many, kTexts);
    CHECK(std::any_of(mw.begin(), mw.end(),
                      [&](const tracker::Words& x) { return x.text == "ще " + std::to_string(M.hidden); }));
    panel::Picture pic;
    pic.width = L.w;
    pic.height = L.h;
    pic.px.assign(size_t(L.w) * size_t(L.h), 0u);
    tracker::paint(pic, L, e, tracker::Hit{});
    CHECK((pic.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);  // its glass
}
