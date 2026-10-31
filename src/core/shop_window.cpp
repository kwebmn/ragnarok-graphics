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
constexpr float kPriceW = 110, kWeightW = 64, kStepBox = 22, kAmountBox = 52, kSumW = 90, kCapH = 18;
constexpr int kNear = 15;  // cells: the NPC a character trades with stands this near
constexpr size_t kMostDigits = 5;
constexpr uint32_t kRepeatAfterMs = 400, kRepeatEveryMs = 80, kFasterAfterMs = 1500;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653, kGold2 = 0xC9A55B;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kInk = 0x1A1408;
constexpr uint32_t kAmber = 0xF5BD4F, kRed = 0xFF8A80;

std::string lowered(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

std::string fill(std::string form, const std::vector<std::string>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        const std::string key = "{" + std::to_string(i + 1) + "}";
        for (size_t at = form.find(key); at != std::string::npos; at = form.find(key, at + values[i].size()))
            form.replace(at, key.size(), values[i]);
    }
    return form;
}

void blit(panel::Picture& out, const panel::Picture& pic, const panel::Box& box, float k) {
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

const Ware* wareAt(const Data& d, int at) {
    return at >= 0 && at < int(d.wares.size()) ? &d.wares[size_t(at)] : nullptr;
}

void place(Data& d, int ware, int amount) {  // the row's amount, made when it had none (0 kept)
    for (Line& l : d.cart)
        if (l.ware == ware) {
            l.amount = amount;
            return;
        }
    d.cart.push_back(Line{ware, amount});
}

int digitsValue(const std::string& digits) { return digits.empty() ? 0 : std::atoi(digits.c_str()); }

struct PadItem {
    padglyph::Button button;
    const std::string* words;
};

std::vector<PadItem> padItems(const Texts& t) {
    return {{padglyph::Button::A, &t.plus},
            {padglyph::Button::X, &t.minus},
            {padglyph::Button::LT, &t.remove},
            {padglyph::Button::Y, &t.info},
            {padglyph::Button::B, &t.close}};
}

struct KeyItem {
    std::string cap;
    const std::string* words;
};

std::vector<KeyItem> keyItems(const Texts& t) {
    return {{"\xE2\x86\x91\xE2\x86\x93", &t.choose},  // ↑↓
            {"\xE2\x86\x90\xE2\x86\x92", &t.toCart},  // ←→
            {t.space, &t.plus},
            {"0\xE2\x80\x93" "9", &t.typeAmount},  // 0–9
            {"Del", &t.remove},
            {"F1", &t.info},
            {"Esc", &t.close}};
}

}  // namespace

int mostFor(const Data& d, int ware) {
    const Ware* w = wareAt(d, ware);
    if (!w) return 0;
    if (d.tab == Tab::Sell) return std::max(0, w->count);
    long long most = kMostBought, others = 0, othersWeight = 0;
    for (const Line& l : d.cart) {
        const Ware* o = l.ware == ware ? nullptr : wareAt(d, l.ware);
        if (!o) continue;
        others += (long long)o->price * l.amount;
        if (o->weight > 0) othersWeight += (long long)o->weight * l.amount;
    }
    if (w->price > 0) most = std::min(most, std::max(0LL, (d.zeny - others) / w->price));
    if (w->weight > 0 && d.weightMost > 0)
        most = std::min(most, std::max(0LL, ((long long)d.weightMost - d.weight) * 10 - othersWeight) / w->weight);
    return int(std::max(0LL, most));
}

void put(Data& d, int ware, int by) {
    if (!wareAt(d, ware) || by == 0) return;
    const int amount = int(std::clamp<long long>((long long)amountOf(d.cart, ware) + by, 0, mostFor(d, ware)));
    if (amount > 0)
        place(d, ware, amount);
    else
        takeBack(d, ware);
}

void takeBack(Data& d, int ware) {
    d.cart.erase(std::remove_if(d.cart.begin(), d.cart.end(), [&](const Line& l) { return l.ware == ware; }),
                 d.cart.end());
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

void typeDigit(Data& d, Edit& e, int ware, char digit) {
    if (digit < '0' || digit > '9' || !wareAt(d, ware)) return;
    if (e.ware != ware) {  // another ware: the one typed before stays as typed
        endEdit(d, e);
        e.ware = ware;
        e.digits.clear();
        e.before = amountOf(d.cart, ware);
    }
    if (e.digits.size() >= kMostDigits) return;
    e.digits += digit;
    while (e.digits.size() > 1 && e.digits[0] == '0') e.digits.erase(e.digits.begin());
    const int top = mostFor(d, ware), want = digitsValue(e.digits);
    if (want > top) e.digits = std::to_string(top);
    place(d, ware, std::min(want, top));
}

void eraseDigit(Data& d, Edit& e) {
    if (e.ware < 0) return;
    if (!e.digits.empty()) e.digits.pop_back();
    place(d, e.ware, digitsValue(e.digits));
}

void endEdit(Data& d, Edit& e) {
    if (e.ware < 0) return;
    if (amountOf(d.cart, e.ware) <= 0) takeBack(d, e.ware);
    e = Edit{};
}

void cancelEdit(Data& d, Edit& e) {
    if (e.ware < 0) return;
    if (e.before > 0)
        place(d, e.ware, e.before);
    else
        takeBack(d, e.ware);
    e = Edit{};
}

int HoldRepeat::step(bool held, uint32_t nowMs) {
    if (!held) {
        held_ = false;
        return 0;
    }
    if (!held_) {
        held_ = true;
        downAt_ = lastAt_ = nowMs;
        return 1;
    }
    const uint32_t heldMs = nowMs - downAt_;
    if (heldMs < kRepeatAfterMs || nowMs - lastAt_ < kRepeatEveryMs) return 0;
    lastAt_ = nowMs;
    return heldMs >= kFasterAfterMs ? 10 : 1;
}

Sums sums(Tab tab, const std::vector<Line>& cart, const std::vector<Ware>& wares, long long zeny, int weight,
          int weightMost) {
    Sums s;
    long long tenths = 0;
    for (const Line& l : cart) {
        if (l.ware < 0 || l.ware >= int(wares.size()) || l.amount <= 0) continue;
        const Ware& w = wares[size_t(l.ware)];
        s.total += (long long)w.price * l.amount;
        if (w.weight > 0) tenths += (long long)w.weight * l.amount;
    }
    const bool buy = tab == Tab::Buy;
    s.zenyAfter = buy ? zeny - s.total : zeny + s.total;
    const long long after = (long long)weight * 10 + (buy ? tenths : -tenths);
    s.weightAfter = std::max(0LL, after / 10);
    s.weightPercent = weightMost > 0 ? int(s.weightAfter * 100 / weightMost) : 0;
    s.ok = s.total > 0 && (!buy || (s.zenyAfter >= 0 && (weightMost <= 0 || after <= (long long)weightMost * 10)));
    if (!s.ok && s.total == 0)  // free wares: anything in the cart goes
        for (const Line& l : cart) s.ok = s.ok || l.amount > 0;
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
    L.firstWare = sh.scroll;
    L.firstLine = sh.cartScroll;
    L.searchBox = panel::Box{side, top, kListW * s, kSearchH * s};
    L.listHead = panel::Box{side, top + (kSearchH + 8) * s, kListW * s, kHeadH * s};
    L.list = panel::Box{side, L.listHead.y + L.listHead.h, kListW * s, kListRows * kRowH * s};
    for (int i = sh.scroll; i < int(shown.size()) && i - sh.scroll < kListRows; ++i)
        L.wares.push_back(panel::Box{L.list.x, L.list.y + float(i - sh.scroll) * kRowH * s, L.list.w, kRowH * s});
    // the cart on the right: a row's − amount +, its sum and the cross taking it back, from its right
    const float cx = side + (kListW + kGapW) * s, cw = w - side - cx;
    L.cartTitle = panel::Box{cx, top, cw, kSearchH * s};
    L.cartHead = panel::Box{cx, L.listHead.y, cw, kHeadH * s};
    L.cart = panel::Box{cx, L.cartHead.y + L.cartHead.h, cw, kCartRows * kRowH * s};
    for (int i = sh.cartScroll; i < int(d.cart.size()) && i - sh.cartScroll < kCartRows; ++i) {
        const panel::Box row{cx, L.cart.y + float(i - sh.cartScroll) * kRowH * s, cw, kRowH * s};
        const float by = row.y + (row.h - kStepBox * s) / 2;
        const float dropX = row.x + row.w - 6 * s - kStepBox * s;
        const float moreX = dropX - 8 * s - kSumW * s - 10 * s - kStepBox * s;
        const float amountX = moreX - 4 * s - kAmountBox * s, lessX = amountX - 4 * s - kStepBox * s;
        L.lines.push_back(row);
        L.less.push_back(panel::Box{lessX, by, kStepBox * s, kStepBox * s});
        L.amounts.push_back(panel::Box{amountX, by, kAmountBox * s, kStepBox * s});
        L.more.push_back(panel::Box{moreX, by, kStepBox * s, kStepBox * s});
        L.drops.push_back(panel::Box{dropX, by, kStepBox * s, kStepBox * s});
    }
    L.totals = panel::Box{cx, L.cart.y + L.cart.h + 10 * s, cw, 3 * kLineH * s};
    // the confirm: «Купити» with its key - Enter, or the controller's Start
    const std::string& button = d.tab == Tab::Buy ? t.buy : t.sell;
    const float keyW = sh.pad ? padglyph::widthOf(padglyph::Button::Start, kGlyph * s)
                              : m("Enter", 11 * s, true) + 12 * s;
    const float bw = 20 * s + m(button, 13.5f * s, true) + 10 * s + keyW + 14 * s;
    const float clw = m(t.clear, 13 * s, true) + 28 * s;
    L.confirm = panel::Box{cx, L.totals.y + L.totals.h + 10 * s, bw, kButtonH * s};
    const float keyH = sh.pad ? kGlyph * s : kCapH * s;
    L.confirmKey = panel::Box{L.confirm.x + L.confirm.w - 14 * s - keyW, L.confirm.y + (L.confirm.h - keyH) / 2,
                              keyW, keyH};
    L.clear = panel::Box{L.confirm.x + L.confirm.w + 10 * s, L.confirm.y, clw, kButtonH * s};
    L.note = panel::Box{cx, L.confirm.y + L.confirm.h + 6 * s, cw, 18 * s};
    const float bottom = std::max(L.list.y + L.list.h, L.note.y + L.note.h);
    L.footer = panel::Box{0, bottom + 10 * s, w, kFooterH * s};
    if (sh.pad) {  // the controller's buttons in the footer, from its right
        const std::vector<PadItem> items = padItems(t);
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
    } else {  // the keys and their words, from its left
        float left = side;
        for (const KeyItem& k : keyItems(t)) {
            const float capW = m(k.cap, 11 * s, true) + 12 * s, ww = m(*k.words, 12 * s, false) + 2;
            L.keyCaps.push_back(panel::Box{left, L.footer.y + (L.footer.h - kCapH * s) / 2, capW, kCapH * s});
            L.keyWords.push_back(panel::Box{left + capW + 6 * s, L.footer.y, ww, L.footer.h});
            left += capW + 6 * s + ww + 16 * s;
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
        if (L.amounts[i].contains(x, y)) return Hit{Part::Amount, at};
        if (L.more[i].contains(x, y)) return Hit{Part::More, at};
        if (L.drops[i].contains(x, y)) return Hit{Part::TakeBack, at};
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
    const bool framed = sh.framed || sh.pad;
    // the list's rows
    panel::roundRect(out, panel::Box{L.listHead.x, L.listHead.y + L.listHead.h - 1 * s, L.listHead.w, 1 * s}, 0,
                     kWhite, 0.08f);
    for (size_t i = 0; i < L.wares.size(); ++i) {
        const panel::Box& b = L.wares[i];
        const int at = L.firstWare + int(i);
        if (at == sh.hoverWare) panel::roundRect(out, b, 6 * s, kWhite, 0.06f);
        if (framed && !sh.cartFocus && at == sh.ware)
            panel::roundRect(out, b, 6 * s, kGold2, 0.08f, kGold, 0.9f, 1.5f * s);
        if (at < int(shown.size()) && amountOf(d.cart, shown[size_t(at)]) > 0)  // in the cart: a gold mark at its start
            panel::roundRect(out, panel::Box{b.x + 1 * s, b.y + 8 * s, 2.5f * s, b.h - 16 * s}, 1 * s, kGold, 0.9f);
        panel::roundRect(out, panel::Box{b.x, b.y + b.h - 1 * s, b.w, 1 * s}, 0, kWhite, 0.05f);
        const Ware* w = at < int(shown.size()) ? wareAt(d, shown[size_t(at)]) : nullptr;
        if (w && w->icon && w->icon->width > 0)
            blit(out, *w->icon, panel::Box{b.x + 6 * s, b.y + (b.h - kIcon * s) / 2, kIcon * s, kIcon * s},
                 w->identified ? 1.0f : 0.45f);
    }
    // the cart's rows: − the amount (a field while typed) + and the cross taking it back
    panel::roundRect(out, panel::Box{L.cartHead.x, L.cartHead.y + L.cartHead.h - 1 * s, L.cartHead.w, 1 * s}, 0,
                     kWhite, 0.08f);
    for (size_t i = 0; i < L.lines.size(); ++i) {
        const panel::Box& b = L.lines[i];
        const int at = L.firstLine + int(i);
        const Line& line = d.cart[size_t(at)];
        if (at == sh.hoverLine) panel::roundRect(out, b, 6 * s, kWhite, 0.06f);
        if (framed && sh.cartFocus && at == sh.line)
            panel::roundRect(out, b, 6 * s, kGold2, 0.08f, kGold, 0.9f, 1.5f * s);
        panel::roundRect(out, panel::Box{b.x, b.y + b.h - 1 * s, b.w, 1 * s}, 0, kWhite, 0.05f);
        const Ware* w = wareAt(d, line.ware);
        if (w && w->icon && w->icon->width > 0)
            blit(out, *w->icon, panel::Box{b.x + 4 * s, b.y + (b.h - kIcon * s) / 2, kIcon * s, kIcon * s}, 1.0f);
        const bool typed = sh.edit.ware == line.ware;
        if (typed) {
            panel::roundRect(out, L.amounts[i], 5 * s, kBlack, 0.45f, kGold, 0.95f, 1.5f * s);
            if (sh.caretOn) {
                const float cx = L.amounts[i].x + L.amounts[i].w / 2 + m(sh.edit.digits, 12.5f * s, true) / 2 + 2 * s;
                panel::roundRect(out, panel::Box{cx, L.amounts[i].y + 4 * s, 1.5f * s, L.amounts[i].h - 8 * s}, 0,
                                 kText, 0.95f);
            }
        } else if (at == sh.hoverAmount) {
            panel::roundRect(out, L.amounts[i], 5 * s, kWhite, 0.10f);
        }
        const auto cell = [&](const panel::Box& box, bool hot, uiicons::Icon glyph) {
            panel::roundRect(out, box, 5 * s, kWhite, hot ? 0.16f : 0.07f);
            const float g = 12 * s;
            uiicons::draw(out, glyph, box.x + (box.w - g) / 2, box.y + (box.h - g) / 2, g, kText, 0.9f, 2.4f);
        };
        cell(L.less[i], at == sh.hoverLess, uiicons::Icon::Minus);
        cell(L.more[i], at == sh.hoverMore, uiicons::Icon::Plus);
        if (at == sh.hoverTakeBack) panel::roundRect(out, L.drops[i], 5 * s, kRed, 0.18f);
    }
    // the buttons; the confirm's key on it
    const Sums sum = sums(d.tab, d.cart, d.wares, d.zeny, d.weight, d.weightMost);
    if (sum.ok)
        panel::roundRect(out, L.confirm, 8 * s, sh.hoverConfirm ? 0xF8D57A : kGold, 1.0f);
    else
        panel::roundRect(out, L.confirm, 8 * s, kWhite, 0.07f, kWhite, 0.10f, 1.0f * s);
    const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
    if (sh.pad)
        padglyph::paint(out, padglyph::Button::Start, f, L.confirmKey.x, L.confirmKey.y, kGlyph * s);
    else
        panel::roundRect(out, L.confirmKey, 4 * s, sum.ok ? kInk : kWhite, sum.ok ? 0.16f : 0.10f);
    panel::roundRect(out, L.clear, 8 * s, kWhite, sh.hoverClear ? 0.14f : 0.07f);
    panel::roundRect(out, panel::Box{0, L.footer.y, L.glass.w, 1 * s}, 0, kWhite, 0.08f);
    for (const panel::Box& cap : L.keyCaps) panel::roundRect(out, cap, 4 * s, kWhite, 0.12f, kWhite, 0.18f, 1.0f * s);
    if (sh.pad) {
        Texts none;
        const std::vector<PadItem> items = padItems(none);
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
        const float nx = b.x + (kIcon + 14) * s, nameEnd = priceR - kPriceW * s - 8 * s;
        std::string name = nameOf(*w);
        if (d.tab == Tab::Sell && w->count > 1) name += "  \xC3\x97" + std::to_string(w->count);
        const int inCart = amountOf(d.cart, shown[size_t(at)]);
        const std::string mark = inCart > 0 ? "\xE2\x86\x92 " + grouped(inCart) : "";  // → 30: so many in the cart
        const float markW = mark.empty() ? 0 : m(mark, 11.5f * s, true) + 6 * s;
        out.push_back(Words{panel::Box{nx, b.y, nameEnd - nx - markW, b.h}, name, w->identified ? kText : kMuted,
                            12.5f * s, false, Align::Left});
        if (!mark.empty())
            out.push_back(Words{panel::Box{nameEnd - markW, b.y, markW, b.h}, mark, kGold, 11.5f * s, true,
                                Align::Right});
        out.push_back(Words{panel::Box{priceR - kPriceW * s, b.y, kPriceW * s, b.h}, money(w->price), kText, 12.5f * s,
                            false, Align::Right});
        if (w->weight >= 0)
            out.push_back(Words{panel::Box{weightR - kWeightW * s, b.y, kWeightW * s, b.h}, weightText(w->weight),
                                kFaint, 12 * s, false, Align::Right});
    }
    // the cart
    out.push_back(Words{L.cartTitle, t.cart, kText, 13.5f * s, true, Align::Left});
    out.push_back(Words{panel::Box{L.cartHead.x + 6 * s, L.cartHead.y, 120 * s, L.cartHead.h}, t.ware, kFaint, 11 * s,
                        false, Align::Left});
    if (!L.less.empty()) {
        const float ax = L.less[0].x, aw = L.more[0].x + L.more[0].w - ax;
        out.push_back(Words{panel::Box{ax - 10 * s, L.cartHead.y, aw + 20 * s, L.cartHead.h}, t.amount, kFaint,
                            11 * s, false, Align::Center});
        const float sumR = L.drops[0].x - 8 * s;
        out.push_back(Words{panel::Box{sumR - kSumW * s, L.cartHead.y, kSumW * s, L.cartHead.h}, t.sum, kFaint,
                            11 * s, false, Align::Right});
    }
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
        const bool typed = sh.edit.ware == line.ware;
        out.push_back(Words{L.amounts[i], typed ? sh.edit.digits : grouped(line.amount), kText, 12.5f * s, true,
                            Align::Center});
        const float sumR = L.drops[i].x - 8 * s;
        out.push_back(Words{panel::Box{sumR - kSumW * s, b.y, kSumW * s, b.h}, money((long long)w->price * line.amount),
                            kText, 12.5f * s, false, Align::Right});
        out.push_back(Words{L.drops[i], "\xC3\x97", int(L.firstLine + int(i)) == sh.hoverTakeBack ? kRed : kMuted,
                            14 * s, false, Align::Center});
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
    // the confirm with its key, the clear; under them the typing's hint, else the note
    out.push_back(Words{panel::Box{L.confirm.x + 20 * s, L.confirm.y, L.confirmKey.x - L.confirm.x - 30 * s,
                                   L.confirm.h},
                        buy ? t.buy : t.sell, sum.ok ? kInk : kFaint, 13.5f * s, true, Align::Left});
    const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
    if (sh.pad) {
        const padglyph::Letter l = padglyph::letter(padglyph::Button::Start, f, L.confirmKey.x, L.confirmKey.y,
                                                    kGlyph * s);
        if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
    } else {
        out.push_back(Words{L.confirmKey, "Enter", sum.ok ? kInk : kFaint, 11 * s, true, Align::Center});
    }
    out.push_back(Words{L.clear, t.clear, d.cart.empty() ? kFaint : kText, 13 * s, true, Align::Center});
    if (sh.edit.ware >= 0)
        out.push_back(Words{L.note, fill(t.editHint, {grouped(mostFor(d, sh.edit.ware))}), kGold, 11 * s, false,
                            Align::Left});
    else
        out.push_back(Words{L.note, t.note, kFaint, 11 * s, false, Align::Left});
    // the footer: the keys, or the controller's buttons
    for (size_t i = 0; i < L.keyCaps.size(); ++i) {
        const std::vector<KeyItem> items = keyItems(t);
        if (i >= items.size()) break;
        out.push_back(Words{L.keyCaps[i], items[i].cap, kText, 11 * s, true, Align::Center});
        out.push_back(Words{L.keyWords[i], *items[i].words, kMuted, 12 * s, false, Align::Left});
    }
    if (!sh.pad) return out;
    const std::vector<PadItem> items = padItems(t);
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
