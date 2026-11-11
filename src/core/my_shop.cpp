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

panel::Box iconOf(const panel::Box& row, float s) { return iconBox(row, s); }

void Ledger::stock(int index, const std::string& name, long long price) {
    Thing& t = things_[index];
    t.name = name;
    t.price = price;
}

bool Ledger::sold(int index, int amount, const std::string& time, Sale& out) {
    const auto it = things_.find(index);
    if (it == things_.end() || amount <= 0) return false;
    it->second.sold += amount;
    out = Sale{time, it->second.name, amount, it->second.price * amount};
    earned_ += out.zeny;
    sales_.insert(sales_.begin(), out);
    if (sales_.size() > 50) sales_.pop_back();
    return true;
}

int Ledger::soldOf(int index) const {
    const auto it = things_.find(index);
    return it == things_.end() ? 0 : it->second.sold;
}

void Ledger::clear() {
    things_.clear();
    sales_.clear();
    earned_ = 0;
}

}  // namespace uagfx::myshop
