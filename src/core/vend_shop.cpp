#include "core/vend_shop.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::vendshop {
namespace {

// At scale 1: the card 520 wide, 18 in; its title 34; a row 44 (8 at most shown); the sums 46; the buttons 40
constexpr float kW = 520, kPad = 18, kTitleH = 34, kRowH = 44, kSumsH = 46, kButtonH = 40, kGap = 10, kIcon = 32;
constexpr size_t kMostRows = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000;

void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    if (out.scene) {  // 3.0.18-beta.12: recorded for uaUI
        panel::recordImage(out, pic, float(ox), float(oy), 1.0f);
        return;
    }
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (!(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

struct Cells {  // a row's inner parts
    panel::Box icon, name, stock, price;
};
Cells cells(const panel::Box& line, const panel::Box& minus, float s) {
    Cells c;
    c.icon = panel::Box{line.x + 6 * s, line.y + (line.h - kIcon * s) / 2, kIcon * s, kIcon * s};
    const float nx = c.icon.x + c.icon.w + 10 * s, right = minus.x - 10 * s;
    c.price = panel::Box{right - 110 * s, line.y, 110 * s, line.h};
    c.name = panel::Box{nx, line.y + 4 * s, c.price.x - nx - 6 * s, line.h / 2 - 2 * s};
    c.stock = panel::Box{nx, line.y + line.h / 2, c.price.x - nx - 6 * s, line.h / 2 - 4 * s};
    return c;
}

}  // namespace

long long total(const std::vector<Row>& rows) {
    long long t = 0;
    for (const Row& r : rows) t += r.price * r.chosen;
    return t;
}

bool canAdd(const Row& r, long long purchase, long long zeny) {
    return r.chosen < r.stock && purchase + r.price <= zeny;
}

bool canRemove(const Row& r) { return r.chosen > 0; }

std::string money(long long zeny) {
    std::string digits = std::to_string(zeny < 0 ? -zeny : zeny), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out += ' ';
        out += digits[i];
    }
    return zeny < 0 ? "-" + out : out;
}

Layout layout(int targetW, int targetH, float scale, size_t rows) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, row = kRowH * s;
    L.title = panel::Box{pad, pad, w - 2 * pad - 30 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 26 * s, pad + 4 * s, 26 * s, 26 * s};
    const size_t shown = std::min(rows, kMostRows);
    const float top = pad + kTitleH * s + 6 * s;
    L.list = panel::Box{pad, top, w - 2 * pad, row * float(std::max<size_t>(1, shown))};
    for (size_t i = 0; i < shown; ++i) {
        const panel::Box line{pad, top + float(i) * row, w - 2 * pad, row - 4 * s};
        L.lines.push_back(line);
        const float bs = 26 * s, by = line.y + (line.h - bs) / 2;
        const panel::Box plus{line.x + line.w - 6 * s - bs, by, bs, bs};
        const panel::Box count{plus.x - 40 * s, line.y, 40 * s, line.h};
        const panel::Box minus{count.x - bs, by, bs, bs};
        L.plus.push_back(plus);
        L.counts.push_back(count);
        L.minus.push_back(minus);
    }
    float y = top + L.list.h + 8 * s;
    L.sums = panel::Box{pad, y, w - 2 * pad, kSumsH * s};
    L.total = panel::Box{pad + 12 * s, y + 2 * s, w - 2 * pad - 24 * s, kSumsH * s / 2 - 2 * s};
    L.zeny = panel::Box{pad + 12 * s, y + kSumsH * s / 2, w - 2 * pad - 24 * s, kSumsH * s / 2 - 4 * s};
    y += kSumsH * s + 8 * s;
    L.hint = panel::Box{pad, y, w - 2 * pad, 16 * s};
    y += 20 * s;
    const float bw = (w - 2 * pad - kGap * s) / 2;
    L.buy = panel::Box{pad, y, bw, kButtonH * s};
    L.cancel = panel::Box{pad + bw + kGap * s, y, bw, kButtonH * s};
    y += kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

int mostScroll(const Layout& L, size_t rows) { return std::max(0, int(rows) - int(L.lines.size())); }

Hit hitTest(const Layout& L, int scroll, size_t rows, float x, float y) {
    for (size_t i = 0; i < L.lines.size(); ++i) {
        const int row = int(i) + scroll;
        if (row >= int(rows)) break;
        if (L.plus[i].contains(x, y)) return Hit{Part::Plus, row};
        if (L.minus[i].contains(x, y)) return Hit{Part::Minus, row};
        if (L.lines[i].contains(x, y)) return Hit{Part::Row, row};
    }
    if (L.buy.contains(x, y)) return Hit{Part::Buy, -1};
    if (L.cancel.contains(x, y)) return Hit{Part::Cancel, -1};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    if (L.title.contains(x, y)) return Hit{Part::Title, -1};
    return Hit{};
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Row>& rows, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 14 * s, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.close, 7 * s, kWhite, sh.hoverPart == int(Part::Close) ? 0.14f : 0.06f);
    const long long sum = total(rows);
    for (size_t i = 0; i < L.lines.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= rows.size()) break;
        const Row& r = rows[at];
        const bool hot = sh.hoverRow == int(at), focus = sh.focus == int(at);
        panel::roundRect(out, L.lines[i], 9 * s, r.chosen > 0 ? kGold : kWhite,
                         r.chosen > 0 ? (hot ? 0.16f : 0.10f) : (hot ? 0.07f : 0.035f), focus ? kGold : kWhite,
                         focus ? 0.9f : 0.08f, (focus ? 1.5f : 1.0f) * s);
        const Cells c = cells(L.lines[i], L.minus[i], s);
        panel::roundRect(out, c.icon, 6 * s, kBlack, 0.30f);
        if (r.icon && r.icon->width > 0) put(out, *r.icon, c.icon);
        const bool canPlus = canAdd(r, sum, sh.zeny), canMinus = canRemove(r);
        const bool hotPlus = hot && sh.hoverPart == int(Part::Plus), hotMinus = hot && sh.hoverPart == int(Part::Minus);
        panel::roundRect(out, L.plus[i], 7 * s, kGold, !canPlus ? 0.04f : hotPlus ? 0.40f : 0.24f, kGold,
                         canPlus ? 0.7f : 0.2f, 1.0f * s);
        panel::roundRect(out, L.minus[i], 7 * s, kWhite, !canMinus ? 0.02f : hotMinus ? 0.16f : 0.07f, kWhite,
                         canMinus ? 0.25f : 0.08f, 1.0f * s);
    }
    panel::roundRect(out, L.sums, 9 * s, kBlack, 0.25f, kWhite, 0.08f, 1.0f * s);
    const bool can = sum > 0 && sum <= sh.zeny;
    panel::roundRect(out, L.buy, 9 * s, kGold, !can ? 0.05f : sh.hoverPart == int(Part::Buy) ? 0.36f : 0.24f, kGold,
                     can ? 0.85f : 0.25f, 1.0f * s);
    panel::roundRect(out, L.cancel, 9 * s, kWhite, sh.hoverPart == int(Part::Cancel) ? 0.12f : 0.06f, kWhite, 0.16f,
                     1.0f * s);
}

std::vector<Words> words(const Layout& L, const std::vector<Row>& rows, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 16 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", kText, 15 * s, false, Align::Center});  // ×
    for (size_t i = 0; i < L.lines.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= rows.size()) break;
        const Row& r = rows[at];
        const Cells c = cells(L.lines[i], L.minus[i], s);
        const std::string name = r.refine > 0 ? "+" + std::to_string(r.refine) + " " + r.name : r.name;
        out.push_back(Words{c.name, name, kText, 13 * s, true, Align::Left});
        std::string stock = t.stock;
        if (const size_t p = stock.find("{1}"); p != std::string::npos) stock.replace(p, 3, std::to_string(r.stock));
        out.push_back(Words{c.stock, stock, kMuted, 11.5f * s, false, Align::Left});
        out.push_back(Words{c.price, money(r.price) + " z", kGold2, 13 * s, true, Align::Right});
        out.push_back(Words{L.counts[i], std::to_string(r.chosen), r.chosen > 0 ? kText : kMuted, 13 * s, r.chosen > 0,
                            Align::Center});
        out.push_back(Words{L.minus[i], "\xE2\x88\x92", kText, 15 * s, true, Align::Center});  // −
        out.push_back(Words{L.plus[i], "+", kText, 15 * s, true, Align::Center});
    }
    const long long sum = total(rows);
    out.push_back(Words{L.total, t.total, kMuted, 12.5f * s, false, Align::Left});
    out.push_back(Words{L.total, money(sum) + " z", sum > sh.zeny ? kShort : kGold2, 14 * s, true, Align::Right});
    out.push_back(Words{L.zeny, t.zeny, kMuted, 12 * s, false, Align::Left});
    out.push_back(Words{L.zeny, money(sh.zeny) + " z", kText, 12.5f * s, false, Align::Right});
    if (!t.hint.empty()) out.push_back(Words{L.hint, t.hint, kMuted, 11 * s, false, Align::Center});
    out.push_back(Words{L.buy, t.buy, kText, 14 * s, true, Align::Center});
    out.push_back(Words{L.cancel, t.cancel, kText, 14 * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::vendshop
