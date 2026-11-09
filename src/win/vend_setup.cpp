#include "win/vend_setup.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "core/item_window.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/vend_setup.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

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

struct State {
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hiddenSale = 0, hiddenCart = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<vs::Row> cart, sale;
    std::vector<int> cartPlaces, salePlaces;  // each row's place (ITEM_INFO + 8)
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

void readList(const State& s, uint32_t listAt, std::vector<vs::Row>& rows, std::vector<int>& places) {
    rows.clear();
    places.clear();
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
        r.count = int(rd32(raw, 0x10));
        const int refine = int(rd32(raw, 0x40));
        r.refine = refine > 0 && refine <= 20 ? refine : 0;
        const auto f = files.find(id);
        if (f != files.end()) r.icon = clienttables::icon(f->second, iw::iconSide(s.scale) * 3 / 4, 4 * s.scale);
        rows.push_back(std::move(r));
        places.push_back(int(rd32(raw, 0x08)));
    }
}

void read(State& s, uint32_t window) {
    readList(s, kCartList, s.cart, s.cartPlaces);
    readList(s, kSaleList, s.sale, s.salePlaces);
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
    t.priceHint = say(settings::Text::VendSetupHint);
    if (s.pad) {  // 3.0.16-beta.39: the controller's buttons (A, B, X, Y, LB, RB as uaRO Pad's menu names them)
        const auto b = [](size_t i) { return std::string(uapad::buttonLabel(i)); };
        t.hint = b(0) + " " + say(settings::Text::VendSetupPadPut) + " \xC2\xB7 " + b(2) + " " +
                 say(settings::Text::VendSetupPadTake) + " \xC2\xB7 " + b(4) + "/" + b(5) + " " +
                 say(settings::Text::VendSetupPadPrice) + " \xC2\xB7 " + b(3) + " " + t.open + " \xC2\xB7 " + b(1) +
                 " " + t.cancel;
    }
    return t;
}

std::vector<Words> gdiWords(const std::vector<vs::Words>& in) {
    std::vector<Words> out;
    for (const vs::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == vs::Align::Right    ? DT_RIGHT
                                 : w.align == vs::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the shop's opening", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    vs::paint(s.picture, s.layout, s.cart, s.sale, s.show);
    drawWords(s.picture, gdiWords(vs::words(s.layout, s.cart, s.sale, s.sign, s.show, texts(s))));
    s.upload = true;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
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

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
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
    if (readGameCursor(cx, cy)) {
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
    if (key != s.key) {
        s.key = key;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
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

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, true);
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::vendsetupwin
