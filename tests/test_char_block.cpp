#include <array>
// 3.0.11-beta.36: the character's block (core/char_block) - its words, its three views and two looks, the mouse over
// it, its picture (the bars, the rings, low HP)
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool inside(const panel::Box& outer, const panel::Box& inner) {
    return inner.x >= outer.x - 1e-3f && inner.y >= outer.y - 1e-3f && inner.x + inner.w <= outer.x + outer.w + 1e-3f &&
           inner.y + inner.h <= outer.y + outer.h + 1e-3f;
}

uint32_t pixel(const panel::Picture& p, float x, float y) {
    return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))];
}
int red(uint32_t c) { return int((c >> 16) & 0xFF); }
int green(uint32_t c) { return int((c >> 8) & 0xFF); }
int blue(uint32_t c) { return int(c & 0xFF); }

charblock::Data sora() {
    charblock::Data d;
    d.name = "Sora";
    d.job = charblock::jobName(9);
    d.baseLevel = 72;
    d.jobLevel = 41;
    d.hp = 3412;
    d.hpMost = 4120;
    d.sp = 812;
    d.spMost = 1560;
    d.baseExp = 0.463f;
    d.jobExp = 0.718f;
    d.weight = 38;
    d.zeny = 1482300;
    d.statusPoints = 3;
    return d;
}

charblock::Texts texts() {
    charblock::Texts t;
    t.weight = "Вага";
    t.points = "очко|очки|очок";
    t.lowHp = "Мало HP \xC2\xB7 {1} %";
    for (int i = 0; i < charblock::kButtons; ++i) t.buttons[i] = "Button " + std::to_string(i);
    t.skillPoints = "очко навичок|очки навичок|очок навичок";  // 3.0.11-beta.141
    t.expMax = "макс.";
    t.hour = "год";
    t.minute = "хв";
    t.toLevel = "До рівня {1}";
    t.levelIn = "Рівень через \xE2\x89\x88 {1}";
    t.perHour = "За годину +{1}";
    t.session = "За сесію +{1}";
    t.lastGain = "Остання здобич +{1}";
    t.fold = "Alt+V \xE2\x80\x94 згорнути";
    t.unfold = "Alt+V \xE2\x80\x94 розгорнути";
    return t;
}

// 3.0.11-beta.141: Sora's experience in numbers - 1 234 567 of 2 666 000 (46,3 %), the job's 71 800 of 100 000, the
// base's pace 4,2 % an hour
charblock::Data soraInNumbers() {
    charblock::Data d = sora();
    d.baseNow = 1234567;
    d.baseNext = 2666000;
    d.jobNow = 71800;
    d.jobNext = 100000;
    d.basePerHour = 111972;  // 4,2 % of 2 666 000
    d.baseSession = 328000;
    d.baseLast = 3456;
    d.skillPoints = 2;
    return d;
}

bool hasText(const std::vector<charblock::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const charblock::Words& x) { return x.text == text; });
}

const charblock::Words* wordsOf(const std::vector<charblock::Words>& w, const std::string& text) {
    const auto it = std::find_if(w.begin(), w.end(), [&](const charblock::Words& x) { return x.text == text; });
    return it == w.end() ? nullptr : &*it;
}

panel::Picture canvas(const charblock::Layout& L) {
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    return p;
}

}  // namespace

TEST(char_block_words_of_the_game) {
    using V = charblock::View;
    CHECK(charblock::nextView(V::Full) == V::Compact && charblock::nextView(V::Compact) == V::Icon &&
          charblock::nextView(V::Icon) == V::Full);
    CHECK(std::string(charblock::jobName(9)) == "Wizard" && std::string(charblock::jobName(4008)) == "Lord Knight");
    CHECK(std::string(charblock::jobName(23)) == "Super Novice" && std::string(charblock::jobName(4049)) ==
                                                                        "Soul Linker");
    CHECK(std::string(charblock::jobName(4045)) == "Super Baby" && std::string(charblock::jobName(13)) == "Knight");
    CHECK(std::string(charblock::jobName(12345)).empty() && std::string(charblock::jobName(-1)).empty());
    CHECK(charblock::grouped(1482300) == "1\xC2\xA0" "482\xC2\xA0" "300" && charblock::grouped(812) == "812");
    CHECK(charblock::grouped(0) == "0" && charblock::grouped(1000) == "1\xC2\xA0" "000");
    CHECK(charblock::percent(0.463f, ',') == "46,3\xC2\xA0%" && charblock::percent(1.0f, '.') == "100.0\xC2\xA0%");
    charblock::Data d = sora();
    CHECK(!charblock::lowHp(d));
    d.hp = 742;
    CHECK(charblock::lowHp(d));
    d.hpMost = 0;  // not known: not low
    CHECK(!charblock::lowHp(d));
}

TEST(char_block_full) {
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 10, 12, 1.0f);
    CHECK(L.x == 10 && L.y == 12 && L.block.x == 0 && L.block.y == 0);
    CHECK(L.block.w >= 360 && L.block.h > 150 && L.w >= int(L.block.w) && L.h > int(L.block.h));
    CHECK(L.portrait.w == 54 && L.portrait.h == 54 && inside(L.block, L.portrait));
    CHECK(L.name.x > L.portrait.x + L.portrait.w && L.levels.x + L.levels.w <= L.block.w);
    CHECK(L.info.y > L.name.y && L.hpBar.y > L.info.y + L.info.h - 1);
    CHECK(inside(L.block, L.hpBar) && inside(L.block, L.spBar) && L.spBar.y > L.hpBar.y + L.hpBar.h);
    CHECK(L.hpText.x > L.hpBar.x + L.hpBar.w && L.hpLabel.x + L.hpLabel.w <= L.hpBar.x);
    CHECK(L.expBar.y > L.spBar.y + L.spBar.h && L.expBar.h < L.hpBar.h && L.baseText.y >= L.expBar.y + L.expBar.h);
    // 3.0.11-beta.141: the points' own line under the info; the job's bar under the base's numbers, its numbers under
    // it
    CHECK(L.points.y >= L.info.y + L.info.h - 1 && L.hpBar.y > L.points.y + L.points.h - 1);
    CHECK(L.jobBar.y > L.baseText.y + L.baseText.h - 1 && L.jobBar.h == L.expBar.h && L.jobText.y >= L.jobBar.y +
                                                                                                       L.jobBar.h);
    CHECK(L.jobBar.x == L.expBar.x && L.expBar.x == L.hpBar.x && L.expBar.w == L.hpBar.w);  // under HP and SP
    CHECK(L.basePct.x > L.expBar.x + L.expBar.w && L.baseLabel.x + L.baseLabel.w <= L.expBar.x);
    CHECK(L.baseText.x == L.expBar.x && L.baseText.x + L.baseText.w > L.basePct.x);  // its numbers the row's width
    for (int i = 0; i < charblock::kButtons; ++i) {
        CHECK(inside(L.block, L.buttons[i]) && L.buttons[i].y >= L.jobText.y + L.jobText.h);
        if (i) CHECK(L.buttons[i].x > L.buttons[i - 1].x + L.buttons[i - 1].w);
    }
    CHECK(L.under.y >= L.block.y + L.block.h && L.under.y + L.under.h <= float(L.h));
    const charblock::Layout L2 = charblock::layout(charblock::View::Full, charblock::Look::Rings, 20, 24, 2.0f);
    CHECK(L2.portrait.w == 108 && std::fabs(L2.block.w - 2 * L.block.w) < 1.0f && L2.x == 20);  // twice the scale
}

TEST(char_block_compact_and_icon) {
    using V = charblock::View;
    using K = charblock::Look;
    const charblock::Layout full = charblock::layout(V::Full, K::Bars, 0, 0, 1.0f);
    const charblock::Layout bars = charblock::layout(V::Compact, K::Bars, 0, 0, 1.0f);
    CHECK(bars.block.h < full.block.h / 2 && bars.block.w < full.block.w);
    CHECK(bars.hpBar.x > bars.portrait.x + bars.portrait.w && bars.spBar.y > bars.hpBar.y);
    CHECK(bars.hpText.y >= bars.spBar.y + bars.spBar.h && bars.spText.x > bars.hpText.x);
    CHECK(bars.buttons[0].w == 0);  // the compact block: no buttons
    // 3.0.11-beta.141: the experience's two thin lines at its foot - the base's, the job's under it
    CHECK(bars.expBar.w > bars.hpBar.w && bars.expBar.h <= 4 && bars.expBar.y >= bars.hpText.y + bars.hpText.h);
    CHECK(bars.jobBar.y > bars.expBar.y + bars.expBar.h - 1 && inside(bars.block, bars.jobBar));
    const charblock::Layout rings = charblock::layout(V::Compact, K::Rings, 0, 0, 1.0f);
    CHECK(std::fabs(rings.cx - (rings.portrait.x + rings.portrait.w / 2)) < 0.5f && rings.hpR > rings.spR);
    CHECK(rings.spR - rings.spW >= rings.portrait.w / 2);  // the rings round the portrait, not over it
    CHECK(rings.badge.w > 0 && rings.badge.y > rings.cy && std::fabs(rings.badge.x + rings.badge.w / 2 - rings.cx) <
                                                               0.5f);
    CHECK(rings.hpBar.w == 0 && inside(rings.block, rings.portrait));
    const charblock::Layout icon = charblock::layout(V::Icon, K::Bars, 0, 0, 1.0f);  // the icon: rings whatever looks
    CHECK(icon.hpR > 0 && icon.hpR < rings.hpR && icon.block.w < rings.block.w && icon.badge.w > 0);
}

TEST(char_block_mouse) {
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 10, 12, 1.0f);
    const panel::Box& p = L.portrait;
    CHECK(charblock::hitTest(L, p.x + p.w / 2, p.y + p.h / 2).part == charblock::Part::Portrait);
    for (int i = 0; i < charblock::kButtons; ++i) {
        const charblock::Hit h = charblock::hitTest(L, L.buttons[i].x + 4, L.buttons[i].y + 4);
        CHECK(h.part == charblock::Part::Button && h.index == i);
    }
    CHECK(charblock::hitTest(L, L.hpBar.x + 3, L.hpBar.y + 3).part == charblock::Part::Body);
    const charblock::Hit base = charblock::hitTest(L, L.expBar.x + 20, L.expBar.y + 1);  // 3.0.11-beta.141
    const charblock::Hit job = charblock::hitTest(L, L.jobText.x + 20, L.jobText.y + 4);
    CHECK(base.part == charblock::Part::Exp && base.index == 0 && job.part == charblock::Part::Exp && job.index == 1);
    CHECK(charblock::hitTest(L, L.block.w + 5, 5).part == charblock::Part::None);
    CHECK(charblock::hitTest(L, 5, L.under.y + 3).part == charblock::Part::None);  // the warning, the tips: not its
    const charblock::Layout R = charblock::layout(charblock::View::Icon, charblock::Look::Rings, 0, 0, 1.0f);
    CHECK(charblock::hitTest(R, R.cx, R.cy).part == charblock::Part::Portrait);
}

// 3.0.11-beta.141 (the owner: «блок с персонажем должен работать от альт-V»): Alt+V as the game's Basic Info window's
// own key - the full block to the small one last used (compact or the icon), a small one to the full
TEST(char_block_alt_v) {
    using V = charblock::View;
    CHECK(charblock::altV(V::Full, V::Compact) == V::Compact && charblock::altV(V::Full, V::Icon) == V::Icon);
    CHECK(charblock::altV(V::Compact, V::Compact) == V::Full && charblock::altV(V::Icon, V::Icon) == V::Full);
    CHECK(charblock::altV(V::Full, V::Full) == V::Compact);  // no small one known: compact
}

// 3.0.11-beta.141: the time to the level - minutes under an hour, hours and minutes under ten hours, whole hours on
TEST(char_block_durations) {
    CHECK(charblock::duration(50, "год", "хв") == "50 хв" && charblock::duration(0, "год", "хв") == "1 хв");
    CHECK(charblock::duration(125, "год", "хв") == "2 год 5 хв" && charblock::duration(120, "год", "хв") == "2 год");
    CHECK(charblock::duration(768, "год", "хв") == "13 год" && charblock::duration(70000, "год", "хв") == "> 999 год");
}

