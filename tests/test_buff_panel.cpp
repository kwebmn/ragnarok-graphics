// 3.0.11-beta.35: the buffs at the right edge (core/buff_panel) - their order, the column, the names, the row, what
// does not fit, the mouse over them, their picture
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/buff_panel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

buffpanel::Entry entry(uint16_t icon, buffpanel::Kind kind, const char* time = "1:44", float left = 0.5f) {
    buffpanel::Entry e;
    e.icon = icon;
    e.name = "Buff " + std::to_string(icon);
    e.time = time;
    e.left = left;
    e.kind = kind;
    return e;
}

bool inside(const panel::Box& outer, const panel::Box& inner) {
    return inner.x >= outer.x - 1e-3f && inner.y >= outer.y - 1e-3f && inner.x + inner.w <= outer.x + outer.w + 1e-3f &&
           inner.y + inner.h <= outer.y + outer.h + 1e-3f;
}

uint32_t pixel(const panel::Picture& p, float x, float y) {
    return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))];
}

}  // namespace

TEST(buff_panel_debuffs_and_names) {
    for (const uint16_t icon : {0, 8, 13, 22, 35, 36, 50, 53, 124, 201, 282, 286}) CHECK(buffpanel::debuff(icon));
    for (const uint16_t icon : {10, 12, 20, 21, 200, 241}) CHECK(!buffpanel::debuff(icon));  // Blessing ... food
    CHECK(std::string(buffpanel::stateName(35)) == "Weight 50 %" && std::string(buffpanel::stateName(32)) ==
                                                                        "Broken Armor");
    CHECK(std::string(buffpanel::stateName(10)).empty());  // Blessing: its skill names it
}

TEST(buff_panel_order) {
    using K = buffpanel::Kind;
    const std::vector<buffpanel::Entry> e{entry(13, K::Debuff), entry(9, K::Buff), entry(10, K::Favourite),
                                          entry(21, K::Buff), entry(12, K::Favourite)};
    CHECK(buffpanel::order(e) == std::vector<int>({2, 4, 1, 3, 0}));  // favourites, the rest, debuffs; each as given
    CHECK(buffpanel::iconSide(K::Favourite, 1) == 32 && buffpanel::iconSide(K::Buff, 1) == 24 &&
          buffpanel::iconSide(K::Debuff, 2) == 48);
}

TEST(buff_panel_column) {
    using K = buffpanel::Kind;
    const std::vector<buffpanel::Entry> e{entry(9, K::Buff), entry(10, K::Favourite), entry(13, K::Debuff),
                                          entry(21, K::Buff), entry(12, K::Favourite)};
    const buffpanel::Layout L = buffpanel::layout(e, buffpanel::Look::Column, 1900, 300, 1030, 0, 1.0f);
    CHECK(L.x + L.w == 1900 && L.y == 300);
    CHECK(L.slots.size() == 5 && L.hidden == 0 && L.more.w == 0);
    CHECK(L.slots[0].entry == 1 && L.slots[1].entry == 4 && L.slots[4].entry == 2);  // as order() puts them
    CHECK(L.slots[0].frame.w == 40 && L.slots[0].icon.w == 32 && L.slots[2].frame.w == 30 && L.slots[2].icon.w == 24);
    CHECK_EQ(L.seps.size(), size_t(2));  // favourites | the rest | debuffs
    CHECK(L.body.w == 54 && std::fabs(L.body.x + L.body.w - float(L.w)) < 1e-3f);  // at the picture's right edge
    CHECK(L.toggle.w > 0 && L.toggle.y + L.toggle.h <= L.body.y && std::fabs(L.toggle.x + L.toggle.w - float(L.w)) <
                                                                       1e-3f);
    for (size_t i = 0; i < L.slots.size(); ++i) {
        const buffpanel::Slot& s = L.slots[i];
        CHECK(inside(L.body, s.frame) && inside(s.frame, s.icon));
        CHECK(s.time.y >= s.frame.y + s.frame.h && inside(L.body, s.time));  // the time under its icon
        CHECK(std::fabs((s.frame.x + s.frame.w / 2) - (L.body.x + L.body.w / 2)) < 0.51f);  // centred
        if (i) CHECK(s.frame.y > L.slots[i - 1].time.y);                                       // down the column
    }
    CHECK(inside(L.hit, L.body) && inside(L.hit, L.toggle));
    CHECK(L.tipRoom.w > 0 && L.tipRoom.x + L.tipRoom.w <= L.body.x);  // the tip left of the column
    // twice the scale: twice the sizes
    const buffpanel::Layout L2 = buffpanel::layout(e, buffpanel::Look::Column, 1900, 300, 2000, 0, 2.0f);
    CHECK(L2.slots[0].frame.w == 80 && L2.body.w == 108);
}

TEST(buff_panel_names_and_row) {
    using K = buffpanel::Kind;
    const std::vector<buffpanel::Entry> e{entry(10, K::Favourite), entry(9, K::Buff), entry(13, K::Debuff)};
    const buffpanel::Layout N = buffpanel::layout(e, buffpanel::Look::Names, 1900, 300, 1030, 0, 1.0f);
    CHECK(N.body.w >= 190 && N.toggle.w > 0 && N.tipRoom.w == 0);
    for (const buffpanel::Slot& s : N.slots)
        CHECK(s.name.w > 40 && s.name.x > s.frame.x + s.frame.w && s.time.x > s.name.x && inside(N.body, s.time));
    const buffpanel::Layout R = buffpanel::layout(e, buffpanel::Look::Row, 1900, 300, 1030, 400, 1.0f);
    CHECK(R.toggle.w == 0 && R.x + R.w == 1900 && R.slots.size() == 3);
    CHECK(R.slots[0].frame.x < R.slots[1].frame.x && R.slots[1].frame.x < R.slots[2].frame.x);  // left to right
    CHECK(R.slots[0].frame.y == R.slots[1].frame.y && R.slots[2].time.y >= R.slots[2].frame.y + R.slots[2].frame.h);
    CHECK_EQ(R.seps.size(), size_t(2));
    CHECK(R.tipRoom.w > 0 && R.tipRoom.y >= R.body.y + R.body.h);  // the tip under the row
    CHECK(std::fabs(R.body.x + R.body.w - float(R.w)) < 1e-3f);
}

TEST(buff_panel_what_does_not_fit) {
    using K = buffpanel::Kind;
    std::vector<buffpanel::Entry> e{entry(10, K::Favourite)};
    for (uint16_t i = 0; i < 12; ++i) e.push_back(entry(uint16_t(100 + i), K::Buff));
    e.push_back(entry(13, K::Debuff));
    e.push_back(entry(8, K::Debuff));
    const buffpanel::Layout L = buffpanel::layout(e, buffpanel::Look::Column, 1900, 300, 800, 0, 1.0f);
    CHECK(L.y + L.h <= 800 && L.hidden > 0 && L.more.w > 0 && inside(L.body, L.more));
    CHECK(L.more.h == 28 && L.more.w == L.body.w - 14 && L.lines.empty());  // the concept's: its whole width, 28 px
    CHECK_EQ(L.slots.size() + size_t(L.hidden), e.size());
    // the favourite and both debuffs shown; the rest's last ones hidden, its first ones kept
    const auto shownIn = [](const buffpanel::Layout& of, int i) {
        return std::any_of(of.slots.begin(), of.slots.end(), [&](const buffpanel::Slot& s) { return s.entry == i; });
    };
    const auto shown = [&](int i) { return shownIn(L, i); };
    CHECK(shown(0) && shown(13) && shown(14) && shown(1) && !shown(12));
    CHECK(L.more.y > L.slots[L.slots.size() - 3].frame.y);  // «ще N» after the rest, before the debuffs
    // the row: as many as `widest` holds
    const buffpanel::Layout R = buffpanel::layout(e, buffpanel::Look::Row, 1900, 300, 1030, 300, 1.0f);
    CHECK(R.w <= 300 && R.hidden > 0 && R.more.w > 0 && shownIn(R, 0) && shownIn(R, 13) && shownIn(R, 14));
    CHECK(R.more.x > R.slots[R.slots.size() - 3].frame.x && R.more.x < R.slots[R.slots.size() - 2].frame.x);
    // debuffs alone past the bottom: never hidden
    std::vector<buffpanel::Entry> d;
    for (uint16_t i = 0; i < 9; ++i) d.push_back(entry(13, K::Debuff));
    const buffpanel::Layout D = buffpanel::layout(d, buffpanel::Look::Column, 1900, 300, 500, 0, 1.0f);
    CHECK(D.slots.size() == 9 && D.hidden == 0 && D.y + D.h <= 500);  // 3.0.11-beta.63: in columns of their own
    // a buff hidden behind debuffs alone: «ще 1» first, none over it
    d.push_back(entry(9, K::Buff));
    const buffpanel::Layout M = buffpanel::layout(d, buffpanel::Look::Column, 1900, 300, 500, 0, 1.0f);
    CHECK(M.hidden == 1 && M.more.w > 0 && M.more.y + M.more.h <= M.slots[0].frame.y);
}

namespace {

// The glass a slot of `entryIndex` stands in: 0 the body, 1.. the further lines; -1 not shown
int glassOf(const buffpanel::Layout& L, int entryIndex) {
    for (const buffpanel::Slot& s : L.slots) {
        if (s.entry != entryIndex) continue;
        if (inside(L.body, s.frame)) return 0;
        for (size_t i = 0; i < L.lines.size(); ++i)
            if (inside(L.lines[i], s.frame)) return int(i) + 1;
        return -2;  // outside every glass
    }
    return -1;
}

}  // namespace

// 3.0.11-beta.63 (the concept's «Крайние случаи»: «Если дебаффов больше, чем мест, те, что не влезли, открывают
// второй столбик слева — только для дебаффов»): debuffs past the room open columns of their own on the left
TEST(buff_panel_debuffs_open_columns) {
    using K = buffpanel::Kind;
    std::vector<buffpanel::Entry> d;
    for (uint16_t i = 0; i < 9; ++i) d.push_back(entry(uint16_t(200 + i), K::Debuff));
    const buffpanel::Layout D = buffpanel::layout(d, buffpanel::Look::Column, 1900, 300, 500, 0, 1.0f);
    CHECK(D.slots.size() == 9 && D.hidden == 0 && D.more.w == 0 && D.x + D.w == 1900 && D.y + D.h <= 500);
    CHECK_EQ(D.lines.size(), size_t(2));  // three a column in 200 px
    CHECK(D.lines[0].x + D.lines[0].w <= D.body.x && D.lines[1].x + D.lines[1].w <= D.lines[0].x);  // to the left
    CHECK(D.lines[0].y == D.body.y && D.lines[0].w == D.body.w && D.lines[0].h <= D.body.h);
    CHECK(glassOf(D, 0) == 0 && glassOf(D, 2) == 0 && glassOf(D, 3) == 1 && glassOf(D, 5) == 1 && glassOf(D, 8) == 2);
    for (size_t i = 0; i < D.slots.size(); ++i) CHECK_EQ(D.slots[i].entry, int(i));  // in their order
    // the mouse: one in a further column is its own; the tips left of them all
    const buffpanel::Slot& s = D.slots[4];
    const buffpanel::Hit h = buffpanel::hitTest(D, s.frame.x + s.frame.w / 2, s.frame.y + s.frame.h / 2);
    CHECK(h.part == buffpanel::Part::Icon && h.slot == 4);
    const panel::Box tip = buffpanel::tipBox(D, 0, 100);
    CHECK(tip.w >= 100 && tip.x >= 0 && tip.x + tip.w <= D.lines[1].x);
    CHECK(D.tipRoom.w > 0 && D.tipRoom.x + D.tipRoom.w <= D.lines[1].x);
    CHECK(inside(D.hit, D.body) && inside(D.hit, D.toggle));  // the main column's box
    CHECK(inside(D.linesHit, D.lines[0]) && inside(D.linesHit, D.lines[1]) && D.linesHit.x + D.linesHit.w <= D.body.x);
    // a buff hidden behind them: «ще 1» at the first column's top, the debuffs after it, those past it further on
    d.push_back(entry(9, K::Buff));
    const buffpanel::Layout M = buffpanel::layout(d, buffpanel::Look::Column, 1900, 300, 500, 0, 1.0f);
    CHECK(M.hidden == 1 && inside(M.body, M.more) && M.slots.size() == 9 && M.y + M.h <= 500);
    CHECK(glassOf(M, 0) == 0 && glassOf(M, 2) == 1 && glassOf(M, 9) == -1);
    // the row: a second row under the first
    std::vector<buffpanel::Entry> r;
    for (uint16_t i = 0; i < 12; ++i) r.push_back(entry(uint16_t(200 + i), K::Debuff));
    const buffpanel::Layout R = buffpanel::layout(r, buffpanel::Look::Row, 1900, 300, 1030, 300, 1.0f);
    CHECK(R.hidden == 0 && R.slots.size() == 12 && R.w <= 300 && R.lines.size() == 1);
    CHECK(R.lines[0].y >= R.body.y + R.body.h && std::fabs(R.lines[0].x + R.lines[0].w - float(R.w)) < 1e-3f);
    CHECK(glassOf(R, 0) == 0 && glassOf(R, 11) == 1 && R.tipRoom.y >= R.lines[0].y + R.lines[0].h);
    const panel::Box rowTip = buffpanel::tipBox(R, 0, 100);
    CHECK(rowTip.y >= R.lines[0].y + R.lines[0].h && rowTip.y + rowTip.h <= float(R.h));
}

// 3.0.11-beta.63 (the concept's variant 2, question 17 - a choice): nothing hidden, what does not fit goes into a
// second column on the left (the row: a second row); the debuffs stay at the first column's end
TEST(buff_panel_second_column) {
    using K = buffpanel::Kind;
    std::vector<buffpanel::Entry> e{entry(10, K::Favourite)};
    for (uint16_t i = 0; i < 12; ++i) e.push_back(entry(uint16_t(100 + i), K::Buff));
    e.push_back(entry(13, K::Debuff));
    e.push_back(entry(8, K::Debuff));
    const buffpanel::Layout L =
        buffpanel::layout(e, buffpanel::Look::Column, 1900, 300, 800, 0, 1.0f, buffpanel::Overflow::Lines);
    CHECK(L.hidden == 0 && L.more.w == 0 && L.slots.size() == e.size() && L.y + L.h <= 800);
    CHECK_EQ(L.lines.size(), size_t(1));
    CHECK(glassOf(L, 0) == 0 && glassOf(L, 1) == 0 && glassOf(L, 13) == 0 && glassOf(L, 14) == 0);
    CHECK(glassOf(L, 12) == 1 && glassOf(L, 11) == 1);
    const auto at = [&](int entryIndex) {
        for (const buffpanel::Slot& s : L.slots)
            if (s.entry == entryIndex) return s.frame.y;
        return -1.0f;
    };
    CHECK(at(13) > at(1) && at(14) > at(13));  // the debuffs last in the first column
    // the names: columns of names
    const buffpanel::Layout N =
        buffpanel::layout(e, buffpanel::Look::Names, 1900, 300, 600, 0, 1.0f, buffpanel::Overflow::Lines);
    CHECK(N.hidden == 0 && N.slots.size() == e.size() && !N.lines.empty() && N.lines[0].w == N.body.w);
    CHECK(N.y + N.h <= 600 && N.lines[0].x + N.lines[0].w <= N.body.x);
    // the row: a second row
    const buffpanel::Layout R =
        buffpanel::layout(e, buffpanel::Look::Row, 1900, 300, 1030, 300, 1.0f, buffpanel::Overflow::Lines);
    CHECK(R.hidden == 0 && R.slots.size() == e.size() && R.w <= 300 && R.lines.size() == 1);
    CHECK(glassOf(R, 0) == 0 && glassOf(R, 13) == 0 && glassOf(R, 12) == 1);
    // everything in its room: one column as before
    const std::vector<buffpanel::Entry> few{entry(10, K::Favourite), entry(9, K::Buff), entry(13, K::Debuff)};
    const buffpanel::Layout F =
        buffpanel::layout(few, buffpanel::Look::Column, 1900, 300, 800, 0, 1.0f, buffpanel::Overflow::Lines);
    CHECK(F.lines.empty() && F.linesHit.w == 0 && F.slots.size() == 3);
}

TEST(buff_panel_mouse) {
    using K = buffpanel::Kind;
    std::vector<buffpanel::Entry> e{entry(10, K::Favourite), entry(9, K::Buff)};
    for (uint16_t i = 0; i < 12; ++i) e.push_back(entry(uint16_t(100 + i), K::Buff));
    const buffpanel::Layout L = buffpanel::layout(e, buffpanel::Look::Column, 1900, 300, 800, 0, 1.0f);
    const buffpanel::Slot& s = L.slots[1];
    buffpanel::Hit h = buffpanel::hitTest(L, s.frame.x + s.frame.w / 2, s.frame.y + s.frame.h / 2);
    CHECK(h.part == buffpanel::Part::Icon && h.slot == 1);
    h = buffpanel::hitTest(L, s.time.x + s.time.w / 2, s.time.y + s.time.h / 2);  // its time is its too
    CHECK(h.part == buffpanel::Part::Icon && h.slot == 1);
    CHECK(buffpanel::hitTest(L, L.toggle.x + L.toggle.w / 2, L.toggle.y + 3).part == buffpanel::Part::Toggle);
    CHECK(buffpanel::hitTest(L, L.more.x + L.more.w / 2, L.more.y + L.more.h / 2).part == buffpanel::Part::More);
    CHECK(buffpanel::hitTest(L, L.tipRoom.x + 5, L.tipRoom.y + 5).part == buffpanel::Part::None);
    const panel::Box tip = buffpanel::tipBox(L, 1, 120);
    CHECK(tip.x + tip.w <= L.body.x && tip.w >= 120 && tip.x >= 0);
    CHECK(std::fabs((tip.y + tip.h / 2) - (s.frame.y + s.frame.h / 2)) < 1.0f);  // beside its icon
    CHECK(buffpanel::wordsWidth("Increase AGI", 13, true) > buffpanel::wordsWidth("Agi", 13, true));
    CHECK(buffpanel::wordsWidth("\xD1\x89\xD0\xB5 3", 12, false) < buffpanel::wordsWidth("abcdefgh", 12, false));
}

TEST(buff_panel_picture) {
    using K = buffpanel::Kind;
    std::vector<uint32_t> red(32 * 32, 0xFFFF0000u);
    const panel::Picture icon = buffpanel::iconPicture(red, 32, 32, true, 24, 5);
    CHECK(icon.width == 24 && icon.height == 24);
    CHECK(pixel(icon, 12, 12) == 0xFFFF0000u && (pixel(icon, 0, 0) >> 24) < 0x80);  // its corners rounded off
    std::vector<uint32_t> keyed(32 * 32, 0xFFFF00FFu);  // magenta without alpha: keyed out
    keyed[16 * 32 + 16] = 0xFF00FF00u;
    const panel::Picture k = buffpanel::iconPicture(keyed, 32, 32, false, 32, 0);
    CHECK((pixel(k, 2, 2) >> 24) == 0 && pixel(k, 16, 16) == 0xFF00FF00u);
    // the ring: a quarter of its time left - gold from twelve o'clock to three, dark the rest
    std::vector<buffpanel::Entry> e{entry(10, K::Favourite, "1:00", 0.25f), entry(13, K::Debuff, "0:24", 1.0f)};
    const panel::Picture big = buffpanel::iconPicture(red, 32, 32, true, 32, 5);
    e[0].picture = &big;
    const buffpanel::Layout L = buffpanel::layout(e, buffpanel::Look::Column, 400, 0, 600, 0, 1.0f);
    panel::Picture out;
    out.width = L.w;
    out.height = L.h;
    out.px.assign(size_t(L.w) * size_t(L.h), 0u);
    buffpanel::paint(out, L, e, -1);
    const panel::Box& f = L.slots[0].frame;
    const uint32_t gold = pixel(out, f.x + f.w * 0.75f, f.y + 1.5f), dark = pixel(out, f.x + f.w * 0.25f, f.y + 1.5f);
    CHECK(((gold >> 16) & 0xFF) > 0xB0 && ((gold >> 8) & 0xFF) > 0x90 && (gold & 0xFF) < ((gold >> 8) & 0xFF));
    CHECK(((dark >> 16) & 0xFF) < 0x40);
    const panel::Box& r = L.slots[1].frame;  // a debuff's ring red
    const uint32_t red2 = pixel(out, r.x + r.w * 0.5f, r.y + 1.5f);
    CHECK(((red2 >> 16) & 0xFF) > 0xB0 && ((red2 >> 8) & 0xFF) < 0x80);
    CHECK(pixel(out, L.slots[0].icon.x + 16, L.slots[0].icon.y + 16) == 0xFFFF0000u);  // its icon over the glass
    // the words: times under, a debuff's in its red, the toggle's, the hovered one's tip
    const buffpanel::Texts t{"назви", "сховати назви", "ще {1}", "клік — в обрані", "клік — прибрати з обраних"};
    const std::vector<buffpanel::Words> w = buffpanel::words(L, e, 0, t);
    const auto has = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const buffpanel::Words& x) { return x.text == text; });
    };
    CHECK(has("1:00") != w.end() && has("0:24") != w.end() && has("0:24")->rgb != has("1:00")->rgb);
    CHECK(has("\xE2\x80\xB9 назви") != w.end() || has("назви") != w.end());
    CHECK(has("Buff 10 \xC2\xB7 1:00") != w.end() && has("клік — прибрати з обраних") != w.end());  // a favourite's
    // its ground left of the column, the words in it; a debuff's tip one line, no click
    const panel::Box tip = buffpanel::tipOf(L, e, 0, t);
    CHECK(tip.w > 0 && tip.x + tip.w <= L.body.x && inside(tip, has("Buff 10 \xC2\xB7 1:00")->box));
    buffpanel::paintTip(out, L, e, 0, t);
    CHECK((pixel(out, tip.x + tip.w / 2, tip.y + tip.h / 2) >> 24) > 0xE0);
    const panel::Box one = buffpanel::tipOf(L, e, 1, t);
    CHECK(one.w > 0 && one.h < tip.h);
    CHECK(buffpanel::tipOf(L, e, -1, t).w == 0);
}
