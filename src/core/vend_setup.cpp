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

}  // namespace uagfx::vendsetup
