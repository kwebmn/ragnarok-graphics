#include "core/osk.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::osk {
namespace {

// At scale 1: a key 46 square, 6 apart; the line typed 40 high over them; 18 round the card
constexpr float kKey = 46, kGap = 6, kPad = 18, kLineH = 40, kHintH = 22;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000;

// The UTF-8 letters of a row, a key each
std::vector<Key> letters(const std::string& utf8) {
    std::vector<Key> out;
    for (size_t i = 0; i < utf8.size();) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        const size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
        if (utf8[i] != ' ') out.push_back(Key{utf8.substr(i, n), Special::None, 1});
        i += n;
    }
    return out;
}

// The rows' letters a grid, lower and upper (the same count each)
struct Letters {
    const char* rows[4];
    const char* upper[4];
};
const Letters kUk = {{"1234567890'", "йцукенгшщзхї", "фівапролджєґ", "ячсмитьбю.,?"},
                     {"1234567890'", "ЙЦУКЕНГШЩЗХЇ", "ФІВАПРОЛДЖЄҐ", "ЯЧСМИТЬБЮ.,!"}};
const Letters kRu = {{"1234567890'", "йцукенгшщзхъ", "фывапролджэё", "ячсмитьбю.,?"},
                     {"1234567890'", "ЙЦУКЕНГШЩЗХЪ", "ФЫВАПРОЛДЖЭЁ", "ЯЧСМИТЬБЮ.,!"}};
const Letters kEn = {{"1234567890'", "qwertyuiop-", "asdfghjkl:;", "zxcvbnm.,?!"},
                     {"1234567890'", "QWERTYUIOP_", "ASDFGHJKL\"/", "ZXCVBNM<>?!"}};
// the symbols the game's code page (1251) has
const Letters kSym = {{"!?@#$%&*()", "-+=/\\\"':;_", "<>[]{}~^|`", "…«»№°·–€©™"},
                      {"!?@#$%&*()", "-+=/\\\"':;_", "<>[]{}~^|`", "…«»№°·–€©™"}};

float rowUnits(const std::vector<Key>& r) {
    float u = 0;
    for (const Key& k : r) u += k.span;
    return u;
}

float widest(const Grid& g) {
    float w = 0;
    for (const auto& r : g) w = std::max(w, rowUnits(r));
    return w;
}

// A key's middle in keys from the grid's left (each row centred)
float middleOf(const Grid& g, int row, int col) {
    const auto& r = g[size_t(row)];
    float x = (widest(g) - rowUnits(r)) / 2;
    for (int i = 0; i < col; ++i) x += r[size_t(i)].span;
    return x + r[size_t(col)].span / 2;
}

}  // namespace

Lang next(Lang l, int by) { return Lang(((int(l) + by) % kLangs + kLangs) % kLangs); }

Grid rows(Lang l, bool shift) {
    const Letters& L = l == Lang::Uk ? kUk : l == Lang::Ru ? kRu : l == Lang::En ? kEn : kSym;
    Grid g;
    for (int i = 0; i < 4; ++i) g.push_back(letters(shift ? L.upper[i] : L.rows[i]));
    g.push_back({Key{"", Special::Shift, 1.5f}, Key{"", Special::Lang, 1.5f}, Key{" ", Special::Space, 5},
                 Key{"", Special::Back, 1.5f}, Key{"", Special::Send, 2}});
    return g;
}

Focus move(const Grid& g, Focus f, int dx, int dy) {
    if (g.empty()) return f;
    f.row = std::clamp(f.row, 0, int(g.size()) - 1);
    f.col = std::clamp(f.col, 0, int(g[size_t(f.row)].size()) - 1);
    if (dx) {
        const int n = int(g[size_t(f.row)].size());
        f.col = ((f.col + dx) % n + n) % n;
    }
    if (dy) {
        const float x = middleOf(g, f.row, f.col);
        const int rows = int(g.size());
        f.row = ((f.row + dy) % rows + rows) % rows;
        const auto& r = g[size_t(f.row)];
        float at = (widest(g) - rowUnits(r)) / 2;
        int best = 0;
        float bestDist = 1e9f;
        for (size_t i = 0; i < r.size(); ++i) {
            const float from = at, to = at + r[i].span;
            const float d = x < from ? from - x : x > to ? x - to : 0;
            if (d < bestDist) {
                bestDist = d;
                best = int(i);
            }
            at = to;
        }
        f.col = best;
    }
    return f;
}

const Key& keyAt(const Grid& g, Focus f) {
    static const Key none;
    if (f.row < 0 || f.row >= int(g.size())) return none;
    const auto& r = g[size_t(f.row)];
    return f.col >= 0 && f.col < int(r.size()) ? r[size_t(f.col)] : none;
}

Layout layout(const Grid& g, int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float unit = (kKey + kGap) * s, pad = kPad * s;
    const float w = widest(g) * unit - kGap * s + 2 * pad;
    L.line = panel::Box{pad, pad, w - 2 * pad, kLineH * s};
    float y = pad + kLineH * s + 10 * s;
    for (const auto& r : g) {
        std::vector<panel::Box> boxes;
        float x = pad + (widest(g) - rowUnits(r)) / 2 * unit;
        for (const Key& k : r) {
            boxes.push_back(panel::Box{x, y, k.span * unit - kGap * s, kKey * s});
            x += k.span * unit;
        }
        L.keys.push_back(std::move(boxes));
        y += unit;
    }
    y += 2 * s;
    L.hint = panel::Box{pad, y, w - 2 * pad, kHintH * s};
    y += kHintH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::clamp(targetW / 2 - L.w / 2, 0, std::max(0, targetW - L.w));
    L.y = std::clamp(targetH - L.h - int(std::lround(120 * s)), 0, std::max(0, targetH - L.h));
    return L;
}

void paint(panel::Picture& out, const Layout& L, const Grid& g, Focus f, bool shift) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.line, 8 * s, kBlack, 0.35f, kGold, 0.6f, 1.0f * s);
    for (size_t r = 0; r < L.keys.size() && r < g.size(); ++r)
        for (size_t c = 0; c < L.keys[r].size() && c < g[r].size(); ++c) {
            const Key& k = g[r][c];
            const bool on = int(r) == f.row && int(c) == f.col;
            const bool lit = k.special == Special::Send || (k.special == Special::Shift && shift);
            panel::roundRect(out, L.keys[r][c], 8 * s, on ? kGold : lit ? kGold : kWhite,
                             on ? 0.34f : lit ? 0.16f : 0.06f, on ? kGold : kWhite, on ? 0.9f : 0.14f, 1.0f * s);
        }
}

std::vector<Words> words(const Layout& L, const Grid& g, Focus f, const std::string& line, bool caret,
                         const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{panel::Box{L.line.x + 12 * s, L.line.y, L.line.w - 24 * s, L.line.h},
                        line + (caret ? "|" : ""), kText, 15 * s, false, Align::Left});
    for (size_t r = 0; r < L.keys.size() && r < g.size(); ++r)
        for (size_t c = 0; c < L.keys[r].size() && c < g[r].size(); ++c) {
            const Key& k = g[r][c];
            const bool on = int(r) == f.row && int(c) == f.col;
            std::string text = k.text;
            switch (k.special) {
                case Special::Shift: text = "\xE2\x87\xA7"; break;  // ⇧
                case Special::Space: text = t.space; break;
                case Special::Back: text = "\xE2\x8C\xAB"; break;  // ⌫
                case Special::Lang: text = t.lang; break;
                case Special::Send: text = t.send; break;
                case Special::None: break;
            }
            out.push_back(Words{L.keys[r][c], text, on ? kGold2 : kText, (k.special == Special::None ? 16 : 12) * s,
                                on || k.special == Special::Send, Align::Center});
        }
    out.push_back(Words{L.hint, t.hint, kMuted, 11.5f * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::osk
