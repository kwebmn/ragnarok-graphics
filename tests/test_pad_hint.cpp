// 3.0.11-beta.92 (the concept «Контроллер в интерфейсе uaRO», stage 6): the controller's hint line in the interface's
// style - its parts in a glass bar, the buttons as glyphs (core/pad_hint)
#include <algorithm>
#include <string>
#include <vector>

#include "core/pad_hint.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(pad_hint_bar) {
    // ↑ ↓: вибір · A: OK · утримати B: закрити (padglyph::Button: Up 8, Down 10, A 0, B 1)
    const std::vector<padhint::Part> parts = {{"", {8, 10}, "вибір"}, {"", {0}, "OK"}, {"утримати", {1}, "закрити"}};
    const padhint::Layout L = padhint::layout(parts, 960, 900, 1.0f);
    CHECK(L.x + L.w / 2 == 960 && L.y + L.h == 900 && L.parts.size() == 3);
    CHECK(L.parts[0].glyphs.size() == 2 && L.parts[1].glyphs[0].x > L.parts[0].words.x + L.parts[0].words.w);
    CHECK(L.parts[2].before.w > 0 && L.parts[2].glyphs[0].x > L.parts[2].before.x);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    padhint::paint(p, L, parts, false);
    const panel::Box& a = L.parts[1].glyphs[0];
    uint32_t ring = 0;  // A's green ring
    for (int y = int(a.y); y < int(a.y) + 3; ++y)
        ring = std::max(ring, (p.px[size_t(y) * size_t(p.width) + size_t(int(a.x + a.w / 2))] >> 8) & 0xFFu);
    CHECK(ring > 0x90);
    const std::vector<padhint::Words> w = padhint::words(L, parts, false);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const padhint::Words& x) { return x.text == text; });
    };
    CHECK(has("вибір") && has("OK") && has("утримати") && has("закрити") && has("A") && has("B"));
}
