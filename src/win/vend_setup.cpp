#include "win/vend_setup.hpp"

#include <map>
#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/card_names.hpp"
#include "core/window_stack.hpp"
#include "core/hotbar_panel.hpp"
#include "core/item_window.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/vend_setup.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "win/client_tables.hpp"
#include "win/info_card.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/stash_watch.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::vendsetupwin {
namespace {

namespace vs = uagfx::vendsetup;
namespace iw = uagfx::itemwin;
using uapad::gamewindow::Blocker;

constexpr uint32_t kWindowMgr = 0x727ED8, kSaleAt = kWindowMgr + 0x148, kSaleClass = 0x69BA68, kSaleSend = 0x4BFE40;
constexpr uint32_t kCartAt = kWindowMgr + 0x144, kCartClass = 0x69B808, kCartSend = 0x4B8760;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510;
constexpr uint32_t kSignEdit = 0x70, kPriceEdits = 0x7C, kPlaces = 0xB0, kGetText = 0x443300;
constexpr uint32_t kSession = 0x771148, kCartList = kSession + 0xF70, kSaleList = kSession + 0xF94, kInfo = 8;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0, kDragInfo = 0x32C;
constexpr int kButtonMsg = 6, kOk = 0x76, kCancel = 0x77, kDropMsg = 0x26;
constexpr int kCartDrag = 0x5D, kSaleDrag = 0x5E, kEndDrag = 0x13, kCartKind = 0xB, kSaleKind = 10;
constexpr int kMostPlaces = 13;  // the price edits the window has (FUN_004bfe40's drop: 0 .. 0xC)
constexpr size_t kInfoSize = 0x48, kMostNodes = 100, kMostWords = 80;
constexpr ULONGLONG kBlinkMs = 530;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t edit);
using SetTextFn = void(__attribute__((thiscall)) *)(uint32_t edit, const char* text);
constexpr uint32_t kSetTextSlot = 0x98;  // an edit's SetText (win/ask_card)

// 3.0.18-beta.90 (live windows, group 3): setting up one's shop a live window - the cart and the sale side by side
// by flexbox, the sign's and prices' fields the game's
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw vstw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<span class="x ctl" data-event-click="vt_cancel()">&#x2715;</span>
<div class="lbl">{{sign_word}}</div>
<div class="field ctl" data-class-typing="focus == 0" data-event-click="vt_sign()">{{sign}}<span class="caret"
  data-if="focus == 0 && caret"></span></div>
<div class="cols"><div class="col"><div class="lbl">{{cart_word}}</div><div class="list">
<div class="ware ctl" data-for="r, i : cart" data-event-click="vt_cart(i)"
  data-event-mousedown="vt_card(0, i)"><img data-if="r.icon != ''"
  data-attr-src="r.icon"/><div class="wname"><span class="name" data-style-color="r.colour">{{r.name}}</span><span
  class="inside"
  data-if="r.inside != ''">{{r.inside}}</span><span
  class="stock">{{r.count}}</span><span class="mkt" data-if="r.market != ''">{{r.market}}</span></div></div>
<div class="hint" data-if="cart.size == 0">{{empty}}</div></div></div>
<div class="col"><div class="lbl">{{sale_word}}</div><div class="list">
<div class="ware ctl" data-for="r, i : sale" data-event-mousedown="vt_card(1, i)"><img data-if="r.icon != ''"
  data-attr-src="r.icon"/><div class="wname"><span class="name" data-style-color="r.colour">{{r.name}}</span><span
  class="inside"
  data-if="r.inside != ''">{{r.inside}}</span><span class="stock">{{r.count}}</span><span class="mkt"
  data-if="r.market != ''">{{r.market}}</span></div><div class="field price ctl"
  data-class-typing="focus == i + 1" data-event-click="vt_price(i)">{{r.price}}<span class="caret"
  data-if="focus == i + 1 && caret"></span></div><span class="pm ctl"
  data-event-click="vt_remove(i)">&#x2715;</span></div>
</div><div class="hint">{{price_hint}}</div><div class="all" data-if="all != ''">{{all}}</div></div></div>
<div class="hint" data-if="hint != ''">{{hint}}</div>
<div class="btns"><div class="btn ctl" data-event-click="vt_cancel()">{{cancel}}</div>
<div class="btn main ctl" data-event-click="vt_open()">{{open}}</div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
struct SetupWareView {
    Rml::String name, count, price, icon;
    Rml::String market;  // 3.0.18-beta.104: the site's market line ("" none)
    Rml::String inside;  // 3.0.18-beta.142: its cards and free slots
    Rml::String colour = "#eef2f8";  // 3.0.18-beta.144: its name's
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<SetupWareView> cartViews, saleViews;
    std::map<const panel::Picture*, std::string> iconPrev;
    Rml::String title, signWord, signText, cartWord, saleWord, open, cancel, empty, priceHint, hint;
    Rml::String ground = "#10141cf0";
    int focusNo = -1;
    bool caretOn = false;
    uisheet::Sheet sheet;  // 3.0.18-beta.30: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hiddenSale = 0, hiddenCart = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<vs::Row> cart, sale;
    std::vector<int> cartPlaces, salePlaces;  // each row's place (ITEM_INFO + 8)
    std::vector<uint32_t> cartInfos, saleInfos;  // 3.0.18-beta.142: each row's ITEM_INFO in the game (its card)
    Rml::String allWord;                         // «Якщо продати все: N z»
    std::string sign;
    vs::Show show;
    vs::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    bool pad = false;          // 3.0.16-beta.39: the controller's row shown (a mouse press hides it)
    int padColumn = 0, padAt = 0;  // 0 the cart's, 1 the sale's; the row in its list
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

uint32_t windowAt(uint32_t at, uint32_t cls, uint32_t send) {
    const uint32_t w = read32(at);
    return w && read32(w) == cls && read32(read32(w) + kSendMsgSlot) == send ? w : 0;
}
uint32_t gameSale() { return windowAt(kSaleAt, kSaleClass, kSaleSend); }
uint32_t gameCart() { return windowAt(kCartAt, kCartClass, kCartSend); }

void holdOne(uint32_t& hidden, uint32_t window, bool on, const char* what) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && hidden != window) {
            LOG_INFO("vend setup: the game's %s window hidden for the new card (its shown flag 0)", what);
            hidden = window;
        }
    } else if (window && hidden == window) {
        write32(window + kShown, 1);
        hidden = 0;
    } else if (!window) {
        hidden = 0;
    }
}

void hold(State& s, bool on) {
    holdOne(s.hiddenSale, gameSale(), on, "shop's");
    holdOne(s.hiddenCart, gameCart(), on, "cart's things");
}

bool modeMsg(int msg, int a, int b, int c) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, c);
    return true;
}

std::string wordsOf(uint32_t edit, unsigned codePage) {
    if (!edit) return "";
    const char* p = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
    char buf[kMostWords + 1] = {};
    if (!p || !readGame(uint32_t(uintptr_t(p)), buf, kMostWords)) return "";
    return utf8Of(widened(buf, codePage));
}

uint32_t priceEdit(uint32_t window, int place) {
    return window && place >= 0 && place < kMostPlaces ? read32(window + kPriceEdits + 4 * uint32_t(place)) : 0;
}

void readList(const State& s, uint32_t listAt, std::vector<vs::Row>& rows, std::vector<int>& places,
              std::vector<uint32_t>& infos) {
    rows.clear();
    places.clear();
    infos.clear();
    const auto& names = clienttables::itemNames();
    const auto& unknown = clienttables::unknownNames();
    const auto& files = clienttables::itemFiles();
    const uint32_t head = read32(listAt);
    if (!head) return;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kInfoSize] = {};
        if (!readGame(node + kInfo, raw, sizeof raw)) break;
        const uint32_t ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
        char digits[17] = {};
        if (!ptr || !len || len > 16 || !readGame(ptr, digits, len)) continue;
        vs::Row r;
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
        r.count = int(rd32(raw, 0x10));
        r.id = std::atoi(id.c_str());
        const int refine = int(rd32(raw, 0x40));
        r.refine = refine > 0 && refine <= 20 ? refine : 0;
        const auto f = files.find(id);
        if (f != files.end()) r.icon = clienttables::icon(f->second, iw::iconSide(s.scale) * 3 / 4, 4 * s.scale);
        infocardwin::describe(raw, r.id, s.codePage, r.name, r.inside);  // 3.0.18-beta.142: [slots], what is inside
        if (const std::string e = infocardwin::elementHex(r.id, raw); !e.empty()) r.colour = e;  // .144
        rows.push_back(std::move(r));
        places.push_back(int(rd32(raw, 0x08)));
        infos.push_back(node + kInfo);
    }
}

void read(State& s, uint32_t window) {
    readList(s, kCartList, s.cart, s.cartPlaces, s.cartInfos);
    readList(s, kSaleList, s.sale, s.salePlaces, s.saleInfos);
    for (size_t i = 0; i < s.sale.size(); ++i) s.sale[i].price = wordsOf(priceEdit(window, int(i)), s.codePage);
    s.sign = wordsOf(window ? read32(window + kSignEdit) : 0, s.codePage);
    s.show.most = std::clamp(int32_t(read32(window + kPlaces)), 0, kMostPlaces);
    // the keys' edit: ours when it is the sign's or a price's
    const uint32_t focus = read32(kFocus);
    s.show.focus = -1;
    if (focus && focus == read32(window + kSignEdit)) s.show.focus = 0;
    for (int i = 0; i < kMostPlaces && s.show.focus < 0; ++i)
        if (focus && focus == priceEdit(window, i)) s.show.focus = 1 + i;
}

vs::Texts texts(const State& s) {
    vs::Texts t;
    t.title = say(settings::Text::VendSetupTitle);
    t.sign = say(settings::Text::VendSetupSign);
    t.signHint = say(settings::Text::VendSetupSignHint);
    t.cart = say(settings::Text::VendSetupCart);
    t.sale = say(settings::Text::VendSetupSale);
    t.open = say(settings::Text::VendSetupOpen);
    t.cancel = say(settings::Text::VendSetupCancel);
    t.empty = say(settings::Text::VendSetupEmpty);
    t.priceHint = say(settings::Text::VendSetupHint) + " \xC2\xB7 " + say(settings::Text::ThingCardTip);  // .142
    if (s.pad) {  // 3.0.16-beta.39: the controller's buttons (A, B, X, Y, LB, RB as uaRO Pad's menu names them)
        const auto b = [](size_t i) { return std::string(uapad::buttonLabel(i)); };
        t.hint = b(0) + " " + say(settings::Text::VendSetupPadPut) + " \xC2\xB7 " + b(2) + " " +
                 say(settings::Text::VendSetupPadTake) + " \xC2\xB7 " + b(4) + "/" + b(5) + " " +
                 say(settings::Text::VendSetupPadPrice) + " \xC2\xB7 " + b(3) + " " + t.open + " \xC2\xB7 " + b(1) +
                 " " + t.cancel;
    }
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
    uapad::gamewindow::setBlocked(Blocker::VendSetup, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::VendSetup, nullptr, nullptr, false);
    s.shown = false;
}

void button(State& s, int which) {
    const uint32_t window = gameSale();
    if (!window) return;
    if (s.told++ < 30)
        LOG_INFO("vend setup: %s - the window's 0x%X", which == kOk ? "«Відкрити лавку» (the game checks the prices)"
                                                                     : "«Скасувати»", unsigned(which));
    reinterpret_cast<SendMsgFn>(uintptr_t(kSaleSend))(window, 0, kButtonMsg, which, 0, 0);
    s.key.clear();
}

// A thing moved as the game's own drag does it: its drag begun (the mode's 0x5D / 0x5E), let go on the other window
// (its 0x26: it ends the drag)
void move(State& s, bool toSale, int place) {
    const uint32_t target = toSale ? gameSale() : gameCart();
    const uint32_t mode = read32(kModeNow);
    if (!target || !mode) return;
    if (!modeMsg(toSale ? kCartDrag : kSaleDrag, place, 0, 0)) return;
    const int kind = int32_t(read32(mode + kDragInfo));
    if (kind != (toSale ? kCartKind : kSaleKind)) {
        modeMsg(kEndDrag, 0, 0, 0);
        if (s.told++ < 30) LOG_INFO("vend setup: the thing %d not moved - the game's drag kind %d", place, kind);
        return;
    }
    reinterpret_cast<SendMsgFn>(uintptr_t(toSale ? kSaleSend : kCartSend))(target, 0, kDropMsg, 0, 0, 0);
    if (s.told++ < 30)
        LOG_INFO("vend setup: the thing %d %s (the game's own drop)", place, toSale ? "put up for sale" : "taken back");
    s.key.clear();
}

void focus(State& s, uint32_t edit, const char* what) {
    if (!edit) return;
    reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
    if (s.told++ < 30) LOG_INFO("vend setup: the keys to %s (the game's own field)", what);
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
        LOG_INFO("vend setup: [interface] vend_setup = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameSale() : 0;
    if (world) hold(s, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = vs::Show{};
        LOG_INFO("vend setup: the game's shop opening - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    read(s, window);
    s.layout = vs::layout(targetWidth, targetHeight, s.scale, s.pad);
    s.show.cartScroll = std::clamp(s.show.cartScroll, 0, vs::mostScroll(s.layout, false, s.cart.size()));
    s.show.saleScroll = std::clamp(s.show.saleScroll, 0, vs::mostScroll(s.layout, true, s.sale.size()));
    s.show.hoverCart = s.show.hoverSale = s.show.hoverRemove = s.show.hoverButton = -1;
    s.show.padCart = s.show.padSale = -1;
    if (s.pad) {  // 3.0.16-beta.39: the controller's row kept in its list and in sight
        const bool sale = s.padColumn == 1;
        const int count = int(sale ? s.sale.size() : s.cart.size());
        s.padAt = std::clamp(s.padAt, 0, std::max(0, count - 1));
        const int rows = int(sale ? s.layout.saleRows.size() : s.layout.cartRows.size());
        int& scroll = sale ? s.show.saleScroll : s.show.cartScroll;
        if (rows > 0 && count > 0) {
            if (s.padAt < scroll) scroll = s.padAt;
            if (s.padAt >= scroll + rows) scroll = s.padAt - rows + 1;
            (sale ? s.show.padSale : s.show.padCart) = s.padAt - scroll;
        }
    }
    s.show.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const vs::Hit h = vs::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == vs::Part::Cart) s.show.hoverCart = h.index;
        if (h.part == vs::Part::Sale || h.part == vs::Part::Price) s.show.hoverSale = h.index;
        if (h.part == vs::Part::Remove) s.show.hoverRemove = h.index;
        if (h.part == vs::Part::Open) s.show.hoverButton = 0;
        if (h.part == vs::Part::Cancel) s.show.hoverButton = 1;
        if (h.part == vs::Part::Cross) s.show.hoverButton = 2;
    }
    std::string key = s.sign + ';' + std::to_string(s.show.cartScroll) + ',' + std::to_string(s.show.saleScroll) +
                      ',' + std::to_string(s.show.hoverCart) + ',' + std::to_string(s.show.hoverSale) + ',' +
                      std::to_string(s.show.hoverRemove) + ',' + std::to_string(s.show.hoverButton) + ',' +
                      std::to_string(s.show.focus) + (s.show.focus >= 0 && s.show.caret ? "|" : "") + ',' +
                      std::to_string(s.show.most) + ',' + std::to_string(s.show.padCart) + ',' +
                      std::to_string(s.show.padSale) + ';' + std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + '#' + std::to_string(uistyle::generation());
    for (const vs::Row& r : s.cart) key += ";c" + r.name + ',' + std::to_string(r.count) + (r.icon ? "i" : "");
    for (const vs::Row& r : s.sale)
        key += ";s" + r.name + ',' + std::to_string(r.count) + ',' + r.price + (r.icon ? "i" : "");
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.30
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.30 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the shop's opening in
// RmlUi's markup, line by line as core vendsetup paints it - its glass, ✕, the sign's field, the lists' gold head
// lines and grounds, the cart's rows (the controller's frame), the sale's rows with their price fields (red without a
// price) and ✕, «Скасувати», «Відкрити»; the words where core vendsetup places them
static void fillSheet(State& s, uisheet::Content& c) {
    const vs::Layout& L = s.layout;
    const vs::Show& sh = s.show;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653, kDanger = 0xE0675C;
    auto& u = c.under;
    const auto icon = [&](const panel::Picture* p, const panel::Box& b) {
        if (p && p->width > 0)
            c.icons.push_back(uisheet::Icon{std::round(b.x + (b.w - float(p->width)) / 2),
                                            std::round(b.y + (b.h - float(p->height)) / 2), float(p->width),
                                            float(p->height), 1, s.sheet.icon(p)});
    };
    c.w = float(L.w), c.h = float(L.h);
    c.glass = uisheet::Box{L.glass.x, L.glass.y, L.glass.w, L.glass.h, 12 * sc, 1.0f * sc, 1, "glass"};
    c.ground = uisheet::Sheet::ground(kCard, uistyle::window());
    c.rect(u, L.cross, 6 * sc, kWhite, sh.hoverButton == 2 ? 0.14f : 0.06f);
    c.rect(u, L.sign, 7 * sc, kBlack, 0.30f, sh.focus == 0 ? kGold : kWhite, sh.focus == 0 ? 0.9f : 0.18f, 1.0f * sc);
    for (const panel::Box* head : {&L.cartHead, &L.saleHead})
        c.rect(u, panel::Box{head->x, head->y + head->h - 1 * sc, head->w, 1 * sc}, 0, kGold, 0.45f);
    c.rect(u, L.cartList, 8 * sc, kBlack, 0.14f);
    c.rect(u, L.saleList, 8 * sc, kBlack, 0.14f);
    for (size_t i = 0; i < L.cartRows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.cartScroll));
        if (at >= s.cart.size()) break;
        const bool full = sh.most > 0 && int(s.sale.size()) >= sh.most;
        c.rect(u, L.cartRows[i], 7 * sc, kWhite, sh.hoverCart == int(i) && !full ? 0.10f : 0.035f);
        if (sh.padCart == int(i)) c.rect(u, L.cartRows[i], 7 * sc, kWhite, 0.0f, kGold, 0.9f, 1.5f * sc);
        const panel::Box b = vs::iconOf(L.cartRows[i], sc);
        c.rect(u, b, 6 * sc, kBlack, 0.30f);
        icon(s.cart[at].icon, b);
    }
    for (size_t i = 0; i < L.saleRows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.saleScroll));
        if (at >= s.sale.size()) break;
        c.rect(u, L.saleRows[i], 7 * sc, kWhite, sh.hoverSale == int(i) ? 0.07f : 0.035f);
        if (sh.padSale == int(i)) c.rect(u, L.saleRows[i], 7 * sc, kWhite, 0.0f, kGold, 0.9f, 1.5f * sc);
        const panel::Box b = vs::iconOf(L.saleRows[i], sc);
        c.rect(u, b, 6 * sc, kBlack, 0.30f);
        icon(s.sale[at].icon, b);
        const bool focused = sh.focus == 1 + int(at);
        const bool none = vs::priceOf(s.sale[at].price) <= 0;
        c.rect(u, L.prices[i], 6 * sc, kBlack, 0.35f, focused ? kGold : none ? kDanger : kWhite,
               focused ? 0.9f : none ? 0.55f : 0.18f, 1.0f * sc);
        c.rect(u, L.removes[i], 6 * sc, kDanger, sh.hoverRemove == int(i) ? 0.30f : 0.10f);
    }
    c.rect(u, L.cancel, 8 * sc, kWhite, sh.hoverButton == 1 ? 0.14f : 0.07f);
    c.rect(u, L.open, 8 * sc, kGold, sh.hoverButton == 0 ? 0.40f : 0.26f, kGold, 0.85f, 1.0f * sc);
    c.say(vs::words(L, s.cart, s.sale, s.sign, sh, texts(s)));
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("vend setup", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<SetupWareView>()) {
            v.RegisterMember("name", &SetupWareView::name);
            v.RegisterMember("count", &SetupWareView::count);
            v.RegisterMember("price", &SetupWareView::price);
            v.RegisterMember("icon", &SetupWareView::icon);
            v.RegisterMember("market", &SetupWareView::market);
            v.RegisterMember("inside", &SetupWareView::inside);
            v.RegisterMember("colour", &SetupWareView::colour);
        }
        m.RegisterArray<std::vector<SetupWareView>>();
        m.Bind("cart", &s.cartViews);
        m.Bind("sale", &s.saleViews);
        m.Bind("title", &s.title);
        m.Bind("sign_word", &s.signWord);
        m.Bind("sign", &s.signText);
        m.Bind("cart_word", &s.cartWord);
        m.Bind("sale_word", &s.saleWord);
        m.Bind("open", &s.open);
        m.Bind("cancel", &s.cancel);
        m.Bind("empty", &s.empty);
        m.Bind("price_hint", &s.priceHint);
        m.Bind("all", &s.allWord);
        m.BindEventCallback("vt_card", [](Rml::DataModelHandle, Rml::Event& ev, const Rml::VariantList& a) {
            State& st = state();  // 3.0.18-beta.142: a right click - the thing's card
            if (a.size() < 2 || ev.GetParameter<int>("button", 0) != 1) return;
            const std::vector<uint32_t>& infos = a[0].Get<int>() ? st.saleInfos : st.cartInfos;
            const int at = a[1].Get<int>();
            if (at >= 0 && size_t(at) < infos.size()) infocardwin::showItem(infos[size_t(at)]);
        });
        m.Bind("hint", &s.hint);
        m.Bind("focus", &s.focusNo);
        m.Bind("caret", &s.caretOn);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("vt_open", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            button(state(), kOk);
        });
        m.BindEventCallback("vt_cancel", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            button(state(), kCancel);
        });
        m.BindEventCallback("vt_sign", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            State& st = state();
            const uint32_t window = gameSale();
            focus(st, window ? read32(window + kSignEdit) : 0, "the sign");
        });
        m.BindEventCallback("vt_cart", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            const int at = a.empty() ? -1 : a[0].Get<int>();
            if (at >= 0 && size_t(at) < st.cartPlaces.size() && int(st.sale.size()) < st.show.most)
                move(st, true, st.cartPlaces[size_t(at)]);
        });
        m.BindEventCallback("vt_price", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            const int at = a.empty() ? -1 : a[0].Get<int>();
            if (at >= 0 && size_t(at) < st.sale.size()) focus(st, priceEdit(gameSale(), at), "a price");
        });
        m.BindEventCallback("vt_remove", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            const int at = a.empty() ? -1 : a[0].Get<int>();
            if (at >= 0 && size_t(at) < st.salePlaces.size()) move(st, false, st.salePlaces[size_t(at)]);
        });
    });
    if (!s.live.made()) return;
    vs::Texts t = texts(s);
    // 3.0.18-beta.104 (R-K5QG6Z: «For sale {1} / {2}» shown as it is): the places used and the most
    t.sale = settings::fmt(t.sale, {std::to_string(s.sale.size()), std::to_string(s.show.most)});
    const auto viewOf = [&](const vs::Row& r, bool sale) {
        SetupWareView v;
        v.name = r.refine > 0 ? "+" + std::to_string(r.refine) + " " + r.name : r.name;
        v.count = "\xC3\x97" + std::to_string(r.count);
        v.inside = r.inside;
        v.colour = r.colour;
        // 3.0.18-beta.104 (R-K5QG6Z «окно с ценой маленькое»): the price grouped, the stack's sum beside the count, the
        // site's market under it
        const long long price = vs::priceOf(r.price);
        v.price = sale ? iw::grouped(price) + " z" : "";
        if (sale && price > 0 && r.count > 1)
            v.count += " \xC2\xB7 " + settings::fmt(say(settings::Text::VendSetupTotal),
                                                    {iw::grouped(price * r.count)});
        std::vector<iw::Item> one(1);
        one[0].id = r.id;
        stashwatch::prices(one);
        v.market = one[0].market;
        if (r.icon && r.icon->width > 0) v.icon = uaui::picture(r.icon, uaui::stamp(r.icon), s.iconPrev[r.icon]);
        return v;
    };
    {  // 3.0.18-beta.142: what the whole sale brings at its prices
        long long all = 0;
        for (const vs::Row& r : s.sale) all += vs::priceOf(r.price) * std::max(1, r.count);
        const Rml::String word = all > 0 ? settings::fmt(say(settings::Text::VendSetupAll), {iw::grouped(all)}) : "";
        if (word != s.allWord) s.allWord = word, s.live.doc.dirty("all");
    }
    std::vector<SetupWareView> cart, sale;
    for (const vs::Row& r : s.cart) cart.push_back(viewOf(r, false));
    for (const vs::Row& r : s.sale) sale.push_back(viewOf(r, true));
    const auto same = [](const std::vector<SetupWareView>& a, const std::vector<SetupWareView>& b) {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const auto& x, const auto& y) {
                   return x.name == y.name && x.count == y.count && x.price == y.price && x.icon == y.icon &&
                          x.market == y.market && x.inside == y.inside && x.colour == y.colour;
               });
    };
    if (!same(cart, s.cartViews)) s.cartViews = std::move(cart), s.live.doc.dirty("cart");
    if (!same(sale, s.saleViews)) s.saleViews = std::move(sale), s.live.doc.dirty("sale");
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    if (t.title != s.title || t.sign != s.signWord || s.sign != s.signText || t.cart != s.cartWord ||
        t.sale != s.saleWord || t.open != s.open || t.cancel != s.cancel || t.empty != s.empty ||
        t.priceHint != s.priceHint || t.hint != s.hint || s.show.focus != s.focusNo || s.show.caret != s.caretOn ||
        ground != s.ground) {
        s.title = t.title, s.signWord = t.sign, s.signText = s.sign, s.cartWord = t.cart, s.saleWord = t.sale;
        s.open = t.open, s.cancel = t.cancel, s.empty = t.empty, s.priceHint = t.priceHint, s.hint = t.hint;
        s.focusNo = s.show.focus, s.caretOn = s.show.caret, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    s.live.frame(s.shown, s.layout.x, s.layout.y, int(winstack::Win::VendSetup), ".ware, .field, .pm, .btn, .x");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    if (uaui::readyAnywhere()) {  // 3.0.18-beta.81: the live window in place of the sheet
        s.sheet.frame("vend setup", false, true, 0, 0, 0, [](uisheet::Content&) {});
        s.sheetShown = false;
        liveFrame(s);
        return;
    }
    s.live.frame(false, 0, 0, 0, nullptr);
    s.sheetShown = s.sheet.frame("vend_setup", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
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
    const bool pressed = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK ||
                         m == WM_RBUTTONDOWN || m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK ||
                         m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!pressed && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_MOUSEWHEEL) {
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1;
        const bool onSale = lx >= s.layout.saleList.x;
        int& scroll = onSale ? s.show.saleScroll : s.show.cartScroll;
        scroll = std::clamp(scroll + by, 0, vs::mostScroll(s.layout, onSale, onSale ? s.sale.size() : s.cart.size()));
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    s.pad = false;  // 3.0.16-beta.39
    const vs::Hit h = vs::hitTest(s.layout, lx, ly);
    const uint32_t window = gameSale();
    switch (h.part) {
        case vs::Part::Open: button(s, kOk); break;
        case vs::Part::Cancel:
        case vs::Part::Cross: button(s, kCancel); break;
        case vs::Part::Sign: focus(s, window ? read32(window + kSignEdit) : 0, "the sign"); break;
        case vs::Part::Cart: {
            const int at = h.index + s.show.cartScroll;
            if (at >= 0 && size_t(at) < s.cartPlaces.size() && int(s.sale.size()) < s.show.most)
                move(s, true, s.cartPlaces[size_t(at)]);
            break;
        }
        case vs::Part::Sale:
        case vs::Part::Price: {
            const int at = h.index + s.show.saleScroll;
            if (at >= 0 && size_t(at) < s.sale.size()) focus(s, priceEdit(window, at), "a price");
            break;
        }
        case vs::Part::Remove: {
            const int at = h.index + s.show.saleScroll;
            if (at >= 0 && size_t(at) < s.salePlaces.size()) move(s, false, s.salePlaces[size_t(at)]);
            break;
        }
        default: break;
    }
    return true;
}

bool shown() { return state().shown; }

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    if (!s.pad) {  // the first press only shows where it is
        s.pad = true;
        s.padColumn = s.cart.empty() && !s.sale.empty() ? 1 : 0;
        s.padAt = 0;
        s.key.clear();
        if (c == PadCommand::Up || c == PadCommand::Down || c == PadCommand::Left || c == PadCommand::Right) return;
    }
    const bool sale = s.padColumn == 1;
    const int count = int(sale ? s.sale.size() : s.cart.size());
    const uint32_t window = gameSale();
    switch (c) {
        case PadCommand::Up: s.padAt = std::max(0, s.padAt - 1); break;
        case PadCommand::Down: s.padAt = std::min(std::max(0, count - 1), s.padAt + 1); break;
        case PadCommand::Left: s.padColumn = 0; break;
        case PadCommand::Right: s.padColumn = 1; break;
        case PadCommand::Put:  // the cart's thing up for sale; on a sale's row its price field gets the keys
            if (!sale && s.padAt < int(s.cartPlaces.size()) && int(s.sale.size()) < s.show.most)
                move(s, true, s.cartPlaces[size_t(s.padAt)]);
            if (sale && s.padAt < int(s.sale.size())) focus(s, priceEdit(window, s.padAt), "a price");
            break;
        case PadCommand::Take:
            if (sale && s.padAt < int(s.salePlaces.size())) move(s, false, s.salePlaces[size_t(s.padAt)]);
            break;
        case PadCommand::Less:
        case PadCommand::More: {
            if (!sale || s.padAt >= int(s.sale.size())) break;
            const uint32_t edit = priceEdit(window, s.padAt);
            if (!edit) break;
            const std::string digits = vs::stepPrice(s.sale[size_t(s.padAt)].price, c == PadCommand::More ? 1 : -1);
            reinterpret_cast<SetTextFn>(uintptr_t(read32(read32(edit) + kSetTextSlot)))(edit, digits.c_str());
            if (s.told++ < 30) LOG_INFO("vend setup: the pad set a price to %s", digits.c_str());
            break;
        }
        case PadCommand::Open: button(s, kOk); break;
        case PadCommand::Cancel: button(s, kCancel); break;
    }
    s.key.clear();
}

bool close() {
    State& s = state();
    if (!s.shown) return false;
    button(s, kCancel);
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (!s.shown || s.show.focus < 0) return false;
    const bool tab = wp == VK_TAB, enter = wp == VK_RETURN;
    if (m == WM_CHAR) return wp == '\t' || wp == '\r';  // the field's Tab and Enter ours (not typed into it)
    if (m != WM_KEYDOWN || (!tab && !enter)) return false;
    const uint32_t window = gameSale();
    const int fields = 1 + int(s.sale.size());  // the sign, then a price a thing for sale
    const bool back = tab && GetKeyState(VK_SHIFT) < 0;
    int next = s.show.focus + (back ? -1 : 1);
    if (enter && s.show.focus == 0 && s.sale.empty()) return true;  // nothing to price yet
    if (next >= fields) next = enter ? fields - 1 : 0;              // Enter stops at the last price, Tab goes round
    if (next < 0) next = fields - 1;
    if (next == s.show.focus) return true;
    if (next == 0) focus(s, read32(window + kSignEdit), "the shop's name (Tab)");
    else focus(s, priceEdit(window, next - 1), enter ? "the next price (Enter)" : "the next price (Tab)");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, true);
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

}  // namespace uagfx::win::vendsetupwin
