#include "uikit/shop_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

#include "core/char_block.hpp"
#include "core/pad_glyphs.hpp"
#include "uikit/hits.hpp"

namespace uagfx::uikit::shop {
namespace {

namespace sw = uagfx::shopwin;
using charblock::grouped;

const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw shl" data-style-font-size="base + 'px'" data-style-background-color="ground">
<div class="head" data-hit="title"><span class="title">{{title}}</span><span class="pl">{{place}}</span></div>
<div class="x" data-hit="close" data-class-hot="closeHot">&#x2715;</div>
<div class="tabs"><img class="tg" data-if="pad" data-attr-src="lbGlyph" data-style-width="lbW + 'px'"
  data-style-height="glyphH + 'px'"/><div class="tab" data-hit="tab" data-attr-data-index="0" data-class-on="buyOn"
  data-class-hot="buyHot">{{buy}}</div><div class="tab" data-hit="tab" data-attr-data-index="1"
  data-class-on="!buyOn" data-class-hot="sellHot">{{sell}}</div><img class="tg" data-if="pad" data-attr-src="rbGlyph"
  data-style-width="rbW + 'px'" data-style-height="glyphH + 'px'"/></div>
<div class="cols"><div class="lcol">
<div class="search" data-hit="search" data-class-typing="typing"><img data-if="searchIcon != ''"
  data-attr-src="searchIcon"/><span class="q" data-if="query != ''">{{query}}</span><span class="caret"
  data-if="caret"></span><span class="q qh" data-if="hint != ''">{{hint}}</span></div>
<div class="lh"><span class="hw">{{wareWord}}</span><span class="hp2">{{priceWord}}</span><span
  class="hwt">{{weightWord}}</span></div>
<div class="wlist" data-hit="list"><div class="wait" data-if="isWaiting">{{waiting}}</div><div class="wr"
  data-for="w : wares" data-hit="ware" data-attr-data-index="w.index" data-class-unused="!w.used"
  data-class-hot="w.hot" data-class-framed="w.framed" data-class-incart="w.inCart"><div class="ic"><img
  data-if="w.icon != ''" data-attr-src="w.icon" data-style-width="w.iconW + 'px'"
  data-style-height="w.iconH + 'px'"/></div><span class="wn" data-class-unknown="w.unknown">{{w.name}}</span><span
  class="mk" data-if="w.mark != ''">{{w.mark}}</span><span class="pr">{{w.price}}</span><span
  class="wt">{{w.weight}}</span></div></div></div>
<div class="rcol"><div class="ct">{{cartWord}}</div>
<div class="ch"><span class="hw">{{wareWord}}</span><span class="ha" data-if="!cartEmpty">{{amountWord}}</span><span
  class="hs" data-if="!cartEmpty">{{sumWord}}</span></div>
<div class="cart" data-hit="cart"><div class="none" data-if="cartEmpty">{{empty}}</div><div class="line"
  data-for="l : lines" data-hit="line" data-attr-data-index="l.index" data-class-unused="!l.used"
  data-class-hot="l.hot" data-class-framed="l.framed"><div class="ic"><img data-if="l.icon != ''"
  data-attr-src="l.icon" data-style-width="l.iconW + 'px'" data-style-height="l.iconH + 'px'"/></div><span
  class="ln">{{l.name}}</span><span class="st" data-hit="less" data-attr-data-index="l.index"
  data-class-hot="l.lessHot">&#x2212;</span><div class="am" data-hit="amount" data-attr-data-index="l.index"
  data-class-typed="l.typed" data-class-hot="l.amountHot"><span>{{l.amount}}</span><span class="caret"
  data-if="l.caret"></span></div><span class="st" data-hit="more" data-attr-data-index="l.index"
  data-class-hot="l.moreHot">+</span><span class="sm">{{l.sum}}</span><span class="dr" data-hit="drop"
  data-attr-data-index="l.index" data-class-hot="l.dropHot">&#x00D7;</span></div></div>
<div class="tots"><div class="tot" data-for="r : totals"><span class="tl">{{r.label}}</span><span
  data-attr-class="r.cls">{{r.value}}</span></div></div>
<div class="acts"><div class="go" data-hit="confirm" data-class-ok="ok" data-class-hot="confirmHot"><span
  class="gw">{{confirm}}</span><img data-if="confirmGlyph != ''" data-attr-src="confirmGlyph"
  data-style-width="glyphW + 'px'" data-style-height="glyphH + 'px'"/><span class="cap"
  data-if="confirmGlyph == ''">{{confirmKey}}</span></div><div class="clr" data-hit="clear" data-class-on="clearOn"
  data-class-hot="clearHot">{{clear}}</div></div>
<div class="note" data-class-edit="editing">{{note}}</div></div></div>
<div class="foot" data-hit="footer" data-class-pad="pad"><div class="key" data-for="k : keys"><img
  data-if="k.glyph != ''" data-attr-src="k.glyph" data-style-width="k.w + 'px'" data-style-height="k.h + 'px'"/><span
  class="cap" data-if="k.glyph == ''">{{k.cap}}</span><span class="kw">{{k.words}}</span></div></div>
</body></rml>)RML";

std::string nameOf(const sw::Ware& w) { return w.refine > 0 ? "+" + std::to_string(w.refine) + " " + w.name : w.name; }

const sw::Ware* wareAt(const sw::Data& d, int at) {
    return at >= 0 && at < int(d.wares.size()) ? &d.wares[size_t(at)] : nullptr;
}

std::string filled(std::string form, const std::string& value) {
    if (const size_t at = form.find("{1}"); at != std::string::npos) form.replace(at, 3, value);
    return form;
}

panel::Box one(Rml::ElementDocument* doc, const char* part) {
    const std::vector<Placed> p = placesOf(doc, part);
    return p.empty() ? panel::Box{} : p.front().box;
}

}  // namespace

const char* rml() { return kRml; }

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto e = c.RegisterStruct<WareV>()) {
        e.RegisterMember("index", &WareV::index);
        e.RegisterMember("icon", &WareV::icon);
        e.RegisterMember("name", &WareV::name);
        e.RegisterMember("mark", &WareV::mark);
        e.RegisterMember("price", &WareV::price);
        e.RegisterMember("weight", &WareV::weight);
        e.RegisterMember("used", &WareV::used);
        e.RegisterMember("hot", &WareV::hot);
        e.RegisterMember("framed", &WareV::framed);
        e.RegisterMember("inCart", &WareV::inCart);
        e.RegisterMember("unknown", &WareV::unknown);
        e.RegisterMember("iconW", &WareV::iconW);
        e.RegisterMember("iconH", &WareV::iconH);
    }
    if (auto e = c.RegisterStruct<LineV>()) {
        e.RegisterMember("index", &LineV::index);
        e.RegisterMember("icon", &LineV::icon);
        e.RegisterMember("name", &LineV::name);
        e.RegisterMember("amount", &LineV::amount);
        e.RegisterMember("sum", &LineV::sum);
        e.RegisterMember("used", &LineV::used);
        e.RegisterMember("hot", &LineV::hot);
        e.RegisterMember("framed", &LineV::framed);
        e.RegisterMember("typed", &LineV::typed);
        e.RegisterMember("caret", &LineV::caret);
        e.RegisterMember("lessHot", &LineV::lessHot);
        e.RegisterMember("moreHot", &LineV::moreHot);
        e.RegisterMember("amountHot", &LineV::amountHot);
        e.RegisterMember("dropHot", &LineV::dropHot);
        e.RegisterMember("iconW", &LineV::iconW);
        e.RegisterMember("iconH", &LineV::iconH);
    }
    if (auto e = c.RegisterStruct<TotalV>()) {
        e.RegisterMember("label", &TotalV::label);
        e.RegisterMember("value", &TotalV::value);
        e.RegisterMember("cls", &TotalV::cls);
    }
    if (auto e = c.RegisterStruct<KeyV>()) {
        e.RegisterMember("cap", &KeyV::cap);
        e.RegisterMember("words", &KeyV::words);
        e.RegisterMember("glyph", &KeyV::glyph);
        e.RegisterMember("w", &KeyV::w);
        e.RegisterMember("h", &KeyV::h);
    }
    c.RegisterArray<std::vector<WareV>>();
    c.RegisterArray<std::vector<LineV>>();
    c.RegisterArray<std::vector<TotalV>>();
    c.RegisterArray<std::vector<KeyV>>();
    for (auto [name, v] : std::initializer_list<std::pair<const char*, Rml::String*>>{
             {"ground", &m.ground},         {"title", &m.title},           {"place", &m.place},
             {"buy", &m.buy},               {"sell", &m.sell},             {"query", &m.query},
             {"hint", &m.hint},             {"searchIcon", &m.searchIcon}, {"wareWord", &m.wareWord},
             {"priceWord", &m.priceWord},   {"weightWord", &m.weightWord}, {"cartWord", &m.cartWord},
             {"amountWord", &m.amountWord}, {"sumWord", &m.sumWord},       {"empty", &m.empty},
             {"waiting", &m.waiting},       {"confirm", &m.confirm},       {"confirmKey", &m.confirmKey},
             {"confirmGlyph", &m.confirmGlyph}, {"clear", &m.clear},       {"note", &m.note},
             {"lbGlyph", &m.lbGlyph},       {"rbGlyph", &m.rbGlyph}})
        c.Bind(name, v);
    for (auto [name, v] : std::initializer_list<std::pair<const char*, bool*>>{
             {"closeHot", &m.closeHot},   {"buyOn", &m.buyOn},         {"buyHot", &m.buyHot},
             {"sellHot", &m.sellHot},     {"typing", &m.typing},       {"caret", &m.caret},
             {"cartEmpty", &m.cartEmpty}, {"isWaiting", &m.isWaiting}, {"ok", &m.ok},
             {"confirmHot", &m.confirmHot}, {"clearHot", &m.clearHot}, {"clearOn", &m.clearOn},
             {"editing", &m.editing},     {"pad", &m.pad}})
        c.Bind(name, v);
    for (auto [name, v] : std::initializer_list<std::pair<const char*, float*>>{
             {"glyphW", &m.glyphW}, {"glyphH", &m.glyphH}, {"lbW", &m.lbW}, {"rbW", &m.rbW}})
        c.Bind(name, v);
    c.Bind("base", &m.base);
    c.Bind("wares", &m.wares);
    c.Bind("lines", &m.lines);
    c.Bind("totals", &m.totals);
    c.Bind("keys", &m.keys);
}

void fill(Model& m, const sw::Data& d, const std::vector<int>& shown, const sw::Show& sh, const sw::Texts& t,
          float scale, const Source& src, const Glyph& glyph) {
    using B = padglyph::Button;
    m.base = 12 * (scale > 0 ? scale : 1);
    m.title = d.name.empty() ? t.title : d.name;
    m.place = d.place;
    m.closeHot = sh.hoverClose;
    m.buy = t.buy;
    m.sell = t.sell;
    m.buyOn = d.tab == sw::Tab::Buy;
    m.buyHot = sh.hoverTab == 0;
    m.sellHot = sh.hoverTab == 1;
    m.pad = sh.pad;
    const float gh = std::round(17 * scale);
    m.glyphH = gh;
    if (sh.pad) {
        m.lbGlyph = glyph(int(B::LB), gh, m.lbW);
        m.rbGlyph = glyph(int(B::RB), gh, m.rbW);
    }
    m.typing = sh.typing;
    m.caret = sh.typing && sh.caretOn;
    m.query = sh.search;
    m.hint = sh.search.empty() && !sh.typing ? t.search : "";
    m.wareWord = t.ware;
    m.priceWord = t.price;
    m.weightWord = t.weight;
    m.isWaiting = d.waiting;
    m.waiting = t.waiting;
    const bool framed = sh.framed || sh.pad;
    m.wares.assign(size_t(sw::kListRows), WareV{});
    for (int i = 0; i < sw::kListRows; ++i) {
        WareV& v = m.wares[size_t(i)];
        const int at = sh.scroll + i;
        v.index = at;
        const sw::Ware* w = at < int(shown.size()) ? wareAt(d, shown[size_t(at)]) : nullptr;
        if (!w) continue;
        v.used = true;
        v.hot = at == sh.hoverWare;
        v.framed = framed && !sh.cartFocus && at == sh.ware;
        const int inCart = sw::amountOf(d.cart, shown[size_t(at)]);
        v.inCart = inCart > 0;
        v.mark = inCart > 0 ? "\xE2\x86\x92 " + grouped(inCart) : "";  // → 30: so many in the cart
        v.name = nameOf(*w);
        if (d.tab == sw::Tab::Sell && w->count > 1) v.name += "  \xC3\x97" + std::to_string(w->count);
        v.unknown = !w->identified;
        v.price = sw::money(w->price);
        v.weight = w->weight >= 0 ? sw::weightText(w->weight) : "";
        if (w->icon && w->icon->width > 0) {
            v.icon = src(w->icon);
            v.iconW = float(w->icon->width);
            v.iconH = float(w->icon->height);
        }
    }
    m.cartWord = t.cart;
    m.amountWord = t.amount;
    m.sumWord = t.sum;
    m.empty = t.empty;
    m.cartEmpty = d.cart.empty();
    m.lines.assign(size_t(sw::kCartRows), LineV{});
    for (int i = 0; i < sw::kCartRows; ++i) {
        LineV& v = m.lines[size_t(i)];
        const int at = sh.cartScroll + i;
        v.index = at;
        if (at >= int(d.cart.size())) continue;
        const sw::Line& line = d.cart[size_t(at)];
        const sw::Ware* w = wareAt(d, line.ware);
        if (!w) continue;
        v.used = true;
        v.hot = at == sh.hoverLine;
        v.framed = framed && sh.cartFocus && at == sh.line;
        v.name = nameOf(*w);
        v.typed = sh.edit.ware == line.ware;
        v.caret = v.typed && sh.caretOn;
        v.amount = v.typed ? sh.edit.digits : grouped(line.amount);
        v.sum = sw::money((long long)w->price * line.amount);
        v.lessHot = at == sh.hoverLess;
        v.moreHot = at == sh.hoverMore;
        v.amountHot = at == sh.hoverAmount;
        v.dropHot = at == sh.hoverTakeBack;
        if (w->icon && w->icon->width > 0) {
            v.icon = src(w->icon);
            v.iconW = float(w->icon->width);
            v.iconH = float(w->icon->height);
        }
    }
    const sw::Sums sum = sw::sums(d.tab, d.cart, d.wares, d.zeny, d.weight, d.weightMost);
    const bool buy = d.tab == sw::Tab::Buy;
    const std::string weight = d.weightMost > 0 ? grouped(sum.weightAfter) + " / " + grouped(d.weightMost) +
                                                      " \xC2\xB7 " + std::to_string(sum.weightPercent) + " %"
                                                : grouped(sum.weightAfter);
    m.totals = {TotalV{t.total, sw::money(sum.total), "tv"},
                TotalV{buy ? t.left : t.after, sw::money(sum.zenyAfter), sum.zenyAfter < 0 ? "tv red" : "tv"},
                TotalV{buy ? t.weightBuy : t.weightSell, weight,
                       sum.weightPercent >= 90 ? "tv red" : sum.weightPercent >= 50 ? "tv amber" : "tv"}};
    m.ok = sum.ok;
    m.confirm = buy ? t.buy : t.sell;
    m.confirmHot = sh.hoverConfirm;
    m.confirmKey = "Enter";
    m.confirmGlyph = sh.pad ? glyph(int(B::Start), gh, m.glyphW) : "";
    m.clear = t.clear;
    m.clearOn = !d.cart.empty();
    m.clearHot = sh.hoverClear;
    m.editing = sh.edit.ware >= 0;
    m.note = m.editing ? filled(t.editHint, grouped(sw::mostFor(d, sh.edit.ware))) : t.note;
    m.keys.clear();
    if (sh.pad) {  // the controller's buttons (from the right)
        const std::pair<B, const std::string*> items[5] = {
            {B::A, &t.plus}, {B::X, &t.minus}, {B::LT, &t.remove}, {B::Y, &t.info}, {B::B, &t.close}};
        for (const auto& [b, words] : items) {
            KeyV k;
            k.h = gh;
            k.glyph = glyph(int(b), gh, k.w);
            k.words = *words;
            m.keys.push_back(k);
        }
    } else {  // the keys (from the left)
        const std::pair<std::string, const std::string*> items[7] = {
            {"\xE2\x86\x91\xE2\x86\x93", &t.choose}, {"\xE2\x86\x90\xE2\x86\x92", &t.toCart},
            {t.space, &t.plus}, {"0\xE2\x80\x93" "9", &t.typeAmount}, {"Del", &t.remove}, {"F1", &t.info},
            {"Esc", &t.close}};
        for (const auto& [cap, words] : items) m.keys.push_back(KeyV{cap, *words, "", 0, 0});
    }
}

bool refit(Rml::ElementDocument* doc, sw::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
    if (size.x <= 0 || size.y <= 0) return false;
    const std::vector<Placed> tabs = placesOf(doc, "tab");
    if (tabs.size() != 2) return false;
    L.glass = panel::Box{0, 0, size.x, size.y};
    L.w = int(std::ceil(size.x));
    L.h = int(std::ceil(size.y));
    L.title = one(doc, "title");
    L.title = panel::Box{0, 0, size.x, L.title.y + L.title.h};
    L.close = one(doc, "close");
    L.tabs = {tabs[0].box, tabs[1].box};
    L.searchBox = one(doc, "search");
    L.list = one(doc, "list");
    L.cart = one(doc, "cart");
    L.confirm = one(doc, "confirm");
    L.clear = one(doc, "clear");
    L.footer = one(doc, "footer");
    const auto rows = [&](const char* part, int first, std::vector<panel::Box>& to) {
        for (panel::Box& b : to) b = panel::Box{};
        for (const Placed& p : placesOf(doc, part)) {
            const int i = p.index - first;
            if (i >= 0 && i < int(to.size())) to[size_t(i)] = p.box;
        }
    };
    rows("ware", L.firstWare, L.wares);
    rows("line", L.firstLine, L.lines);
    rows("less", L.firstLine, L.less);
    rows("amount", L.firstLine, L.amounts);
    rows("more", L.firstLine, L.more);
    rows("drop", L.firstLine, L.drops);
    return true;
}

}  // namespace uagfx::uikit::shop
