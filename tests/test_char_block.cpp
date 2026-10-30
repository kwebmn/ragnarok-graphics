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
    return t;
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
    for (int i = 0; i < charblock::kButtons; ++i) {
        CHECK(inside(L.block, L.buttons[i]) && L.buttons[i].y >= L.baseText.y + L.baseText.h);
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
    CHECK(bars.buttons[0].w == 0 && bars.expBar.w == 0);  // the compact block: no buttons, no experience
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
