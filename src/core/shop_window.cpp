#include "core/shop_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/char_block.hpp"
#include "core/pad_glyphs.hpp"
#include "core/reference_index.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::shopwin {

using charblock::grouped;

namespace {

// The concept's sizes at scale 1 (its 1600 px mockup's at 1080): 1000 wide, the title 46, the tabs 32, the search 34,
// the list 520 wide, its rows 35, the cart's 35, the totals' lines 24, the buttons 36, the footer 40
constexpr float kW = 1000, kTitleH = 46, kTabsH = 32, kSide = 16, kSearchH = 34, kHeadH = 22, kRowH = 35;
constexpr float kListW = 520, kGapW = 24, kIcon = 30, kLineH = 24, kButtonH = 36, kFooterH = 40, kGlyph = 17;
constexpr float kPriceW = 110, kWeightW = 64, kAmountW = 96, kStepBox = 22;
constexpr int kNear = 15;  // cells: the NPC a character trades with stands this near
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653, kGold2 = 0xC9A55B;
constexpr uint32_t kTile = 0x1B1D24, kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kInk = 0x1A1408;
constexpr uint32_t kAmber = 0xF5BD4F, kRed = 0xFF8A80;

std::string lowered(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box, float k) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (k < 1) {
                const auto ch = [&](int sh) { return uint32_t(float((c >> sh) & 0xFF) * k + 0.5f); };
                c = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
            }
            if (!(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

std::string nameOf(const Ware& w) { return w.refine > 0 ? "+" + std::to_string(w.refine) + " " + w.name : w.name; }

struct PadItem {
    padglyph::Button button;
    const std::string* words;
};

std::vector<PadItem> padItems(const Data& d, const Texts& t) {
    return {{padglyph::Button::A, &t.plus},
            {padglyph::Button::X, &t.minus},
            {padglyph::Button::Y, d.tab == Tab::Buy ? &t.buy : &t.sell},
            {padglyph::Button::B, &t.close}};
}

const Ware* wareAt(const Data& d, int at) {
    return at >= 0 && at < int(d.wares.size()) ? &d.wares[size_t(at)] : nullptr;
}

}  // namespace

int most(const Ware& w, Tab tab) { return tab == Tab::Sell ? std::max(0, w.count) : kMostBought; }

void put(std::vector<Line>& cart, const std::vector<Ware>& wares, int ware, int by, Tab tab) {
    if (ware < 0 || ware >= int(wares.size()) || by == 0) return;
    const int top = most(wares[size_t(ware)], tab);
    const auto it = std::find_if(cart.begin(), cart.end(), [&](const Line& l) { return l.ware == ware; });
    const long long now = it == cart.end() ? 0 : it->amount;
    const int want = int(std::clamp<long long>(now + by, 0, top));
    if (it == cart.end()) {
        if (want > 0) cart.push_back(Line{ware, want});
    } else if (want > 0) {
        it->amount = want;
    } else {
        cart.erase(it);
    }
}

int amountOf(const std::vector<Line>& cart, int ware) {
    for (const Line& l : cart)
        if (l.ware == ware) return l.amount;
    return 0;
}

std::vector<int> shownOf(const std::vector<Ware>& wares, const std::string& search) {
    std::vector<int> out;
    const std::string want = lowered(search);
    for (size_t i = 0; i < wares.size(); ++i)
        if (want.empty() || lowered(wares[i].name).find(want) != std::string::npos) out.push_back(int(i));
    return out;
}

std::string weightText(int tenths) {
    if (tenths % 10 == 0) return std::to_string(tenths / 10);
    return std::to_string(tenths / 10) + "." + std::to_string(std::abs(tenths % 10));
}

std::string money(long long zeny) { return (zeny < 0 ? "\xE2\x88\x92" + grouped(-zeny) : grouped(zeny)) + " z"; }

int scrollFor(int focus, int scroll, int rows, int count) {
    if (focus < scroll) scroll = focus;
    if (focus >= scroll + rows) scroll = focus - rows + 1;
    return std::clamp(scroll, 0, std::max(0, count - rows));
}

Sums sums(Tab tab, const std::vector<Line>& cart, const std::vector<Ware>& wares, long long zeny, int weight,
          int weightMost) {
    Sums s;
    long long tenths = 0;
    for (const Line& l : cart) {
        if (l.ware < 0 || l.ware >= int(wares.size())) continue;
        const Ware& w = wares[size_t(l.ware)];
        s.total += (long long)w.price * l.amount;
        if (w.weight > 0) tenths += (long long)w.weight * l.amount;
    }
    const bool buy = tab == Tab::Buy;
    s.zenyAfter = buy ? zeny - s.total : zeny + s.total;
    const long long after = (long long)weight * 10 + (buy ? tenths : -tenths);
    s.weightAfter = std::max(0LL, after / 10);
    s.weightPercent = weightMost > 0 ? int(s.weightAfter * 100 / weightMost) : 0;
    s.ok = !cart.empty() && (!buy || (s.zenyAfter >= 0 && (weightMost <= 0 || after <= (long long)weightMost * 10)));
    return s;
}

int matchShop(const std::vector<refindex::Shop>& shops, const std::string& map, const std::vector<int>& items, int x,
              int y) {
    int best = -1, bestScore = 0, bestDistance = 0;
    for (size_t i = 0; i < shops.size(); ++i) {
        const refindex::Shop& s = shops[i];
        if (s.map != map) continue;
        int score = 0;
        for (const int id : items)
            for (const auto& it : s.items)
                if (it.first == id) {
                    ++score;
                    break;
                }
        const int distance = std::max(std::abs(s.x - x), std::abs(s.y - y));
        const bool better = score > bestScore || (score == bestScore && (best < 0 || distance < bestDistance));
        if (better && (score > 0 || distance <= kNear)) {
            best = int(i);
            bestScore = score;
            bestDistance = distance;
        }
    }
    return best;
}

int iconSide(float scale) { return int(std::lround(26 * scale)); }

Layout layout(int x, int y, float scale, const Data& d, const std::vector<int>& shown, const Show& sh,
              const Texts& t, const Measure& m) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, side = kSide * s;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    L.tabsRow = panel::Box{side, kTitleH * s, w - 2 * side, kTabsH * s};
    float tx = side + (sh.pad ? padglyph::widthOf(padglyph::Button::LB, kGlyph * s) + 8 * s : 0);
    if (sh.pad)
        L.tabGlyphs[0] = panel::Box{side, L.tabsRow.y + (L.tabsRow.h - kGlyph * s) / 2 - 1 * s,
                                    padglyph::widthOf(padglyph::Button::LB, kGlyph * s), kGlyph * s};
    for (const std::string* name : {&t.buy, &t.sell}) {
        const float tw = m(*name, 13 * s, true) + 4 * s;
        L.tabs.push_back(panel::Box{tx, L.tabsRow.y, tw, L.tabsRow.h});
        tx += tw + 18 * s;
    }
    if (sh.pad)
        L.tabGlyphs[1] = panel::Box{tx - 10 * s, L.tabGlyphs[0].y, padglyph::widthOf(padglyph::Button::RB, kGlyph * s),
                                    kGlyph * s};
    const float top = L.tabsRow.y + L.tabsRow.h + 12 * s;
    // the list on the left
    L.searchBox = panel::Box{side, top, kListW * s, kSearchH * s};
    L.listHead = panel::Box{side, top + (kSearchH + 8) * s, kListW * s, kHeadH * s};
    L.list = panel::Box{side, L.listHead.y + L.listHead.h, kListW * s, kListRows * kRowH * s};
    L.firstWare = sh.scroll;
    L.firstLine = sh.cartScroll;
    for (int i = sh.scroll; i < int(shown.size()) && i - sh.scroll < kListRows; ++i)
        L.wares.push_back(panel::Box{L.list.x, L.list.y + float(i - sh.scroll) * kRowH * s, L.list.w, kRowH * s});
    // the cart on the right
    const float cx = side + (kListW + kGapW) * s, cw = w - side - cx;
    L.cartTitle = panel::Box{cx, top, cw, kSearchH * s};
    L.cartHead = panel::Box{cx, L.listHead.y, cw, kHeadH * s};
    L.cart = panel::Box{cx, L.cartHead.y + L.cartHead.h, cw, kCartRows * kRowH * s};
    for (int i = sh.cartScroll; i < int(d.cart.size()) && i - sh.cartScroll < kCartRows; ++i) {
        const panel::Box row{cx, L.cart.y + float(i - sh.cartScroll) * kRowH * s, cw, kRowH * s};
        L.lines.push_back(row);
        const float ax = row.x + row.w - 100 * s - kAmountW * s;  // the amount's column: − n +
        const float by = row.y + (row.h - kStepBox * s) / 2;
        L.less.push_back(panel::Box{ax, by, kStepBox * s, kStepBox * s});
        L.more.push_back(panel::Box{ax + kAmountW * s - kStepBox * s, by, kStepBox * s, kStepBox * s});
    }
    L.totals = panel::Box{cx, L.cart.y + L.cart.h + 10 * s, cw, 3 * kLineH * s};
    const std::string& button = d.tab == Tab::Buy ? t.buyButton : t.sellButton;
    const float bw = m(button, 13 * s, true) + 36 * s, clw = m(t.clear, 13 * s, true) + 28 * s;
    L.confirm = panel::Box{cx, L.totals.y + L.totals.h + 10 * s, bw, kButtonH * s};
    L.clear = panel::Box{L.confirm.x + L.confirm.w + 10 * s, L.confirm.y, clw, kButtonH * s};
    L.note = panel::Box{cx, L.confirm.y + L.confirm.h + 6 * s, cw, 18 * s};
    const float bottom = std::max(L.list.y + L.list.h, L.note.y + L.note.h);
    L.footer = panel::Box{0, bottom + 10 * s, w, kFooterH * s};
    if (sh.pad) {  // the controller's buttons in the footer, from its right
        const std::vector<PadItem> items = padItems(d, t);
        float right = w - side;
        L.padGlyphs.resize(items.size());
        L.padWords.resize(items.size());
        for (size_t i = items.size(); i-- > 0;) {
            const float ww = m(*items[i].words, 12 * s, false) + 2;
            const float gw = padglyph::widthOf(items[i].button, kGlyph * s);
            L.padWords[i] = panel::Box{right - ww, L.footer.y, ww, L.footer.h};
            L.padGlyphs[i] = panel::Box{L.padWords[i].x - 6 * s - gw, L.footer.y + (L.footer.h - kGlyph * s) / 2, gw,
                                        kGlyph * s};
            right = L.padGlyphs[i].x - 16 * s;
        }
    }
    L.glass = panel::Box{0, 0, w, L.footer.y + L.footer.h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (size_t i = 0; i < L.tabs.size(); ++i)
        if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, int(i)};
    if (L.searchBox.contains(x, y)) return Hit{Part::Search, -1};
    if (L.confirm.contains(x, y)) return Hit{Part::Confirm, -1};
    if (L.clear.contains(x, y)) return Hit{Part::Clear, -1};
    for (size_t i = 0; i < L.lines.size(); ++i) {
        const int at = L.firstLine + int(i);
        if (L.less[i].contains(x, y)) return Hit{Part::Less, at};
        if (L.more[i].contains(x, y)) return Hit{Part::More, at};
        if (L.lines[i].contains(x, y)) return Hit{Part::Line, at};
    }
    for (size_t i = 0; i < L.wares.size(); ++i)
        if (L.wares[i].contains(x, y)) return Hit{Part::Ware, L.firstWare + int(i)};
    if (L.list.contains(x, y)) return Hit{Part::List, -1};
    if (L.cart.contains(x, y)) return Hit{Part::Cart, -1};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

void paint(panel::Picture& out, const Layout& L, const Data& d, const std::vector<int>& shown, const Show& sh,
           const Measure& m) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.09f, 1.0f * s);
    if (sh.hoverClose) panel::roundRect(out, L.close, 7 * s, kWhite, 0.12f);
    panel::roundRect(out, panel::Box{L.tabsRow.x, L.tabsRow.y + L.tabsRow.h - 1 * s, L.tabsRow.w, 1 * s}, 0, kWhite,
                     0.08f);
    const int on = d.tab == Tab::Buy ? 0 : 1;
    if (on < int(L.tabs.size())) {
        const panel::Box& b = L.tabs[size_t(on)];
        panel::roundRect(out, panel::Box{b.x, b.y + b.h - 2.5f * s, b.w, 2 * s}, 1 * s, kGold, 0.95f);
    }
    // the search
    panel::roundRect(out, L.searchBox, 8 * s, kBlack, 0.35f, sh.typing ? kGold2 : kWhite,
                     sh.typing ? 0.6f : sh.hoverSearch ? 0.18f : 0.10f, 1.0f * s);
    const float icon = 15 * s;
    uiicons::draw(out, uiicons::Icon::Search, L.searchBox.x + 10 * s, L.searchBox.y + (L.searchBox.h - icon) / 2,
                  icon, kMuted, 0.9f, 2.2f);
    if (sh.typing && sh.caretOn) {
        const float x = L.searchBox.x + 32 * s + m(sh.search, 12.5f * s, false) + 1 * s;
        panel::roundRect(out, panel::Box{x, L.searchBox.y + 9 * s, 1.5f * s, L.searchBox.h - 18 * s}, 0, kText,
                         0.95f);
    }
    // the list's rows
    panel::roundRect(out, panel::Box{L.listHead.x, L.listHead.y + L.listHead.h - 1 * s, L.listHead.w, 1 * s}, 0,
                     kWhite, 0.08f);
    for (size_t i = 0; i < L.wares.size(); ++i) {
        const panel::Box& b = L.wares[i];
        const int at = L.firstWare + int(i);
        if (at == sh.hoverWare) panel::roundRect(out, b, 6 * s, kWhite, 0.06f);
        if (sh.pad && !sh.padCart && at == sh.padWare)
            panel::roundRect(out, b, 6 * s, kGold2, 0.08f, kGold, 0.9f, 1.5f * s);
        if (at < int(shown.size()) && amountOf(d.cart, shown[size_t(at)]) > 0)  // in the cart: a gold mark at its start
            panel::roundRect(out, panel::Box{b.x + 1 * s, b.y + 8 * s, 2.5f * s, b.h - 16 * s}, 1 * s, kGold, 0.9f);
        panel::roundRect(out, panel::Box{b.x, b.y + b.h - 1 * s, b.w, 1 * s}, 0, kWhite, 0.05f);
        const Ware* w = at < int(shown.size()) ? wareAt(d, shown[size_t(at)]) : nullptr;
        if (w && w->icon && w->icon->width > 0)
            put(out, *w->icon, panel::Box{b.x + 6 * s, b.y + (b.h - kIcon * s) / 2, kIcon * s, kIcon * s},
                w->identified ? 1.0f : 0.45f);
    }
    // the cart's rows, their − and +
    panel::roundRect(out, panel::Box{L.cartHead.x, L.cartHead.y + L.cartHead.h - 1 * s, L.cartHead.w, 1 * s}, 0,
                     kWhite, 0.08f);
    for (size_t i = 0; i < L.lines.size(); ++i) {
        const panel::Box& b = L.lines[i];
        const int at = L.firstLine + int(i);
        if (at == sh.hoverLine) panel::roundRect(out, b, 6 * s, kWhite, 0.06f);
        if (sh.pad && sh.padCart && at == sh.padLine)
            panel::roundRect(out, b, 6 * s, kGold2, 0.08f, kGold, 0.9f, 1.5f * s);
        panel::roundRect(out, panel::Box{b.x, b.y + b.h - 1 * s, b.w, 1 * s}, 0, kWhite, 0.05f);
        const Line& line = d.cart[size_t(at)];
        const Ware* w = wareAt(d, line.ware);
        if (w && w->icon && w->icon->width > 0)
            put(out, *w->icon, panel::Box{b.x + 4 * s, b.y + (b.h - kIcon * s) / 2, kIcon * s, kIcon * s}, 1.0f);
        for (const auto& [box, hot, glyph] : {std::tuple{L.less[i], at == sh.hoverLess, uiicons::Icon::Minus},
                                              std::tuple{L.more[i], at == sh.hoverMore, uiicons::Icon::Plus}}) {
            panel::roundRect(out, box, 5 * s, kWhite, hot ? 0.16f : 0.07f);
            const float g = 12 * s;
            uiicons::draw(out, glyph, box.x + (box.w - g) / 2, box.y + (box.h - g) / 2, g, kText, 0.9f, 2.4f);
        }
    }
    // the buttons
    const Sums sum = sums(d.tab, d.cart, d.wares, d.zeny, d.weight, d.weightMost);
    if (sum.ok)
        panel::roundRect(out, L.confirm, 8 * s, sh.hoverConfirm ? 0xF8D57A : kGold, 1.0f);
    else
        panel::roundRect(out, L.confirm, 8 * s, kWhite, 0.07f, kWhite, 0.10f, 1.0f * s);
    panel::roundRect(out, L.clear, 8 * s, kWhite, sh.hoverClear ? 0.14f : 0.07f);
    panel::roundRect(out, panel::Box{0, L.footer.y, L.glass.w, 1 * s}, 0, kWhite, 0.08f);
    if (sh.pad) {
        const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
        Texts none;
        const std::vector<PadItem> items = padItems(d, none);
        for (size_t i = 0; i < items.size() && i < L.padGlyphs.size(); ++i)
            padglyph::paint(out, items[i].button, f, L.padGlyphs[i].x, L.padGlyphs[i].y, kGlyph * s);
        padglyph::paint(out, padglyph::Button::LB, f, L.tabGlyphs[0].x, L.tabGlyphs[0].y, kGlyph * s);
        padglyph::paint(out, padglyph::Button::RB, f, L.tabGlyphs[1].x, L.tabGlyphs[1].y, kGlyph * s);
    }
}

std::vector<Words> words(const Layout& L, const Data& d, const std::vector<int>& shown, const Show& sh,
                         const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s, side = kSide * s;
    const std::string& title = d.name.empty() ? t.title : d.name;
    const float titleW = m(title, 15 * s, true) + 2;
    out.push_back(Words{panel::Box{side, 0, titleW, L.title.h}, title, kText, 15 * s, true, Align::Left});
    if (!d.place.empty())
        out.push_back(Words{panel::Box{side + titleW + 8 * s, 0, m(d.place, 11.5f * s, false) + 2, L.title.h}, d.place,
                            kFaint, 11.5f * s, false, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", sh.hoverClose ? kText : kMuted, 16 * s, false, Align::Center});
    const int on = d.tab == Tab::Buy ? 0 : 1;
    for (size_t i = 0; i < L.tabs.size(); ++i)
        out.push_back(Words{L.tabs[i], i == 0 ? t.buy : t.sell,
                            int(i) == on ? kText : int(i) == sh.hoverTab ? 0xC9D1DE : kFaint, 13 * s, true,
                            Align::Center});
    // the search, the list's head and rows
    const panel::Box field{L.searchBox.x + 32 * s, L.searchBox.y, L.searchBox.w - 40 * s, L.searchBox.h};
    if (sh.search.empty() && !sh.typing)
        out.push_back(Words{field, t.search, kFaint, 12 * s, false, Align::Left});
    else if (!sh.search.empty())
        out.push_back(Words{field, sh.search, kText, 12.5f * s, false, Align::Left});
    const float priceR = L.list.x + L.list.w - (kWeightW + 12) * s, weightR = L.list.x + L.list.w - 10 * s;
    out.push_back(Words{panel::Box{L.listHead.x + 6 * s, L.listHead.y, 200 * s, L.listHead.h}, t.ware, kFaint,
                        11 * s, false, Align::Left});
    out.push_back(Words{panel::Box{priceR - kPriceW * s, L.listHead.y, kPriceW * s, L.listHead.h}, t.price, kFaint,
                        11 * s, false, Align::Right});
    out.push_back(Words{panel::Box{weightR - kWeightW * s, L.listHead.y, kWeightW * s, L.listHead.h}, t.weight,
                        kFaint, 11 * s, false, Align::Right});
    if (d.waiting)
        out.push_back(Words{panel::Box{L.list.x + 6 * s, L.list.y + 8 * s, L.list.w - 12 * s, 22 * s}, t.waiting,
                            kMuted, 12.5f * s, false, Align::Left});
    for (size_t i = 0; i < L.wares.size(); ++i) {
        const panel::Box& b = L.wares[i];
        const int at = L.firstWare + int(i);
        const Ware* w = at < int(shown.size()) ? wareAt(d, shown[size_t(at)]) : nullptr;
        if (!w) continue;
        const float nx = b.x + (kIcon + 14) * s;
        std::string name = nameOf(*w);
        if (d.tab == Tab::Sell && w->count > 1) name += "  \xC3\x97" + std::to_string(w->count);
        out.push_back(Words{panel::Box{nx, b.y, priceR - kPriceW * s - nx - 8 * s, b.h}, name,
                            w->identified ? kText : kMuted, 12.5f * s, false, Align::Left});
        out.push_back(Words{panel::Box{priceR - kPriceW * s, b.y, kPriceW * s, b.h}, money(w->price), kText, 12.5f * s,
                            false, Align::Right});
        if (w->weight >= 0)
            out.push_back(Words{panel::Box{weightR - kWeightW * s, b.y, kWeightW * s, b.h}, weightText(w->weight),
                                kFaint, 12 * s, false, Align::Right});
    }
    // the cart
    out.push_back(Words{L.cartTitle, t.cart, kText, 13.5f * s, true, Align::Left});
    const float sumR = L.cart.x + L.cart.w - 6 * s;
    out.push_back(Words{panel::Box{L.cartHead.x + 6 * s, L.cartHead.y, 120 * s, L.cartHead.h}, t.ware, kFaint, 11 * s,
                        false, Align::Left});
    if (!L.less.empty() || d.cart.empty()) {
        const float ax = L.cart.x + L.cart.w - 100 * s - kAmountW * s;
        out.push_back(Words{panel::Box{ax - 10 * s, L.cartHead.y, kAmountW * s + 20 * s, L.cartHead.h}, t.amount,
                            kFaint, 11 * s, false, Align::Center});
    }
    out.push_back(Words{panel::Box{sumR - 90 * s, L.cartHead.y, 90 * s, L.cartHead.h}, t.sum, kFaint, 11 * s, false,
                        Align::Right});
    if (d.cart.empty())
        out.push_back(Words{panel::Box{L.cart.x + 6 * s, L.cart.y + 6 * s, L.cart.w - 12 * s, 24 * s}, t.empty, kFaint,
                            12 * s, false, Align::Left});
    for (size_t i = 0; i < L.lines.size(); ++i) {
        const panel::Box& b = L.lines[i];
        const Line& line = d.cart[size_t(L.firstLine) + i];
        const Ware* w = wareAt(d, line.ware);
        if (!w) continue;
        const float nx = b.x + (kIcon + 10) * s;
        out.push_back(Words{panel::Box{nx, b.y, L.less[i].x - nx - 8 * s, b.h}, nameOf(*w), kText, 12.5f * s, false,
                            Align::Left});
        const float ax = L.less[i].x + L.less[i].w, aw = L.more[i].x - ax;
        out.push_back(Words{panel::Box{ax, b.y, aw, b.h}, grouped(line.amount), kText, 12.5f * s, true,
                            Align::Center});
        out.push_back(Words{panel::Box{sumR - 96 * s, b.y, 96 * s, b.h}, money((long long)w->price * line.amount),
                            kText, 12.5f * s, false, Align::Right});
    }
    // the totals
    const Sums sum = sums(d.tab, d.cart, d.wares, d.zeny, d.weight, d.weightMost);
    const bool buy = d.tab == Tab::Buy;
    const std::string weight =
        d.weightMost > 0 ? grouped(sum.weightAfter) + " / " + grouped(d.weightMost) + " \xC2\xB7 " +
                               std::to_string(sum.weightPercent) + " %"
                         : grouped(sum.weightAfter);
    const uint32_t weightRgb = sum.weightPercent >= 90 ? kRed : sum.weightPercent >= 50 ? kAmber : kText;
    const std::pair<std::string, std::pair<std::string, uint32_t>> rows[3] = {
        {t.total, {money(sum.total), kText}},
        {buy ? t.left : t.after, {money(sum.zenyAfter), sum.zenyAfter < 0 ? kRed : kText}},
        {buy ? t.weightBuy : t.weightSell, {weight, weightRgb}}};
    for (int i = 0; i < 3; ++i) {
        const panel::Box row{L.totals.x, L.totals.y + float(i) * kLineH * s, L.totals.w, kLineH * s};
        out.push_back(Words{panel::Box{row.x, row.y, row.w * 0.5f, row.h}, rows[i].first, kMuted, 13 * s, false,
                            Align::Left});
        out.push_back(Words{panel::Box{row.x + row.w * 0.4f, row.y, row.w * 0.6f - 6 * s, row.h},
                            rows[i].second.first, rows[i].second.second, 13 * s, true, Align::Right});
    }
    out.push_back(Words{L.confirm, buy ? t.buyButton : t.sellButton, sum.ok ? kInk : kFaint, 13 * s, true,
                        Align::Center});
    out.push_back(Words{L.clear, t.clear, d.cart.empty() ? kFaint : kText, 13 * s, true, Align::Center});
    out.push_back(Words{L.note, t.note, kFaint, 11 * s, false, Align::Left});
    if (!sh.pad) {
        out.push_back(Words{panel::Box{side, L.footer.y, L.footer.w - 2 * side, L.footer.h}, t.mouse, kFaint, 11.5f * s,
                            false, Align::Left});
        return out;
    }
    const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
    const std::vector<PadItem> items = padItems(d, t);
    for (size_t i = 0; i < items.size() && i < L.padGlyphs.size(); ++i) {
        const padglyph::Letter l = padglyph::letter(items[i].button, f, L.padGlyphs[i].x, L.padGlyphs[i].y,
                                                    kGlyph * s);
        if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
        out.push_back(Words{L.padWords[i], *items[i].words, kText, 12 * s, false, Align::Left});
    }
    for (int i = 0; i < 2; ++i) {
        const padglyph::Letter l = padglyph::letter(i == 0 ? padglyph::Button::LB : padglyph::Button::RB, f,
                                                    L.tabGlyphs[i].x, L.tabGlyphs[i].y, kGlyph * s);
        if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
    }
    return out;
}

}  // namespace uagfx::shopwin
