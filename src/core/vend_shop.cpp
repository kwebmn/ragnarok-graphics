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

panel::Box iconAt(const Layout& L, size_t line) {
    return line < L.lines.size() && line < L.minus.size() ? cells(L.lines[line], L.minus[line], L.s).icon
                                                          : panel::Box{};
}

std::string withSlots(const std::string& name, int slots) {
    return slots > 0 ? name + " [" + std::to_string(slots) + "]" : name;
}

std::string inside(const int cards[4], int slots, const std::function<std::string(int)>& cardName,
                   const std::string& freeWord) {
    const bool made = cards[0] == 255 || cards[0] == 254 || cards[0] == 0xFF00 || cards[0] == -256;
    std::string out;
    int used = 0;
    if (!made)
        for (int i = 0; i < 4; ++i) {
            if (cards[i] <= 0) continue;
            ++used;
            std::string n = cardName(cards[i]);
            if (n.empty()) n = "#" + std::to_string(cards[i]);
            out += (out.empty() ? "" : " \xC2\xB7 ") + n;
        }
    const int free = made ? 0 : slots - used;
    if (free > 0) {
        std::string w = freeWord;
        if (const size_t p = w.find("{1}"); p != std::string::npos) w.replace(p, 3, std::to_string(free));
        out += (out.empty() ? "" : " \xC2\xB7 ") + w;
    }
    return out;
}

}  // namespace uagfx::vendshop
