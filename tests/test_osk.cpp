// 3.0.11-beta.214 (the owner: «3. можно» - the grid keyboard): the controller's on-screen keyboard (core/osk) - the
// Ukrainian, Russian, English and symbols' grids, the D-pad moving over them, the special keys in the last row
#include <algorithm>
#include <string>
#include <vector>

#include "core/osk.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool hasKey(const std::vector<std::vector<osk::Key>>& rows, const std::string& t) {
    for (const auto& r : rows)
        for (const osk::Key& k : r)
            if (k.text == t) return true;
    return false;
}

}  // namespace

// Each grid has its own letters; Shift makes them capitals; the last row the specials
TEST(osk_grids) {
    const auto uk = osk::rows(osk::Lang::Uk, false);
    CHECK(hasKey(uk, "ї") && hasKey(uk, "є") && hasKey(uk, "ґ") && hasKey(uk, "і") && !hasKey(uk, "ы"));
    CHECK(hasKey(osk::rows(osk::Lang::Uk, true), "Ї") && hasKey(osk::rows(osk::Lang::Uk, true), "Й"));
    const auto ru = osk::rows(osk::Lang::Ru, false);
    CHECK(hasKey(ru, "ы") && hasKey(ru, "э") && hasKey(ru, "ъ") && hasKey(ru, "ё") && !hasKey(ru, "ї"));
    const auto en = osk::rows(osk::Lang::En, false);
    CHECK(hasKey(en, "q") && hasKey(en, "m") && hasKey(osk::rows(osk::Lang::En, true), "Q"));
    const auto sym = osk::rows(osk::Lang::Symbols, false);
    CHECK(hasKey(sym, "!") && hasKey(sym, "?") && hasKey(sym, "@") && hasKey(sym, "%"));
    for (const osk::Lang l : {osk::Lang::Uk, osk::Lang::Ru, osk::Lang::En, osk::Lang::Symbols}) {
        const auto grid = osk::rows(l, false);
        CHECK(hasKey(grid, "1") || l == osk::Lang::Symbols);
        const auto& last = grid.back();
        const auto special = [&](osk::Special s) {
            return std::any_of(last.begin(), last.end(), [&](const osk::Key& k) { return k.special == s; });
        };
        CHECK(special(osk::Special::Shift) && special(osk::Special::Space) && special(osk::Special::Back) &&
              special(osk::Special::Lang) && special(osk::Special::Send));
    }
}

// The D-pad: left and right round a row; up and down to the key under the same place, round the grid
TEST(osk_focus_moves) {
    const auto g = osk::rows(osk::Lang::En, false);
    osk::Focus f{1, 0};
    f = osk::move(g, f, -1, 0);
    CHECK(f.row == 1 && f.col == int(g[1].size()) - 1);
    f = osk::move(g, f, 1, 0);
    CHECK(f.row == 1 && f.col == 0);
    f = osk::move(g, osk::Focus{0, 0}, 0, -1);  // up from the top: the last row
    CHECK(f.row == int(g.size()) - 1);
    // down from the middle of a row lands under it
    const osk::Focus mid{1, int(g[1].size()) / 2};
    const osk::Focus below = osk::move(g, mid, 0, 1);
    CHECK(below.row == 2 && std::abs(below.col - mid.col) <= 1);
    // into the last row: the wide space key under the middle
    osk::Focus to = osk::move(g, osk::Focus{int(g.size()) - 2, int(g[g.size() - 2].size()) / 2}, 0, 1);
    CHECK(osk::keyAt(g, to).special == osk::Special::Space);
}

// The next grid in turn (the symbols last), and back
TEST(osk_next_grid) {
    CHECK(osk::next(osk::Lang::Uk, 1) == osk::Lang::Ru);
    CHECK(osk::next(osk::Lang::Symbols, 1) == osk::Lang::Uk);
    CHECK(osk::next(osk::Lang::Uk, -1) == osk::Lang::Symbols);
}

// At the screen's lower middle; a box a key, the space wide; the line typed over the keys
TEST(osk_layout_and_words) {
    const auto g = osk::rows(osk::Lang::Uk, false);
    const osk::Layout L = osk::layout(g, 1920, 1080, 1.0f);
    CHECK(L.x >= 0 && L.y >= 0 && L.x + L.w <= 1920 && L.y + L.h <= 1080);
    CHECK(L.y > 1080 / 3);
    CHECK_EQ(L.keys.size(), g.size());
    CHECK(L.line.y + L.line.h <= L.keys[0][0].y);
    size_t space = 0;
    for (size_t i = 0; i < g.back().size(); ++i)
        if (g.back()[i].special == osk::Special::Space) space = i;
    CHECK(L.keys.back()[space].w > 3 * L.keys[1][0].w);
    osk::Texts t;
    t.space = "пробіл";
    t.send = "Надіслати";
    t.lang = "УКР";
    t.hint = "B — закрити";
    const std::vector<osk::Words> w = osk::words(L, g, osk::Focus{1, 2}, "Привіт", true, t);
    const auto has = [&](const std::string& x) {
        return std::any_of(w.begin(), w.end(), [&](const osk::Words& y) { return y.text == x; });
    };
    CHECK(has("Привіт|") && has("пробіл") && has("Надіслати") && has("УКР") && has("B — закрити"));
    CHECK(has(g[1][2].text));
}
