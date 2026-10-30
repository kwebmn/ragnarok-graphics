// 3.0.11-beta.48: the cast's label over the hero (core/cast_bar) - its pill and bar, its words, the seconds left
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/cast_bar.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(cast_bar_layout_picture_and_words) {
    panel::Picture icon;
    icon.width = icon.height = 22;
    icon.px.assign(22 * 22, 0xFFC04020u);
    castbar::Data d;
    d.name = "Fire Bolt";
    d.left = "0,8 \xD1\x81";
    d.done = 0.6f;
    d.icon = &icon;
    const castbar::Layout L = castbar::layout(d, 960, 600, 1.0f);
    CHECK(L.y + int(std::lround(L.bar.y)) == 600);                 // the bar where the game's gauge stood
    CHECK(std::abs(L.x + L.w / 2 - 960) <= 1);                     // centred on it
    CHECK(std::fabs(L.bar.w - 150) < 1e-3f && std::fabs(L.bar.h - 7) < 1e-3f);
    CHECK(L.pill.y + L.pill.h <= L.bar.y && L.icon.x >= L.pill.x && L.name.x > L.icon.x + L.icon.w);
    CHECK(L.left.x > L.name.x + L.name.w - 1e-3f && L.left.x + L.left.w <= L.pill.x + L.pill.w + 1e-3f);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    castbar::paint(p, L, d);
    const auto at = [&](float x, float y) { return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))]; };
    const uint32_t filled = at(L.bar.x + L.bar.w * 0.3f, L.bar.y + L.bar.h * 0.75f);
    const uint32_t empty = at(L.bar.x + L.bar.w * 0.9f, L.bar.y + L.bar.h / 2);
    CHECK(((filled >> 16) & 0xFF) > 0xA0 && ((filled >> 8) & 0xFF) > 0x80);  // gold
    CHECK(((empty >> 16) & 0xFF) < 0x40 && (empty >> 24) > 0x80);             // the dark track
    const uint32_t ic = at(L.icon.x + L.icon.w / 2, L.icon.y + L.icon.h / 2);
    CHECK(((ic >> 16) & 0xFF) > 0xA0 && (ic & 0xFF) < 0x40);  // the icon
    const uint32_t pill = at(L.pill.x + L.pill.w - 3, L.pill.y + L.pill.h / 2);
    CHECK((pill >> 24) > 0xC0 && (pill >> 24) < 0xF0);  // 86 %
    const std::vector<castbar::Words> w = castbar::words(L, d);
    CHECK(w.size() == 2 && w[0].text == "Fire Bolt" && w[1].text == "0,8 \xD1\x81");
    castbar::Data unknown = d;  // the skill not known: the time alone
    unknown.name.clear();
    unknown.icon = nullptr;
    const castbar::Layout U = castbar::layout(unknown, 960, 600, 1.0f);
    CHECK(U.pill.w < L.pill.w && castbar::words(U, unknown).size() == 1);
    const castbar::Layout B = castbar::layout(d, 960, 600, 2.0f);
    CHECK(std::fabs(B.bar.w - 300) < 1e-3f && B.y + int(std::lround(B.bar.y)) == 600);
}

TEST(cast_bar_seconds) {
    CHECK(castbar::seconds(750, ',', "\xD1\x81") == "0,8\xC2\xA0\xD1\x81");
    CHECK(castbar::seconds(800, ',', "s") == "0,8\xC2\xA0s" && castbar::seconds(801, '.', "s") == "0.9\xC2\xA0s");
    CHECK(castbar::seconds(2340, ',', "s") == "2,4\xC2\xA0s" && castbar::seconds(0, ',', "s") == "0,0\xC2\xA0s");
    CHECK(castbar::seconds(-50, ',', "s") == "0,0\xC2\xA0s");
}
