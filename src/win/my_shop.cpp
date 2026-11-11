#include "win/my_shop.hpp"

#include <map>
#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/card_names.hpp"
#include "core/char_block.hpp"
#include "core/window_stack.hpp"
#include "core/hotbar_panel.hpp"
#include "core/item_window.hpp"
#include "core/my_shop.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/client_tables.hpp"
#include "win/info_card.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::myshopwin {
namespace {

namespace ms = uagfx::myshop;
namespace iw = uagfx::itemwin;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the own-shop window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown
// flag; the session's list of the things for sale (a node's ITEM_INFO at +8: the item's number as words +0x30 /
// +0x34, how many +0x10, the price +0x18, known +0x3C, refined +0x40); the shop's sign the game keeps (a char*)
constexpr uint32_t kWindowMgr = 0x727ED8, kShopAt = kWindowMgr + 0x150, kShopClass = 0x69B478, kShopSend = 0x4B0A60;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kSession = 0x771148, kForSale = kSession + 0xF7C, kInfo = 8;
constexpr uint32_t kSign = 0x772D98;
constexpr int kButtonMsg = 6;
constexpr size_t kInfoSize = 0x48, kMostNodes = 100;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

// 3.0.18-beta.90 (live windows, group 3): one's own open shop a live window - what is left by flexbox, its worth,
// «Закрити лавку». 3.0.18-beta.143 (the windows' audit): each thing's sold count, what is inside it, its card by a
// right click; «Зароблено» and the sales as the server tells them (0x0137, the newest first)
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw vsw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<span class="x ctl" data-event-click="ms_close()">&#x2715;</span>
<div class="list"><div class="ware ctl" data-for="r, i : rows" data-event-mousedown="ms_card(i)"><img
  data-if="r.icon != ''" data-attr-src="r.icon"/>
<div class="wname"><span class="name" data-style-color="r.colour">{{r.name}}</span><span class="inside"
  data-if="r.inside != ''">{{r.inside}}</span><span class="stock">{{r.left}}<span class="sold"
  data-if="r.sold != ''"> &#xB7; {{r.sold}}</span></span></div>
<span class="price">{{r.price}}</span></div>
<div class="hint" data-if="rows.size == 0">{{empty}}</div></div>
<div class="sums"><div class="srow"><span class="lbl">{{worth_word}}</span><span
  class="val">{{worth}}</span></div><div class="srow"><span class="lbl">{{earned_word}}</span><span
  class="val good">{{earned}}</span></div></div>
<div class="lbl">{{sales_word}}</div>
<div class="sales"><div class="sale" data-for="x : sales"><span class="t">{{x.time}}</span><span
  class="what">{{x.what}}</span><span class="z">{{x.zeny}}</span></div>
<div class="hint" data-if="sales.size == 0">{{no_sales}}</div></div>
<div class="hint">{{tip}}</div>
<div class="btns"><div class="btn main ctl" data-event-click="ms_close()">{{close}}</div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
struct OwnWareView {
    Rml::String name, left, price, icon, inside, sold, colour = "#eef2f8";
    bool operator==(const OwnWareView& o) const {
        return name == o.name && left == o.left && price == o.price && icon == o.icon && inside == o.inside &&
               sold == o.sold && colour == o.colour;
    }
};
struct SaleView {  // 3.0.18-beta.143
    Rml::String time, what, zeny;
    bool operator==(const SaleView& o) const { return time == o.time && what == o.what && zeny == o.zeny; }
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<OwnWareView> rowViews;
    std::vector<SaleView> saleViews;  // 3.0.18-beta.143
    Rml::String earnedWord, earned, salesWord, noSales, tip;
    ms::Ledger ledger;
    bool open = false;  // the shop seen open (its sales kept till it closes)
    std::map<const panel::Picture*, std::string> iconPrev;
    Rml::String title, worthWord, worth, close, empty, ground = "#10141cf0";
    uisheet::Sheet sheet;  // 3.0.18-beta.30: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<ms::Row> rows;
    std::string sign;
    ms::Show show;
    ms::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t rd32(const uint8_t* p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}

uint32_t gameShop() {
    const uint32_t w = read32(kShopAt);
    return w && read32(w) == kShopClass && read32(read32(w) + kSendMsgSlot) == kShopSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("my shop: the game's own-shop window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

void readShop(State& s) {
    s.rows.clear();
    const auto& names = clienttables::itemNames();
    const auto& unknown = clienttables::unknownNames();
    const auto& files = clienttables::itemFiles();
    const uint32_t head = read32(kForSale);
    if (head) {
        uint32_t node = read32(head);
        for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
            uint8_t raw[kInfoSize] = {};
            if (!readGame(node + kInfo, raw, sizeof raw)) break;
            const uint32_t ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
            char digits[17] = {};
            if (!ptr || !len || len > 16 || !readGame(ptr, digits, len)) continue;
            ms::Row r;
            const std::string id = std::to_string(std::atoi(digits));
            std::string words;
            if (!raw[0x3C]) {
                const auto u = unknown.find(id);
                if (u != unknown.end()) words = u->second;
            }
            if (words.empty()) {
                const auto w = names.find(id);
                if (w != names.end()) words = w->second;
            }
            r.name = words.empty() ? "#" + id : utf8Of(widened(hotbar::spaced(words), s.codePage));
            // 3.0.18-beta.118 (the owner: «в вендинге не видно карт»): its cards in its name, as the game names it
            r.name = cardnames::namedFromInfo(r.name, raw, clienttables::cardWords());
            r.left = int(rd32(raw, 0x10));
            r.price = int32_t(rd32(raw, 0x18));
            const int refine = int(rd32(raw, 0x40));
            r.refine = refine > 0 && refine <= 20 ? refine : 0;
            const auto f = files.find(id);
            if (f != files.end()) r.icon = clienttables::icon(f->second, iw::iconSide(s.scale) * 7 / 8, 4 * s.scale);
            r.index = int(rd32(raw, 0x08));  // 3.0.18-beta.143
            r.info = node + kInfo;
            infocardwin::describe(raw, std::atoi(id.c_str()), s.codePage, r.name, r.inside);
            if (const std::string e = infocardwin::elementHex(std::atoi(id.c_str()), raw); !e.empty()) r.colour = e;
            s.ledger.stock(r.index, r.refine > 0 ? "+" + std::to_string(r.refine) + " " + r.name : r.name, r.price);
            s.rows.push_back(std::move(r));
        }
    }
    const uint32_t sign = read32(kSign);
    char buf[81] = {};
    s.sign = sign && readGame(sign, buf, 80) ? utf8Of(widened(buf, s.codePage)) : "";
}

ms::Texts texts(const State& s) {
    ms::Texts t;
    t.title = s.sign.empty() ? say(settings::Text::MyShopTitle)
                             : settings::fmt(say(settings::Text::MyShopSign), {s.sign});
    t.left = say(settings::Text::MyShopLeft);
    t.worth = say(settings::Text::MyShopWorth);
    t.close = say(settings::Text::MyShopClose);
    t.empty = say(settings::Text::MyShopSold);
    return t;
}

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // 3.0.18-beta.81: the live window's own box
    if (s.live.box(lx, ly, lw, lh)) x0 = float(lx), y0 = float(ly), b.w = float(lw), b.h = float(lh);
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(Blocker::MyShop, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::MyShop, nullptr, nullptr, false);
    s.shown = false;
}

void closeShop(State& s) {
    const uint32_t window = gameShop();
    if (!window) return;
    if (s.told++ < 30) LOG_INFO("my shop: «Закрити лавку» - the window's 0x87 (the game closes the shop)");
    reinterpret_cast<SendMsgFn>(uintptr_t(kShopSend))(window, 0, kButtonMsg, ms::kCloseButton, 0, 0);
    s.key.clear();
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("my shop: [interface] my_shop = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameShop() : 0;
    if (world) hold(s, window, on);
    if (window && !s.open) {  // 3.0.18-beta.143: the shop opened - its sales from none
        s.open = true;
        s.ledger.clear();
    } else if (!window && world) {
        s.open = false;
    }
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) LOG_INFO("my shop: the game's own-shop window - the new card shows it");
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readShop(s);
    s.layout = ms::layout(targetWidth, targetHeight, s.scale, s.rows.size());
    s.show.scroll = std::clamp(s.show.scroll, 0, ms::mostScroll(s.layout, s.rows.size()));
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        s.show.hoverClose = ms::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                        float(cy) * float(k) - float(s.layout.y))
                                .part == ms::Part::Close;
    std::string key = s.sign + ';' + std::to_string(s.show.scroll) + (s.show.hoverClose ? "x" : "") + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (const ms::Row& r : s.rows)
        key += ';' + r.name + ',' + std::to_string(r.left) + ',' + std::to_string(r.price) + (r.icon ? "i" : "");
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.30
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.30 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): one's own shop in RmlUi's
// markup, line by line as core myshop paints it - its glass, ✕, the empty list's ground, the rows with their icons,
// «Закрити лавку»; the words where core myshop places them
static void fillSheet(State& s, uisheet::Content& c) {
    const ms::Layout& L = s.layout;
    const ms::Show& sh = s.show;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653;
    auto& u = c.under;
    c.w = float(L.w), c.h = float(L.h);
    c.glass = uisheet::Box{L.glass.x, L.glass.y, L.glass.w, L.glass.h, 12 * sc, 1.0f * sc, 1, "glass"};
    c.ground = uisheet::Sheet::ground(kCard, uistyle::window());
    c.rect(u, L.cross, 6 * sc, kWhite, sh.hoverClose ? 0.14f : 0.06f);
    if (L.rows.empty()) c.rect(u, L.list, 8 * sc, kBlack, 0.16f);
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= s.rows.size()) break;
        c.rect(u, L.rows[i], 8 * sc, kWhite, 0.035f);
        const panel::Box b = ms::iconOf(L.rows[i], sc);
        c.rect(u, b, 6 * sc, kBlack, 0.30f);
        const panel::Picture* p = s.rows[at].icon;
        if (p && p->width > 0)
            c.icons.push_back(uisheet::Icon{std::round(b.x + (b.w - float(p->width)) / 2),
                                            std::round(b.y + (b.h - float(p->height)) / 2), float(p->width),
                                            float(p->height), 1, s.sheet.icon(p)});
    }
    c.rect(u, L.close, 8 * sc, kWhite, sh.hoverClose ? 0.14f : 0.07f, kGold, 0.6f, 1.0f * sc);
    c.say(ms::words(L, s.rows, sh, texts(s)));
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("my shop", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<OwnWareView>()) {
            v.RegisterMember("name", &OwnWareView::name);
            v.RegisterMember("left", &OwnWareView::left);
            v.RegisterMember("price", &OwnWareView::price);
            v.RegisterMember("icon", &OwnWareView::icon);
            v.RegisterMember("inside", &OwnWareView::inside);
            v.RegisterMember("sold", &OwnWareView::sold);
            v.RegisterMember("colour", &OwnWareView::colour);
        }
        m.RegisterArray<std::vector<OwnWareView>>();
        if (auto v = m.RegisterStruct<SaleView>()) {
            v.RegisterMember("time", &SaleView::time);
            v.RegisterMember("what", &SaleView::what);
            v.RegisterMember("zeny", &SaleView::zeny);
        }
        m.RegisterArray<std::vector<SaleView>>();
        m.Bind("sales", &s.saleViews);
        m.Bind("earned_word", &s.earnedWord);
        m.Bind("earned", &s.earned);
        m.Bind("sales_word", &s.salesWord);
        m.Bind("no_sales", &s.noSales);
        m.Bind("tip", &s.tip);
        m.BindEventCallback("ms_card", [](Rml::DataModelHandle, Rml::Event& ev, const Rml::VariantList& a) {
            State& st = state();  // 3.0.18-beta.143: a right click - the thing's card
            if (a.empty() || ev.GetParameter<int>("button", 0) != 1) return;
            const int at = a[0].Get<int>();
            if (at >= 0 && size_t(at) < st.rows.size()) infocardwin::showItem(st.rows[size_t(at)].info);
        });
        m.Bind("rows", &s.rowViews);
        m.Bind("title", &s.title);
        m.Bind("worth_word", &s.worthWord);
        m.Bind("worth", &s.worth);
        m.Bind("close", &s.close);
        m.Bind("empty", &s.empty);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("ms_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            closeShop(state());
        });
    });
    if (!s.live.made()) return;
    const ms::Texts t = texts(s);
    const auto money = [](long long z) { return charblock::grouped(z) + " z"; };
    std::vector<OwnWareView> rows;
    for (const ms::Row& r : s.rows) {
        OwnWareView v;
        v.name = r.refine > 0 ? "+" + std::to_string(r.refine) + " " + r.name : r.name;
        std::string left = t.left;
        if (const size_t p = left.find("{1}"); p != std::string::npos) left.replace(p, 3, std::to_string(r.left));
        v.left = left;
        v.price = money(r.price);
        v.inside = r.inside;
        v.colour = r.colour;
        const int sold = s.ledger.soldOf(r.index);
        v.sold = sold > 0 ? settings::fmt(say(settings::Text::MyShopSoldCount), {std::to_string(sold)}) : "";
        if (r.icon && r.icon->width > 0) v.icon = uaui::picture(r.icon, uaui::stamp(r.icon), s.iconPrev[r.icon]);
        rows.push_back(v);
    }
    const Rml::String worth = money(ms::worth(s.rows));
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    if (rows != s.rowViews) s.rowViews = std::move(rows), s.live.doc.dirty("rows");
    {  // 3.0.18-beta.143: the sales, the newest first (8 shown), the zeny earned
        std::vector<SaleView> sales;
        for (const ms::Sale& x : s.ledger.sales()) {
            if (sales.size() >= 8) break;
            sales.push_back(SaleView{x.time, std::to_string(x.amount) + " \xC3\x97 " + x.name, "+" + money(x.zeny)});
        }
        if (sales != s.saleViews) s.saleViews = std::move(sales), s.live.doc.dirty("sales");
        const Rml::String earned = money(s.ledger.earned());
        if (earned != s.earned || s.earnedWord.empty()) {
            s.earned = earned;
            s.earnedWord = say(settings::Text::MyShopEarned), s.salesWord = say(settings::Text::MyShopSales);
            s.noSales = say(settings::Text::MyShopNoSales), s.tip = say(settings::Text::ThingCardTip);
            s.live.doc.dirtyAll();
        }
    }
    if (t.title != s.title || t.worth != s.worthWord || worth != s.worth || t.close != s.close ||
        t.empty != s.empty || ground != s.ground) {
        s.title = t.title, s.worthWord = t.worth, s.worth = worth, s.close = t.close, s.empty = t.empty;
        s.ground = ground;
        s.live.doc.dirtyAll();
    }
    s.live.frame(s.shown, s.layout.x, s.layout.y, int(winstack::Win::MyShop), ".btn, .x");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    if (uaui::readyAnywhere()) {  // 3.0.18-beta.81: the live window in place of the sheet
        s.sheet.frame("my shop", false, true, 0, 0, 0, [](uisheet::Content&) {});
        s.sheetShown = false;
        liveFrame(s);
        return;
    }
    s.live.frame(false, 0, 0, 0, nullptr);
    s.sheetShown = s.sheet.frame("my_shop", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (s.live.shown) {  // 3.0.18-beta.81
        s.live.draw(s.shadow, record, renderer, s.scale, s.k);
        return;
    }
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                  12 * s.scale, s.scale, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (s.live.shown) {  // 3.0.18-beta.81: RmlUi's
        if (!s.live.mouse(game, m, wp, lp)) return false;
        result = 0;
        return true;
    }
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_MOUSEWHEEL) {
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1;
        s.show.scroll = std::clamp(s.show.scroll + by, 0, ms::mostScroll(s.layout, s.rows.size()));
        s.key.clear();
        return true;
    }
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && ms::hitTest(s.layout, lx, ly).part == ms::Part::Close)
        closeShop(s);
    return true;
}

bool shown() { return state().shown; }

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameShop(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    switch (c) {
        case PadCommand::Up: s.show.scroll = std::max(0, s.show.scroll - 1); break;
        case PadCommand::Down:
            s.show.scroll = std::min(ms::mostScroll(s.layout, s.rows.size()), s.show.scroll + 1);
            break;
        case PadCommand::Close: closeShop(s); break;
    }
    s.key.clear();
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    if (s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.81
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

void packet(const packets::Event& e) {  // 3.0.18-beta.143: a sale from the shop - its line in «Продажі»
    const auto* x = std::get_if<packets::ShopSold>(&e);
    if (!x) return;
    State& s = state();
    SYSTEMTIME t{};
    GetLocalTime(&t);
    char hm[8];
    std::snprintf(hm, sizeof hm, "%02u:%02u", unsigned(t.wHour), unsigned(t.wMinute));
    ms::Sale sale;
    if (s.ledger.sold(x->index, x->amount, hm, sale))
        LOG_INFO("my shop: sold %d x %s for %lld z (%lld z in all)", sale.amount, sale.name.c_str(), sale.zeny,
                 s.ledger.earned());
    else
        LOG_INFO("my shop: a sale of place %d (%d) not matched to a thing", x->index, x->amount);
}

}  // namespace uagfx::win::myshopwin
