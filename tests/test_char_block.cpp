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

TEST(char_block_picture) {
    const charblock::Texts t = texts();
    charblock::Data d = sora();
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    panel::Picture p = canvas(L);
    charblock::paint(p, L, d, t, charblock::Show{});
    const uint32_t hp = pixel(p, L.hpBar.x + L.hpBar.w * 0.4f, L.hpBar.y + L.hpBar.h / 2);  // 83 %: filled there
    CHECK(green(hp) > 0xA0 && green(hp) > red(hp) && green(hp) > blue(hp));
    const uint32_t sp = pixel(p, L.spBar.x + L.spBar.w * 0.3f, L.spBar.y + L.spBar.h / 2);  // 52 %
    CHECK(blue(sp) > 0xC0 && blue(sp) > red(sp));
    const uint32_t empty = pixel(p, L.spBar.x + L.spBar.w * 0.9f, L.spBar.y + L.spBar.h / 2);
    CHECK(blue(empty) < 0x60);
    d.hp = 742;  // low: the bar red, the warning under the block
    panel::Picture low = canvas(L);
    charblock::Show warn;
    warn.warning = true;
    charblock::paint(low, L, d, t, warn);
    const uint32_t lowHp = pixel(low, L.hpBar.x + 4, L.hpBar.y + L.hpBar.h / 2);
    CHECK(red(lowHp) > 0xC0 && green(lowHp) < 0x80);
    std::vector<charblock::Words> w = charblock::words(L, d, t, warn);
    const auto has = [&](const std::string& text) {
        return std::find_if(w.begin(), w.end(), [&](const charblock::Words& x) { return x.text == text; }) != w.end();
    };
    CHECK(has("Sora") && has("Wizard") && has("Base 72 \xC2\xB7 Job 41") && has("+3 очки"));
    CHECK(has("742 / 4\xC2\xA0" "120") && has("812 / 1\xC2\xA0" "560") && has("! Мало HP \xC2\xB7 18 %"));
    CHECK(has("1\xC2\xA0" "482\xC2\xA0" "300") && has("38\xC2\xA0%"));
    // the rings: the HP ring filled from twelve o'clock round, the rest of it dark; the level on its badge
    d.hp = 2060;  // a half
    const charblock::Layout R = charblock::layout(charblock::View::Compact, charblock::Look::Rings, 0, 0, 1.0f);
    panel::Picture r = canvas(R);
    charblock::paint(r, R, d, t, charblock::Show{});
    const float mid = R.hpR - R.hpW / 2;
    const uint32_t on = pixel(r, R.cx + mid * 0.7071f, R.cy - mid * 0.7071f);   // half past one: filled
    const uint32_t off = pixel(r, R.cx - mid * 0.7071f, R.cy - mid * 0.7071f);  // half past ten: not
    CHECK(green(on) > 0xA0 && green(off) < 0x60);
    charblock::Show hover;
    hover.hoverBlock = true;  // its numbers in a tip
    w = charblock::words(R, d, t, hover);
    CHECK(has("72") && has("HP 2\xC2\xA0" "060 / 4\xC2\xA0" "120") && has("SP 812 / 1\xC2\xA0" "560"));
}

// 3.0.11-beta.46 (the concept's motion and low HP): the portrait glows red, the warning apart in its own picture (its
// place kept for the tip under it), a new level's gold edge
TEST(char_block_low_hp_glow_warning_apart_and_flare) {
    const charblock::Texts t = texts();
    charblock::Data d = sora();
    d.hp = 742;
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    panel::Picture p = canvas(L);
    charblock::Show apart;
    apart.warning = apart.warningApart = true;
    charblock::paint(p, L, d, t, apart);
    const panel::Box& pic = L.portrait;
    const uint32_t glow = pixel(p, pic.x + pic.w / 2, pic.y - 1.5f);  // just over the portrait's top
    CHECK(red(glow) > 0x90 && red(glow) > 2 * green(glow));
    const std::vector<charblock::Words> w = charblock::words(L, d, t, apart);
    CHECK(std::none_of(w.begin(), w.end(), [](const charblock::Words& x) { return x.text.rfind("! ", 0) == 0; }));
    CHECK(pixel(p, L.under.x + 30, L.under.y + 15) == 0u);  // the warning's place empty in the block's picture
    panel::Picture warn = canvas(L);
    charblock::paintWarning(warn, L, d, t);
    CHECK(pixel(warn, L.under.x + 30, L.under.y + 15) != 0u && pixel(warn, pic.x + pic.w / 2, pic.y + 2) == 0u);
    const std::vector<charblock::Words> ww = charblock::warningWords(L, d, t);
    CHECK(ww.size() == 1 && ww[0].text == "! Мало HP \xC2\xB7 18 %");
    d.hp = 3000;  // not low: no glow, no warning
    panel::Picture fine = canvas(L);
    charblock::paint(fine, L, d, t, charblock::Show{});
    CHECK(red(pixel(fine, pic.x + pic.w / 2, pic.y - 1.5f)) < 0x40);
    CHECK(charblock::warningWords(L, d, t).empty());
    panel::Picture flare = canvas(L);  // a new level: the edge gold
    charblock::paintFlare(flare, L);
    const uint32_t edge = pixel(flare, L.block.x + L.block.w / 2, L.block.y + 0.5f);
    CHECK(red(edge) > 0xB0 && green(edge) > 0x90 && blue(edge) < green(edge));
    CHECK((pixel(flare, L.block.x + L.block.w / 2, L.block.y + L.block.h / 2) >> 24) < 0x40);  // its middle clear
    const charblock::Layout R = charblock::layout(charblock::View::Compact, charblock::Look::Rings, 0, 0, 1.0f);
    panel::Picture ring = canvas(R);
    charblock::paintFlare(ring, R);
    const uint32_t gold = pixel(ring, R.cx, R.cy - R.hpR - 2);
    CHECK(red(gold) > 0xB0 && green(gold) > 0x90);
}

// 3.0.11-beta.51 (the concept's states): a tip only after its while; a pressed button 1 px down and darker
TEST(char_block_tip_after_its_while_and_a_press) {
    const charblock::Texts t = texts();
    const charblock::Data d = sora();
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    charblock::Show early;
    early.hoverButton = 2;
    early.hoverBlock = true;
    early.tip = false;
    const auto has = [&](const charblock::Show& sh, const std::string& text) {
        const std::vector<charblock::Words> w = charblock::words(L, d, t, sh);
        return std::any_of(w.begin(), w.end(), [&](const charblock::Words& x) { return x.text == text; });
    };
    CHECK(!has(early, "Button 2"));
    charblock::Show late = early;
    late.tip = true;
    CHECK(has(late, "Button 2"));
    panel::Picture plain = canvas(L), pressed = canvas(L);
    charblock::paint(plain, L, d, t, charblock::Show{});
    charblock::Show down;
    down.pressedButton = 1;
    charblock::paint(pressed, L, d, t, down);
    const panel::Box& b = L.buttons[1];
    const uint32_t before = pixel(plain, b.x + 3, b.y + 0.5f), after = pixel(pressed, b.x + 3, b.y + 0.5f);
    CHECK(before != after);  // its top edge moved down a pixel
    const uint32_t mid0 = pixel(plain, b.x + 4, b.y + b.h - 4), mid1 = pixel(pressed, b.x + 4, b.y + b.h - 4);
    CHECK(green(mid1) < green(mid0));  // darker
}

// 3.0.11-beta.114 (the concept «Концепция окон и инвентаря», question 10 «можна»): the arrows worn running low - «Стріли
// 37» under 50 (amber; red at 10 or fewer), «0» in red when they ran out (gone from the bag); none from 50 on, or none
// worn. 3.0.11-beta.141: on the points' line under the info (the weight's numbers took the info line's room)
TEST(char_block_the_arrows_running_low) {
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    charblock::Texts t = texts();
    t.ammo = "Стріли";
    charblock::Data d = sora();
    const auto colourOf = [&](const charblock::Data& data, const std::string& text) -> int64_t {
        for (const charblock::Words& w : charblock::words(L, data, t, charblock::Show{}))
            if (w.text == text && w.box.y >= L.points.y - 1 && w.box.y <= L.points.y + 1) return int64_t(w.rgb);
        return -1;
    };
    d.ammo = 37;
    CHECK(colourOf(d, "Стріли") >= 0 && colourOf(d, "37") == 0xF0A050);  // amber
    d.ammo = 8;
    CHECK(colourOf(d, "8") == 0xFF5A4E);  // red
    d.ammo = 0;
    CHECK(colourOf(d, "0") == 0xFF5A4E);  // ran out
    d.ammo = 120;
    CHECK(colourOf(d, "Стріли") < 0);  // plenty: none
    d.ammo = -1;
    CHECK(colourOf(d, "Стріли") < 0);  // none worn
}


// 3.0.11-beta.139 (the owner: «мне кажется что в интерфейсе вес не работает ... или не правильно считает»): the rings
// show the weight only as a dot from 50 % - its numbers now in their tip, the game's own (whole units, as its Basic
// Info window showed them) and the per cent as the game counts it
TEST(char_block_rings_tip_tells_the_weight) {
    charblock::Data d = sora();
    d.weightNow = 1710;
    d.weightMost = 4500;
    d.weight = 38;
    const charblock::Layout R = charblock::layout(charblock::View::Compact, charblock::Look::Rings, 0, 0, 1.0f);
    charblock::Show hover;
    hover.hoverBlock = true;
    const std::vector<charblock::Words> w = charblock::words(R, d, texts(), hover);
    const auto has = [&](const std::string& s) {
        return std::any_of(w.begin(), w.end(), [&](const charblock::Words& x) { return x.text == s; });
    };
    CHECK(has("Вага 1\xC2\xA0" "710 / 4\xC2\xA0" "500 \xC2\xB7 38\xC2\xA0%"));
    d.weightMost = 0;  // not known: no line
    const std::vector<charblock::Words> none = charblock::words(R, d, texts(), hover);
    CHECK(std::none_of(none.begin(), none.end(),
                       [](const charblock::Words& x) { return x.text.rfind("Вага", 0) == 0; }));
}

// 3.0.11-beta.139 (the owner: «пишет что 60% но хп воостанавливается» - uaRO stops the natural regen at 70 %, its
// natural_heal_weight_rate, not the classic 50 %): the weight's dot and colour by the server's own state - 1 no regen
// (amber), 2 the 90 % one (red), 0 none (no dot at 60 %); not known yet: from 50 % as before; the tip says which
TEST(char_block_overweight_by_the_servers_state) {
    const charblock::Layout R = charblock::layout(charblock::View::Compact, charblock::Look::Rings, 0, 0, 1.0f);
    charblock::Texts t = texts();
    t.overweight1 = "HP і SP не відновлюються";
    t.overweight2 = "Атаки й уміння заборонені";
    charblock::Data d = sora();
    d.weight = 60;
    d.weightNow = 2700;
    d.weightMost = 4500;
    const auto dot = [&](const charblock::Data& data) {
        panel::Picture p = canvas(R);
        charblock::paint(p, R, data, t, charblock::Show{});
        return pixel(p, R.weightDot.x + R.weightDot.w / 2, R.weightDot.y + R.weightDot.h / 2);
    };
    d.overweight = 0;  // the server: none - its regen goes on at 60 %
    CHECK(!(red(dot(d)) > 0xD0 && green(dot(d)) > 0x80 && blue(dot(d)) < 0x70));
    d.overweight = 1;
    const uint32_t amber = dot(d);
    CHECK(red(amber) > 0xD0 && green(amber) > 0x80 && blue(amber) < 0x70);
    d.overweight = 2;
    const uint32_t red90 = dot(d);
    CHECK(red(red90) > 0xD0 && green(red90) < 0x70);
    d.overweight = -1;  // not known: by 50 %
    CHECK(red(dot(d)) > 0xD0 && green(dot(d)) > 0x80);
    charblock::Show hover;
    hover.hoverBlock = true;
    d.overweight = 1;
    const std::vector<charblock::Words> w = charblock::words(R, d, t, hover);
    CHECK(std::any_of(w.begin(), w.end(),
                      [](const charblock::Words& x) { return x.text == "HP і SP не відновлюються"; }));
    d.overweight = 0;
    const std::vector<charblock::Words> calm = charblock::words(R, d, t, hover);
    CHECK(std::none_of(calm.begin(), calm.end(),
                       [](const charblock::Words& x) { return x.text == "HP і SP не відновлюються"; }));
}

// 3.0.11-beta.141 (the owner: «блок с персонажем должен работать от альт-V»): Alt+V as the game's Basic Info window's
// own key - the full block to the small one last used (compact or the icon), a small one to the full
TEST(char_block_alt_v) {
    using V = charblock::View;
    CHECK(charblock::altV(V::Full, V::Compact) == V::Compact && charblock::altV(V::Full, V::Icon) == V::Icon);
    CHECK(charblock::altV(V::Compact, V::Compact) == V::Full && charblock::altV(V::Icon, V::Icon) == V::Full);
    CHECK(charblock::altV(V::Full, V::Full) == V::Compact);  // no small one known: compact
}

// 3.0.11-beta.141 (the owner: «прогресбар с бейз опытом и джоб опытом, также желательно там же написать и точное
// количество опыта. и побольше подробностей»): the full block's two bars (the concept's colours: the base's violet,
// the job's teal), each with its per cent at the right and its numbers under it - the base's pace an hour and the
// time to the next level beside them; the last level «макс.»
TEST(char_block_experience_in_numbers) {
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    const charblock::Texts t = texts();
    charblock::Data d = soraInNumbers();
    const std::vector<charblock::Words> w = charblock::words(L, d, t, charblock::Show{});
    CHECK(hasText(w, "Base") && hasText(w, "Job") && hasText(w, "46,3\xC2\xA0%") && hasText(w, "71,8\xC2\xA0%"));
    const charblock::Words* base = wordsOf(w, "1\xC2\xA0" "234\xC2\xA0" "567 / 2\xC2\xA0" "666\xC2\xA0" "000");
    const charblock::Words* job = wordsOf(w, "71\xC2\xA0" "800 / 100\xC2\xA0" "000");
    CHECK(base && std::fabs(base->box.y - L.baseText.y) < 1 && job && std::fabs(job->box.y - L.jobText.y) < 1);
    const charblock::Words* pace = wordsOf(w, "+4,2\xC2\xA0%/год \xC2\xB7 \xE2\x89\x88 13 год");
    CHECK(pace && pace->align == charblock::Align::Right && std::fabs(pace->box.y - L.baseText.y) < 1);
    panel::Picture p = canvas(L);
    charblock::paint(p, L, d, t, charblock::Show{});
    const uint32_t violet = pixel(p, L.expBar.x + L.expBar.w * 0.25f, L.expBar.y + L.expBar.h / 2);
    CHECK(blue(violet) > 0xE0 && red(violet) > 0x98 && green(violet) < red(violet));  // #B7A6FF
    const uint32_t teal = pixel(p, L.jobBar.x + L.jobBar.w * 0.55f, L.jobBar.y + L.jobBar.h / 2);
    CHECK(green(teal) > 0xC0 && red(teal) < 0x90);  // #7FD3C3
    CHECK(green(pixel(p, L.jobBar.x + L.jobBar.w * 0.9f, L.jobBar.y + L.jobBar.h / 2)) < 0x60);  // past 71,8 %
    d.jobNext = 0;  // the job's last level
    d.jobNow = 0;
    const std::vector<charblock::Words> top = charblock::words(L, d, t, charblock::Show{});
    CHECK(hasText(top, "макс.") && !hasText(top, "0 / 0"));
    d.baseNow = d.baseNext = -1;  // the numbers not known: the per cent alone
    const std::vector<charblock::Words> part = charblock::words(L, d, t, charblock::Show{});
    CHECK(hasText(part, "46,3\xC2\xA0%") && !wordsOf(part, "+4,2\xC2\xA0%/год \xC2\xB7 \xE2\x89\x88 13 год"));
}

// 3.0.11-beta.141: the weight in the game's numbers on the info line (by the server's state's colour), the free skill
// points beside the stat points on their own line
TEST(char_block_weight_and_points_lines) {
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    charblock::Data d = soraInNumbers();
    d.weightNow = 1710;
    d.weightMost = 4500;
    d.overweight = 0;
    const std::vector<charblock::Words> w = charblock::words(L, d, texts(), charblock::Show{});
    // 3.0.11-beta.235: its per cent by its bar on the info line, the numbers in its tip
    const charblock::Words* weight = wordsOf(w, "38\xC2\xA0%");
    CHECK(weight && std::fabs(weight->box.y - L.info.y) < 1 && weight->box.x > L.weightBar.x + L.weightBar.w);
    charblock::Show tip;
    tip.hoverZone = int(charblock::Zone::Weight);
    CHECK(hasText(charblock::words(L, d, texts(), tip), "Вага 1\xC2\xA0" "710 / 4\xC2\xA0" "500 \xC2\xB7 38\xC2\xA0%"));
    const charblock::Words* points = wordsOf(w, "+3 очки");
    const charblock::Words* skills = wordsOf(w, "+2 очки навичок");
    CHECK(points && skills && std::fabs(points->box.y - L.points.y) < 1 && skills->box.x > points->box.x);
}

// 3.0.11-beta.141: «и побольше подробностей» - pointed at an experience row, its tip: the per cent and numbers, what is
// left to the level, the pace an hour, the time to the level, this session's gain, the last gain
TEST(char_block_experience_tip) {
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    charblock::Show sh;
    sh.hoverExp = 0;
    sh.hoverBlock = true;
    const std::vector<charblock::Words> w = charblock::words(L, soraInNumbers(), texts(), sh);
    CHECK(hasText(w, "Base 46,3\xC2\xA0% \xC2\xB7 1\xC2\xA0" "234\xC2\xA0" "567 / 2\xC2\xA0" "666\xC2\xA0" "000"));
    CHECK(hasText(w, "До рівня 1\xC2\xA0" "431\xC2\xA0" "433"));
    CHECK(hasText(w, "За годину +111\xC2\xA0" "972 \xC2\xB7 4,2\xC2\xA0%"));
    CHECK(hasText(w, "Рівень через \xE2\x89\x88 13 год"));
    CHECK(hasText(w, "За сесію +328\xC2\xA0" "000 \xC2\xB7 12,3\xC2\xA0%"));
    CHECK(hasText(w, "Остання здобич +3\xC2\xA0" "456"));
    sh.tip = false;  // not before its while
    CHECK(!hasText(charblock::words(L, soraInNumbers(), texts(), sh), "До рівня 1\xC2\xA0" "431\xC2\xA0" "433"));
    sh.tip = true;
    sh.hoverExp = 1;  // the job's: no pace known, none gained
    const std::vector<charblock::Words> j = charblock::words(L, soraInNumbers(), texts(), sh);
    CHECK(hasText(j, "Job 71,8\xC2\xA0% \xC2\xB7 71\xC2\xA0" "800 / 100\xC2\xA0" "000") &&
          hasText(j, "До рівня 28\xC2\xA0" "200"));
    CHECK(std::none_of(j.begin(), j.end(),
                       [](const charblock::Words& x) { return x.text.rfind("За годину", 0) == 0; }));
}

// 3.0.11-beta.141: the portrait's tip tells Alt+V (the full block folds, a small one unfolds); the compact block and
// the rings tell the experience in their tips
TEST(char_block_alt_v_and_experience_in_tips) {
    using V = charblock::View;
    charblock::Show sh;
    sh.hoverPortrait = sh.hoverBlock = true;
    const charblock::Data d = soraInNumbers();
    const std::vector<charblock::Words> full =
        charblock::words(charblock::layout(V::Full, charblock::Look::Bars, 0, 0, 1.0f), d, texts(), sh);
    CHECK(hasText(full, "Alt+V \xE2\x80\x94 згорнути"));
    const std::vector<charblock::Words> compact =
        charblock::words(charblock::layout(V::Compact, charblock::Look::Bars, 0, 0, 1.0f), d, texts(), sh);
    CHECK(hasText(compact, "Alt+V \xE2\x80\x94 розгорнути"));
    sh.hoverPortrait = false;  // the compact block's body: its numbers
    const std::vector<charblock::Words> body =
        charblock::words(charblock::layout(V::Compact, charblock::Look::Bars, 0, 0, 1.0f), d, texts(), sh);
    CHECK(hasText(body, "Base 46,3\xC2\xA0% \xC2\xB7 1\xC2\xA0" "234\xC2\xA0" "567 / 2\xC2\xA0" "666\xC2\xA0" "000"));
    CHECK(hasText(body, "Job 71,8\xC2\xA0% \xC2\xB7 71\xC2\xA0" "800 / 100\xC2\xA0" "000"));
    const std::vector<charblock::Words> rings =
        charblock::words(charblock::layout(V::Compact, charblock::Look::Rings, 0, 0, 1.0f), d, texts(), sh);
    CHECK(hasText(rings, "Base 46,3\xC2\xA0% \xC2\xB7 1\xC2\xA0" "234\xC2\xA0" "567 / 2\xC2\xA0" "666\xC2\xA0" "000"));
    CHECK(hasText(rings, "Alt+V \xE2\x80\x94 розгорнути"));
}

// 3.0.11-beta.141: the time to the level - minutes under an hour, hours and minutes under ten hours, whole hours on
TEST(char_block_durations) {
    CHECK(charblock::duration(50, "год", "хв") == "50 хв" && charblock::duration(0, "год", "хв") == "1 хв");
    CHECK(charblock::duration(125, "год", "хв") == "2 год 5 хв" && charblock::duration(120, "год", "хв") == "2 год");
    CHECK(charblock::duration(768, "год", "хв") == "13 год" && charblock::duration(70000, "год", "хв") == "> 999 год");
}

// 3.0.11-beta.195 (the tester: «щось написано під base - job ... щось злиплося» - «Людмила Павличенко», a
// Gunslinger): the name and the job keep off the levels on the full card's first line - the job left out where it
// does not fit, the name cut short with «…» where even it does not
TEST(char_block_long_name_keeps_off_the_levels) {
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    const auto line = [&](const charblock::Data& d) {
        std::vector<charblock::Words> out;
        for (const charblock::Words& w : charblock::words(L, d, texts(), charblock::Show{}))
            if (w.box.y < L.name.y + L.name.h && w.box.y + w.box.h > L.name.y) out.push_back(w);
        return out;
    };
    const auto levelsOf = [](const std::vector<charblock::Words>& ws) -> const charblock::Words* {
        for (const charblock::Words& w : ws)
            if (w.text.rfind("Base ", 0) == 0) return &w;
        return nullptr;
    };
    charblock::Data d = sora();
    d.name = "\xD0\x9B\xD1\x8E\xD0\xB4\xD0\xBC\xD0\xB8\xD0\xBB\xD0\xB0 \xD0\x9F\xD0\xB0\xD0\xB2\xD0\xBB\xD0\xB8\xD1\x87"
             "\xD0\xB5\xD0\xBD\xD0\xBA\xD0\xBE";  // Людмила Павличенко
    d.job = "Gunslinger";
    d.baseLevel = 62;
    d.jobLevel = 38;
    std::vector<charblock::Words> ws = line(d);
    const charblock::Words* lv = levelsOf(ws);
    CHECK(lv != nullptr);
    bool named = false;
    for (const charblock::Words& w : ws) {
        if (lv && &w != lv) CHECK(w.box.x + panel::wordsWidth(w.text, w.px, w.bold) <= lv->box.x + 0.5f);
        named = named || w.text == d.name;
    }
    CHECK(named);  // the name itself whole: only the job gave way
    d.name = "\xD0\x9B\xD1\x8E\xD0\xB4\xD0\xBC\xD0\xB8\xD0\xBB\xD0\xB0 \xD0\x9F\xD0\xB0\xD0\xB2\xD0\xBB\xD0\xB8\xD1\x87"
             "\xD0\xB5\xD0\xBD\xD0\xBA\xD0\xBE \xD0\x9F\xD0\xB0\xD0\xB2\xD0\xBB\xD0\xB8\xD1\x87\xD0\xB5\xD0\xBD\xD0\xBA"
             "\xD0\xBE\xD0\xB2\xD0\xB0";  // a name longer than the line
    ws = line(d);
    lv = levelsOf(ws);
    CHECK(lv != nullptr);
    bool cut = false;
    for (const charblock::Words& w : ws) {
        if (lv && &w != lv) CHECK(w.box.x + panel::wordsWidth(w.text, w.px, w.bold) <= lv->box.x + 0.5f);
        cut = cut || (w.text.size() > 3 && w.text.compare(w.text.size() - 3, 3, "\xE2\x80\xA6") == 0);
        CHECK(w.text != d.job);
    }
    CHECK(cut);
    d = sora();  // a short one: its job beside it as before
    ws = line(d);
    CHECK(std::any_of(ws.begin(), ws.end(), [&](const charblock::Words& w) { return w.text == d.job; }));
}

// 3.0.11-beta.235 (the concept «Блок персонажа»: «больше информации», «подсказки когда наводишь на непонятное»): the
// guild's and the party's line, the regen under HP and SP, the battle's four cells (a click opens «Характеристики»),
// the session in place of the experience rows at the last levels; a tip on each
TEST(char_block_more_lines_and_their_tips) {
    charblock::Texts t = texts();
    t.guild = "Гільдія {1}";
    t.party = "Група {1}";
    t.regen = "+{1} кожні {2} с";
    t.regenFull = "повне";
    t.regenToFull = "до повного \xE2\x89\x88 {1}";
    t.regenNone = "не відновлюється: вага";
    t.sessionHead = "Сесія \xC2\xB7 {1}";
    t.killed = "вбито";
    t.deaths = "смертей";
    t.tipAspd = "{1} удару за секунду; межа сервера 190";
    t.tipOpen = "Клік відкриває «Характеристики»";
    t.tipKills = "Монстри, яких ти бив і які загинули за сесію";
    t.tipOnline = "{1} з {2} у грі";
    charblock::Data d = soraInNumbers();
    d.guild = "Heroes";
    d.party = "Maze";
    d.partyOnline = 3;
    d.partyTotal = 4;
    d.hp = 900;
    d.hpMost = 1000;
    d.hpRegen = 10;
    d.spRegen = 4;
    d.sp = d.spMost = 200;
    d.atk = 213;
    d.atk2 = 180;
    d.def = 45;
    d.def2 = 65;
    d.flee = 189;
    d.flee2 = 1;
    d.aspd = 182;
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    CHECK(!L.sessionShown && L.baseRow.w > 0 && L.battle[3].x > L.battle[0].x && L.buttons[0].y > L.jobRow.y);
    std::vector<charblock::Words> w = charblock::words(L, d, t, charblock::Show{});
    CHECK(hasText(w, "Гільдія Heroes") && hasText(w, "Група Maze \xC2\xB7 3/4"));
    CHECK(hasText(w, "+10 кожні 6 с") && hasText(w, "до повного \xE2\x89\x88 1 хв"));  // 10 ticks of 6 s
    CHECK(hasText(w, "+4 кожні 8 с") && hasText(w, "повне"));
    CHECK(hasText(w, "213+180") && hasText(w, "ASPD") && hasText(w, "182"));
    const panel::Box& aspd = L.battle[3];
    const charblock::Hit h = charblock::hitTest(L, aspd.x + 4, aspd.y + 4);
    CHECK(h.part == charblock::Part::Zone && charblock::opensStats(charblock::Zone(h.index)));
    charblock::Show sh;
    sh.hoverZone = h.index;
    w = charblock::words(L, d, t, sh);
    CHECK(hasText(w, "2,78 удару за секунду; межа сервера 190") && hasText(w, "Клік відкриває «Характеристики»"));
    d.overweight = 1;  // from 70 %: no regen
    CHECK(hasText(charblock::words(L, d, t, charblock::Show{}), "не відновлюється: вага"));
    // the last levels: the session in place of the experience
    d.overweight = 0;
    d.baseNext = d.jobNext = 0;
    d.sessionMinutes = 134;
    d.zenyGain = 125400;
    d.kills = 312;
    d.deaths = 3;
    CHECK(charblock::lastLevels(d));
    const charblock::Layout S = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f, true);
    CHECK(S.sessionShown && S.baseRow.w == 0 && S.session[2].w > 0 && S.buttons[0].y > S.session[0].y);
    w = charblock::words(S, d, t, charblock::Show{});
    CHECK(hasText(w, "Сесія \xC2\xB7 2 год 14 хв") && hasText(w, "+125\xC2\xA0" "400 z") && hasText(w, "312") &&
          hasText(w, "вбито"));
    sh.hoverZone = int(charblock::Zone::Kills);
    CHECK(hasText(charblock::words(S, d, t, sh), "Монстри, яких ти бив і які загинули за сесію"));
    sh.hoverZone = int(charblock::Zone::Social);
    CHECK(hasText(charblock::words(S, d, t, sh), "3 з 4 у грі"));
}

// 3.0.11-beta.236 (the concept «Блок персонажа»): the rings' dots their own tips (the free points', the weight's);
// the experience's strip at the screen's bottom - the base's bar over the job's, its tip
TEST(char_block_ring_dots_and_the_bottom_strip) {
    charblock::Texts t = texts();
    t.tipPoints = "Вільні очки характеристик: Alt+A";
    t.tipWeight = "70 %: HP і SP не відновлюються";
    charblock::Data d = soraInNumbers();
    const charblock::Layout R = charblock::layout(charblock::View::Compact, charblock::Look::Rings, 0, 0, 1.0f);
    const panel::Box& pd = R.pointsDot;
    charblock::Hit h = charblock::hitTest(R, pd.x + pd.w / 2, pd.y + pd.h / 2);
    CHECK(h.part == charblock::Part::Zone && h.index == int(charblock::Zone::Points));
    charblock::Show sh;
    sh.hoverZone = h.index;
    std::vector<charblock::Words> w = charblock::words(R, d, t, sh);
    CHECK(hasText(w, "+3 очки") && hasText(w, "Вільні очки характеристик: Alt+A"));
    const panel::Box& wd = R.weightDot;
    h = charblock::hitTest(R, wd.x + wd.w / 2, wd.y + wd.h / 2);
    CHECK(h.part == charblock::Part::Zone && h.index == int(charblock::Zone::Weight));
    CHECK(charblock::hitTest(R, R.cx, R.cy).part == charblock::Part::Portrait);
    // the strip: 8 px at scale 1, the base's 46,3 % lit, past it dark; the job's under it
    CHECK(charblock::stripHeight(1.0f) == 8 && charblock::stripHeight(2.0f) == 16);
    panel::Picture p;
    p.width = 1000;
    p.height = charblock::stripHeight(1.0f);
    p.px.assign(size_t(p.width) * size_t(p.height), 0u);
    charblock::paintStrip(p, d, 1.0f);
    const auto red = [](uint32_t c) { return (c >> 16) & 0xFF; };
    CHECK(red(pixel(p, 250, 2)) > 0x90 && red(pixel(p, 650, 2)) < 0x30);  // off the 10 % ticks
    const std::vector<std::string> tip = charblock::stripTip(d, t);
    CHECK(tip.size() == 2 && tip[0].rfind("Base 46,3", 0) == 0 && tip[1].rfind("Job", 0) == 0);
    // 3.0.18-beta.23: uaUI's strip - the same bars as the picture's (each its fill, colour and opacity)
    const std::array<charblock::StripBar, 2> bars = charblock::stripBars(d);
    CHECK(std::fabs(bars[0].fill - 0.463f) < 0.001f && bars[0].alpha == 1.0f && bars[0].rgb == 0xB7A6FFu);
    CHECK(bars[1].rgb == 0x7FD3C3u);
    charblock::Data last = d;
    last.jobNext = 0;  // the job's last level: full, faint
    CHECK(charblock::stripBars(last)[1].fill == 1.0f && charblock::stripBars(last)[1].alpha == 0.45f);
}

// 3.0.11-beta.238 (the owner: «когда сидишь, реген совсем другой»; «желтеньким сколько примерно % заняло убийство
// последнего моба»): a minute's regen as the server gives it - the natural twice as often sitting, the skills' each
// 10 s; the last fight's cost in yellow after each bar's fill, its words in the time to full's place
TEST(char_block_regen_sitting_skills_and_the_fight) {
    charblock::Texts t = texts();
    t.regenPerMin = "+{1}/хв \xC2\xB7 {2}";
    t.standing = "стоячи";
    t.sitting = "сидячи";
    t.regenNatural = "природне +{1} кожні {2} с";
    t.regenSkill = "Від навички: +{1} кожні 10 с";
    t.fightCost = "останній бій −{1}";
    t.regenFull = "повне";
    charblock::Data d = soraInNumbers();
    d.hp = 5000;
    d.hpMost = 15137;
    d.hpRegen = 88;   // 1 + VIT/5 + MaxHP/200
    d.hpSkill = 352;  // Increase HP Recovery 10
    d.sp = d.spMost = 477;
    d.spRegen = 10;
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    std::vector<charblock::Words> w = charblock::words(L, d, t, charblock::Show{});
    CHECK(hasText(w, "+2\xC2\xA0" "992/хв \xC2\xB7 стоячи"));  // 88 x 10 + 352 x 6
    d.sitting = true;
    w = charblock::words(L, d, t, charblock::Show{});
    CHECK(hasText(w, "+3\xC2\xA0" "872/хв \xC2\xB7 сидячи"));  // 88 x 20 + 352 x 6
    charblock::Show sh;
    sh.hoverZone = int(charblock::Zone::HpRegen);
    w = charblock::words(L, d, t, sh);
    CHECK(hasText(w, "HP природне +88 кожні 3 с") && hasText(w, "Від навички: +352 кожні 10 с"));
    // the fight: 12 % of HP spent - its words and its yellow after the fill
    d.hpCost = 0.12f;
    d.spCost = 0;
    d.costShown = 1;
    w = charblock::words(L, d, t, charblock::Show{});
    CHECK(hasText(w, "останній бій −12,0\xC2\xA0%"));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    charblock::paint(p, L, d, t, charblock::Show{});
    const float fill = 5000.0f / 15137.0f;
    const uint32_t y = pixel(p, L.hpBar.x + L.hpBar.w * (fill + 0.06f), L.hpBar.y + L.hpBar.h / 2);
    CHECK(((y >> 16) & 0xFF) > 0xB0 && ((y >> 8) & 0xFF) > 0x90 && (y & 0xFF) < 0x90);  // yellow
}

// 3.0.11-beta.239 (the owner: «в самом диалоге по опыту ничего не видно»): the last gain on the block's experience rows
// - lit at the end of each fill, its amount and part of the level in the pace's place
TEST(char_block_the_last_gain_on_the_experience_rows) {
    charblock::Data d = soraInNumbers();  // base 1 234 567 of 2 666 000
    d.gainBase = 26660;                   // 1 % of the level
    d.gainShown = 1;
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    const std::vector<charblock::Words> w = charblock::words(L, d, texts(), charblock::Show{});
    CHECK(hasText(w, "+26\xC2\xA0" "660 (1,00\xC2\xA0%)"));
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    charblock::paint(p, L, d, texts(), charblock::Show{});
    const float share = 1234567.0f / 2666000.0f;
    const uint32_t lit = pixel(p, L.expBar.x + L.expBar.w * (share - 0.005f), L.expBar.y + L.expBar.h / 2);
    CHECK(((lit >> 16) & 0xFF) > 0xD0 && ((lit >> 8) & 0xFF) > 0xB0);  // yellow
    d.gainShown = 0;
    CHECK(!hasText(charblock::words(L, d, texts(), charblock::Show{}), "+26\xC2\xA0" "660 (1,00\xC2\xA0%)"));
}

// 3.0.18-beta.17 (the owner: «Переписывать ... в разметку RmlUi вручную»): the RML block's words are the picture's
TEST(char_block_model_says_what_the_words_say) {
    const charblock::Data d = soraInNumbers();
    const charblock::Texts t = texts();
    const charblock::Layout L = charblock::layout(charblock::View::Full, charblock::Look::Bars, 0, 0, 1.0f);
    const std::vector<charblock::Words> w = charblock::words(L, d, t, charblock::Show{});
    const charblock::Model m = charblock::model(d, t, charblock::View::Full, charblock::Look::Bars, false);
    CHECK(hasText(w, m.levels) && hasText(w, m.hp) && hasText(w, m.sp) && hasText(w, m.zeny));
    CHECK(hasText(w, m.weightPct) && m.weightTone.empty() && m.weightShare == 0.38f);
    CHECK(m.points.size() == 2 && m.points[0].tone == "gold" && hasText(w, m.points[0].text));
    CHECK(m.baseExp.shown && hasText(w, m.baseExp.pct) && hasText(w, m.baseExp.numbers));
    CHECK(m.jobExp.shown && hasText(w, m.jobExp.pct));
    CHECK(!m.low && m.letter == "S" && m.badge == "72");
    CHECK(m.hpShare > 0.82f && m.hpShare < 0.83f);
    CHECK(!m.baseExp.tip.empty());
    // low HP: the warning's words
    charblock::Data low = d;
    low.hp = 400;
    const charblock::Model ml = charblock::model(low, t, charblock::View::Full, charblock::Look::Bars, false);
    CHECK(ml.low && ml.lowText == "! Мало HP \xC2\xB7 10 %");  // 400 of 4 120 rounded
}
