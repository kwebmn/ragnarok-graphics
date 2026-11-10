#include "core/vend_setup.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/ui_style.hpp"
#include "core/vend_shop.hpp"

namespace uagfx::vendsetup {
namespace {

// At scale 1: the card 640 wide, 16 in; the title 30; the sign's field 30; a column's heading 22; a row 38 (7 shown);
// the buttons 38
constexpr float kW = 640, kPad = 16, kTitleH = 30, kFieldH = 30, kHeadH = 22, kRowH = 38, kButtonH = 38, kIcon = 26;
constexpr float kGap = 14, kPriceW = 118, kRemoveW = 22;
constexpr size_t kRows = 7;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000, kDanger = 0xE0675C;
constexpr long long kMostPrice = 99990000;  // the game's OK asks past it (msgstringtable 0x398)

panel::Box iconBox(const panel::Box& row, float s) {
    return panel::Box{row.x + 5 * s, row.y + (row.h - kIcon * s) / 2, kIcon * s, kIcon * s};
}

std::string named(const Row& r) {
    std::string n = r.refine > 0 ? "+" + std::to_string(r.refine) + " " + r.name : r.name;
    if (r.count > 1) n += " \xC3\x97" + std::to_string(r.count);  // ×
    return n;
}

}  // namespace

long long priceOf(const std::string& words) {
    long long v = 0;
    bool any = false;
    for (const char c : words) {
        if (!std::isdigit(static_cast<unsigned char>(c))) continue;
        any = true;
        v = std::min<long long>(v * 10 + (c - '0'), 10LL * kMostPrice);
    }
    return any ? v : 0;
}

std::string stepPrice(const std::string& words, int dir) {
    const long long v = std::min<long long>(priceOf(words), kMostPrice);
    if (dir == 0) return std::to_string(v);
    if (dir > 0 && v == 0) return "10";
    const long long from = dir < 0 ? v - 1 : v;  // down from 10 000 steps by 100, not 1 000
    long long step = 1;
    while (step * 100 <= from) step *= 10;
    long long next = dir > 0 ? (v / step + 1) * step : ((v + step - 1) / step - 1) * step;
    next = std::clamp<long long>(next, 0, kMostPrice);
    return std::to_string(next);
}

Layout layout(int targetW, int targetH, float scale, bool hint) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, inner = w - 2 * pad, col = (inner - kGap * s) / 2;
    L.title = panel::Box{pad, pad, inner - 28 * s, kTitleH * s};
    L.cross = panel::Box{w - pad - 24 * s, pad + 3 * s, 24 * s, 24 * s};
    float y = pad + kTitleH * s + 6 * s;
    L.signLabel = panel::Box{pad, y, 110 * s, kFieldH * s};
    L.sign = panel::Box{pad + 112 * s, y, inner - 112 * s, kFieldH * s};
    y += kFieldH * s + 12 * s;
    const float right = pad + col + kGap * s;
    L.cartHead = panel::Box{pad, y, col, kHeadH * s};
    L.saleHead = panel::Box{right, y, col, kHeadH * s};
    y += kHeadH * s + 4 * s;
    L.cartList = panel::Box{pad, y, col, kRowH * s * float(kRows)};
    L.saleList = panel::Box{right, y, col, kRowH * s * float(kRows)};
    for (size_t i = 0; i < kRows; ++i) {
        const float ry = y + float(i) * kRowH * s;
        L.cartRows.push_back(panel::Box{pad, ry, col, kRowH * s - 4 * s});
        const panel::Box sale{right, ry, col, kRowH * s - 4 * s};
        L.saleRows.push_back(sale);
        L.removes.push_back(panel::Box{sale.x + sale.w - kRemoveW * s - 4 * s, sale.y + (sale.h - kRemoveW * s) / 2,
                                       kRemoveW * s, kRemoveW * s});
        L.prices.push_back(panel::Box{L.removes.back().x - kPriceW * s - 6 * s, sale.y + 5 * s, kPriceW * s,
                                      sale.h - 10 * s});
    }
    y += L.cartList.h + 12 * s;
    L.cancel = panel::Box{pad, y, col, kButtonH * s};
    L.open = panel::Box{right, y, col, kButtonH * s};
    y += kButtonH * s;
    if (hint) {  // 3.0.16-beta.39
        L.hint = panel::Box{pad, y + 6 * s, inner, 18 * s};
        y += 24 * s;
    }
    y += pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::clamp(int(float(targetW) * 0.5f) - L.w / 2, 0, std::max(0, targetW - L.w));
    L.y = std::clamp(int(float(targetH) * 0.18f), 0, std::max(0, targetH - L.h));
    return L;
}

int mostScroll(const Layout& L, bool sale, size_t count) {
    return std::max(0, int(count) - int(sale ? L.saleRows.size() : L.cartRows.size()));
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.cross.contains(x, y)) return Hit{Part::Cross};
    if (L.open.contains(x, y)) return Hit{Part::Open};
    if (L.cancel.contains(x, y)) return Hit{Part::Cancel};
    if (L.sign.contains(x, y)) return Hit{Part::Sign};
    for (size_t i = 0; i < L.cartRows.size(); ++i)
        if (L.cartRows[i].contains(x, y)) return Hit{Part::Cart, int(i)};
    for (size_t i = 0; i < L.saleRows.size(); ++i) {
        if (L.removes[i].contains(x, y)) return Hit{Part::Remove, int(i)};
        if (L.prices[i].contains(x, y)) return Hit{Part::Price, int(i)};
        if (L.saleRows[i].contains(x, y)) return Hit{Part::Sale, int(i)};
    }
    if (L.glass.contains(x, y)) return Hit{Part::Body};
    return Hit{};
}

std::vector<Words> words(const Layout& L, const std::vector<Row>& cart, const std::vector<Row>& sale,
                         const std::string& sign, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    const std::string caret = sh.caret ? "|" : "";
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.cross, "\xC3\x97", kText, 14 * s, false, Align::Center});  // ×
    out.push_back(Words{L.signLabel, t.sign, kMuted, 12.5f * s, false, Align::Left});
    const panel::Box signIn{L.sign.x + 8 * s, L.sign.y, L.sign.w - 16 * s, L.sign.h};
    if (sign.empty() && sh.focus != 0)
        out.push_back(Words{signIn, t.signHint, kMuted, 12.5f * s, false, Align::Left});
    else
        out.push_back(Words{signIn, sign + (sh.focus == 0 ? caret : ""), kText, 13 * s, false, Align::Left});
    out.push_back(Words{L.cartHead, t.cart, kGold2, 12.5f * s, true, Align::Left});
    std::string head = t.sale;
    for (const auto& [key, value] : {std::pair<const char*, int>{"{1}", int(sale.size())}, {"{2}", sh.most}})
        if (const size_t p = head.find(key); p != std::string::npos) head.replace(p, 3, std::to_string(value));
    out.push_back(Words{L.saleHead, head, kGold2, 12.5f * s, true, Align::Left});
    if (cart.empty()) out.push_back(Words{L.cartList, t.empty, kMuted, 12.5f * s, false, Align::Center});
    for (size_t i = 0; i < L.cartRows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.cartScroll));
        if (at >= cart.size()) break;
        const panel::Box icon = iconBox(L.cartRows[i], s);
        const float nx = icon.x + icon.w + 8 * s;
        out.push_back(Words{panel::Box{nx, L.cartRows[i].y, L.cartRows[i].x + L.cartRows[i].w - nx - 6 * s,
                                       L.cartRows[i].h},
                            named(cart[at]), kText, 12.5f * s, false, Align::Left});
    }
    if (sale.empty()) out.push_back(Words{L.saleList, t.priceHint, kMuted, 12 * s, false, Align::Center});
    for (size_t i = 0; i < L.saleRows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.saleScroll));
        if (at >= sale.size()) break;
        const panel::Box icon = iconBox(L.saleRows[i], s);
        const float nx = icon.x + icon.w + 8 * s;
        out.push_back(Words{panel::Box{nx, L.saleRows[i].y, L.prices[i].x - nx - 6 * s, L.saleRows[i].h},
                            named(sale[at]), kText, 12 * s, false, Align::Left});
        const bool focused = sh.focus == 1 + int(at);
        const long long price = priceOf(sale[at].price);
        const std::string shown = focused ? sale[at].price + caret : price > 0 ? vendshop::money(price) : "0";
        out.push_back(Words{panel::Box{L.prices[i].x + 4 * s, L.prices[i].y, L.prices[i].w - 22 * s, L.prices[i].h},
                            shown, price > 0 || focused ? kGold2 : kDanger, 12.5f * s, true, Align::Right});
        out.push_back(Words{panel::Box{L.prices[i].x + L.prices[i].w - 18 * s, L.prices[i].y, 16 * s, L.prices[i].h},
                            "z", kMuted, 11 * s, false, Align::Center});
        out.push_back(Words{L.removes[i], "\xC3\x97", kText, 13 * s, false, Align::Center});
    }
    out.push_back(Words{L.cancel, t.cancel, kText, 13.5f * s, true, Align::Center});
    out.push_back(Words{L.open, t.open, kText, 13.5f * s, true, Align::Center});
    if (L.hint.h > 0 && !t.hint.empty()) out.push_back(Words{L.hint, t.hint, kMuted, 11.5f * s, false, Align::Center});
    return out;
}

panel::Box iconOf(const panel::Box& row, float s) { return iconBox(row, s); }

}  // namespace uagfx::vendsetup
