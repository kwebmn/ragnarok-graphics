#include "core/my_shop.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"
#include "core/vend_shop.hpp"

namespace uagfx::myshop {
namespace {

// At scale 1: the card 420 wide, 16 in; the title 30; a row 40 (8 shown at most); the sum 26; the button 38
constexpr float kW = 420, kPad = 16, kTitleH = 30, kRowH = 40, kSumH = 26, kButtonH = 38, kIcon = 28;
constexpr size_t kMostRows = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000;

void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
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

panel::Box iconBox(const panel::Box& row, float s) {
    return panel::Box{row.x + 6 * s, row.y + (row.h - kIcon * s) / 2, kIcon * s, kIcon * s};
}

}  // namespace

long long worth(const std::vector<Row>& rows) {
    long long t = 0;
    for (const Row& r : rows) t += r.price * r.left;
    return t;
}

Layout layout(int targetW, int targetH, float scale, size_t count) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, inner = w - 2 * pad;
    L.title = panel::Box{pad, pad, inner - 28 * s, kTitleH * s};
    L.cross = panel::Box{w - pad - 24 * s, pad + 3 * s, 24 * s, 24 * s};
    float y = pad + kTitleH * s + 6 * s;
    const size_t shown = std::min(count, kMostRows);
    L.list = panel::Box{pad, y, inner, kRowH * s * float(std::max<size_t>(shown, 2))};
    for (size_t i = 0; i < shown; ++i)
        L.rows.push_back(panel::Box{pad, y + float(i) * kRowH * s, inner, kRowH * s - 4 * s});
    y += L.list.h + 6 * s;
    L.sum = panel::Box{pad + 10 * s, y, inner - 20 * s, kSumH * s};
    y += kSumH * s + 8 * s;
    L.close = panel::Box{pad, y, inner, kButtonH * s};
    y += kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::clamp(int(float(targetW) * 0.64f), 0, std::max(0, targetW - L.w));
    L.y = std::clamp(int(float(targetH) * 0.20f), 0, std::max(0, targetH - L.h));
    return L;
}

int mostScroll(const Layout& L, size_t count) { return std::max(0, int(count) - int(L.rows.size())); }

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y) || L.cross.contains(x, y)) return Hit{Part::Close};
    if (L.glass.contains(x, y)) return Hit{Part::Body};
    return Hit{};
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Row>& rows, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.cross, 6 * s, kWhite, sh.hoverClose ? 0.14f : 0.06f);
    if (L.rows.empty()) panel::roundRect(out, L.list, 8 * s, kBlack, 0.16f);
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= rows.size()) break;
        panel::roundRect(out, L.rows[i], 8 * s, kWhite, 0.035f);
        const panel::Box icon = iconBox(L.rows[i], s);
        panel::roundRect(out, icon, 6 * s, kBlack, 0.30f);
        if (rows[at].icon && rows[at].icon->width > 0) put(out, *rows[at].icon, icon);
    }
    panel::roundRect(out, L.close, 8 * s, kWhite, sh.hoverClose ? 0.14f : 0.07f, kGold, 0.6f, 1.0f * s);
}

std::vector<Words> words(const Layout& L, const std::vector<Row>& rows, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.cross, "\xC3\x97", kText, 14 * s, false, Align::Center});  // ×
    if (L.rows.empty()) out.push_back(Words{L.list, t.empty, kMuted, 13 * s, false, Align::Center});
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= rows.size()) break;
        const Row& r = rows[at];
        const panel::Box icon = iconBox(L.rows[i], s);
        const float nx = icon.x + icon.w + 10 * s, right = L.rows[i].x + L.rows[i].w - 8 * s;
        const panel::Box name{nx, L.rows[i].y + 3 * s, right - nx - 120 * s, L.rows[i].h / 2};
        const panel::Box left{nx, L.rows[i].y + L.rows[i].h / 2, right - nx - 120 * s, L.rows[i].h / 2 - 3 * s};
        const panel::Box price{right - 116 * s, L.rows[i].y, 116 * s, L.rows[i].h};
        out.push_back(Words{name, r.refine > 0 ? "+" + std::to_string(r.refine) + " " + r.name : r.name, kText,
                            13 * s, true, Align::Left});
        std::string l = t.left;
        if (const size_t p = l.find("{1}"); p != std::string::npos) l.replace(p, 3, std::to_string(r.left));
        out.push_back(Words{left, l, kMuted, 11.5f * s, false, Align::Left});
        out.push_back(Words{price, vendshop::money(r.price) + " z", kGold2, 13 * s, true, Align::Right});
    }
    out.push_back(Words{L.sum, t.worth, kMuted, 12 * s, false, Align::Left});
    out.push_back(Words{L.sum, vendshop::money(worth(rows)) + " z", kGold2, 13 * s, true, Align::Right});
    out.push_back(Words{L.close, t.close, kText, 13.5f * s, true, Align::Center});
    return out;
}

}  // namespace uagfx::myshop
