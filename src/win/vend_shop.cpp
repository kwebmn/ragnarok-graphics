#include "win/vend_shop.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "core/item_window.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/vend_shop.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::vendshopwin {
namespace {

namespace vs = uagfx::vendshop;
namespace iw = uagfx::itemwin;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the shop's two windows (where the manager keeps them, their classes and SendMsgs; the shop's account at
// the things' window + 0xA0 - its message 0x1c), a window's shown flag; the purchase window's buttons (0x76 «Купити»:
// the game mode's 0x5c - the purchase sent - and both windows deleted; 0x77 both deleted) and its refresh (0x17)
constexpr uint32_t kWindowMgr = 0x727ED8, kThingsAt = kWindowMgr + 0x14C, kThingsClass = 0x69B510;
constexpr uint32_t kThingsSend = 0x4B1F50, kBuyAt = kWindowMgr + 0x154, kBuyClass = 0x69B640, kBuySend = 0x4B5170;
constexpr uint32_t kShopAccount = 0xA0, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr int kButtonMsg = 6, kBuyButton = 0x76, kCancelButton = 0x77, kRefresh = 0x17;
// The session: the shop's things (+0xF64, their count +0xF68) and the purchase (+0xF88, +0xF8C) - a node's ITEM_INFO
// at +8 (its type +0, its place +8, how many +0x10, the price +0x18, the item's number as words +0x30 / +0x34, known
// +0x3C, refined +0x40); the purchase's own add and take (thiscall: the session, an ITEM_INFO - matched by its
// number's words, its count added or taken) and their first bytes; the character's zeny
constexpr uint32_t kSession = 0x771148, kThings = kSession + 0xF64, kPurchase = kSession + 0xF88, kInfo = 8;
constexpr uint32_t kAdd = 0x629DA0, kTake = 0x629E90, kZeny = 0x772038;
constexpr uint8_t kAddBytes[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10, 0x53, 0x56, 0x57, 0x8B, 0xF9, 0x89};
constexpr uint8_t kTakeBytes[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10, 0x8B, 0x81, 0x88, 0x0F, 0x00, 0x00};
constexpr size_t kInfoSize = 0x48, kMostNodes = 200;
constexpr int kShiftMany = 10;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ListFn = void(__attribute__((thiscall)) *)(uint32_t session, const uint8_t* info);

struct Thing {
    vs::Row row;
    uint8_t info[kInfoSize] = {};  // its ITEM_INFO as the session has it (its words' pointer the session's)
    std::string digits;            // the item's number as the game keeps it (the purchase matched by it)
};

struct State {
    WindowShadow shadow;
    bool on = false, shown = false, hid = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::map<uint32_t, std::string> signs;  // the shops' signs by their merchants' accounts
    std::vector<Thing> things;
    std::vector<vs::Row> rows;
    vs::Show show;
    vs::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool padNow = false;
    int told = 0;
    int known = -1;  // the purchase's add and take: 1 this build's, 0 not
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

uint32_t gameWindow(uint32_t at, uint32_t cls, uint32_t send) {
    const uint32_t w = read32(at);
    return w && read32(w) == cls && read32(read32(w) + kSendMsgSlot) == send ? w : 0;
}

uint32_t thingsWindow() { return gameWindow(kThingsAt, kThingsClass, kThingsSend); }
uint32_t buyWindow() { return gameWindow(kBuyAt, kBuyClass, kBuySend); }

bool knownFunctions(State& s) {
    if (s.known >= 0) return s.known == 1;
    uint8_t a[sizeof kAddBytes] = {}, t[sizeof kTakeBytes] = {};
    s.known = readGame(kAdd, a, sizeof a) && std::memcmp(a, kAddBytes, sizeof a) == 0 &&
                      readGame(kTake, t, sizeof t) && std::memcmp(t, kTakeBytes, sizeof t) == 0
                  ? 1
                  : 0;
    if (!s.known) LOG_INFO("vending: the game's purchase list is not where this build has it - + and - do nothing");
    return s.known == 1;
}

void holdOne(uint32_t window, bool on, bool& hid) {
    if (!window) return;
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0)) hid = true;
    } else if (hid) {
        write32(window + kShown, 1);
    }
}

void hold(State& s, bool on) {
    const uint32_t a = thingsWindow(), b = buyWindow();
    if (!a && !b) {
        s.hid = false;
        return;
    }
    bool hid = s.hid;
    holdOne(a, on, hid);
    holdOne(b, on, hid);
    if (!on) hid = false;
    if (hid && !s.hid) LOG_INFO("vending: the game's shop windows hidden for «Лавка» (their shown flags 0)");
    s.hid = hid;
}

// A session list's things: their ITEM_INFOs and the item's number as words
std::vector<Thing> readList(const State& s, uint32_t headAt) {
    std::vector<Thing> out;
    const auto& names = clienttables::itemNames();
    const auto& unknown = clienttables::unknownNames();
    const auto& files = clienttables::itemFiles();
    const uint32_t head = read32(headAt);
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        Thing t;
        if (!readGame(node + kInfo, t.info, sizeof t.info)) break;
        const uint32_t ptr = rd32(t.info, 0x30), len = rd32(t.info, 0x34);
        if (!ptr || !len || len > 16) continue;
        char digits[17] = {};
        if (!readGame(ptr, digits, len)) continue;
        t.digits.assign(digits, len);
        vs::Row& r = t.row;
        r.index = int(rd32(t.info, 0x08));
        r.id = std::atoi(digits);
        r.stock = int(rd32(t.info, 0x10));
        r.price = int32_t(rd32(t.info, 0x18));
        const int refine = int(rd32(t.info, 0x40));
        r.refine = refine > 0 && refine <= 20 ? refine : 0;
        const bool identified = t.info[0x3C] != 0;
        const std::string id = std::to_string(r.id);
        std::string words;
        if (!identified) {
            const auto u = unknown.find(id);
            if (u != unknown.end()) words = u->second;
        }
        if (words.empty()) {
            const auto w = names.find(id);
            if (w != names.end()) words = w->second;
        }
        r.name = words.empty() ? "#" + id : utf8Of(widened(hotbar::spaced(words), s.codePage));
        const auto f = files.find(id);
        if (f != files.end()) r.icon = clienttables::icon(f->second, iw::iconSide(s.scale), 4 * s.scale);
        out.push_back(std::move(t));
    }
    return out;
}

// The shop's things, each with how many of it the purchase has (matched as the game matches: by the number's words)
void readShop(State& s) {
    s.things = readList(s, kThings);
    const std::vector<Thing> bought = readList(s, kPurchase);
    s.rows.clear();
    for (Thing& t : s.things) {
        t.row.chosen = 0;
        for (const Thing& b : bought)
            if (b.digits == t.digits) t.row.chosen += b.row.stock;  // a purchase node's count: how many chosen
        s.rows.push_back(t.row);
    }
}

vs::Texts texts(const State& s, uint32_t account) {
    vs::Texts t;
    const auto sign = s.signs.find(account);
    t.title = sign != s.signs.end() && !sign->second.empty() ? utf8Of(widened(sign->second, s.codePage))
                                                              : say(settings::Text::VendTitle);
    t.stock = say(settings::Text::VendStock);
    t.total = say(settings::Text::VendTotal);
    t.zeny = say(settings::Text::VendZeny);
    t.buy = say(settings::Text::VendBuy);
    t.cancel = say(settings::Text::MsgCancel);
    if (s.padNow) {
        std::string hint = say(settings::Text::VendPadHint);
        const char* const labels[3] = {uapad::buttonLabel(0), uapad::buttonLabel(2), uapad::buttonLabel(3)};
        const char* const marks[3] = {"{1}", "{2}", "{3}"};
        for (int i = 0; i < 3; ++i)
            if (const size_t at = hint.find(marks[i]); at != std::string::npos) hint.replace(at, 3, labels[i]);
        t.hint = hint;
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

void render(State& s, uint32_t account) {
    const uicost::Paint cost("«Лавка»", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    vs::paint(s.picture, s.layout, s.rows, s.show);
    drawWords(s.picture, gdiWords(vs::words(s.layout, s.rows, s.show, texts(s, account))));
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
    uapad::gamewindow::setBlocked(Blocker::Vending, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Vending, nullptr, nullptr, false);
    s.shown = false;
}

void refresh() {  // the purchase window's own list and total made again from the session's
    if (const uint32_t w = buyWindow()) reinterpret_cast<SendMsgFn>(uintptr_t(kBuySend))(w, 0, kRefresh, 0, 0, 0);
}

// One more of a shop's thing (or `many`, as far as the shop and the zeny go) into the purchase - the session's own add
void more(State& s, int row, int many) {
    if (row < 0 || row >= int(s.things.size()) || !knownFunctions(s)) return;
    const vs::Row& r = s.things[size_t(row)].row;
    const long long sum = vs::total(s.rows), zeny = int32_t(read32(kZeny));
    int n = 0;
    vs::Row probe = s.rows[size_t(row)];
    while (n < many && vs::canAdd(probe, sum + probe.price * n, zeny)) {
        ++n;
        ++probe.chosen;
    }
    if (!n) return;
    uint8_t info[kInfoSize];
    std::memcpy(info, s.things[size_t(row)].info, sizeof info);
    const uint32_t count = uint32_t(n);
    std::memcpy(info + 0x10, &count, 4);
    reinterpret_cast<ListFn>(uintptr_t(kAdd))(kSession, info);
    refresh();
    if (s.told++ < 40) LOG_INFO("vending: #%d +%d into the purchase (the session's own add)", r.id, n);
    readShop(s);
    s.key.clear();
}

void less(State& s, int row, int many) {
    if (row < 0 || row >= int(s.things.size()) || !knownFunctions(s)) return;
    const vs::Row& r = s.rows[size_t(row)];
    const int n = std::min(many, r.chosen);
    if (n <= 0) return;
    uint8_t info[kInfoSize];
    std::memcpy(info, s.things[size_t(row)].info, sizeof info);
    const uint32_t count = uint32_t(n);
    std::memcpy(info + 0x10, &count, 4);
    reinterpret_cast<ListFn>(uintptr_t(kTake))(kSession, info);
    refresh();
    if (s.told++ < 40) LOG_INFO("vending: #%d -%d out of the purchase (the session's own take)", r.id, n);
    readShop(s);
    s.key.clear();
}

void press(State& s, int id) {
    const uint32_t w = buyWindow();
    if (!w) return;
    if (s.told++ < 40)
        LOG_INFO("vending: %s - the game's purchase window's 0x%X (%d things, %lld zeny)",
                 id == kBuyButton ? "«Купити»" : "«Скасувати»", unsigned(id), int(s.rows.size()), vs::total(s.rows));
    reinterpret_cast<SendMsgFn>(uintptr_t(kBuySend))(w, 0, kButtonMsg, id, 0, 0);
    s.key.clear();
}

}  // namespace

void packet(const packets::Event& e) {
    const auto* sign = std::get_if<packets::ShopSign>(&e);
    if (!sign) return;
    State& s = state();
    if (sign->title.empty()) s.signs.erase(sign->account);
    else if (s.signs.size() < 500) s.signs[sign->account] = sign->title;
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("vending: [interface] vending = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    if (world) hold(s, on);
    const uint32_t things = world ? thingsWindow() : 0, buy = world ? buyWindow() : 0;
    if (!on || !things || !buy || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    const uint32_t account = read32(things + kShopAccount);
    if (!s.shown) {
        s.show = vs::Show{};
        LOG_INFO("vending: a shop open (its merchant's account %u) - «Лавка» shows it", unsigned(account));
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.padNow = uapad::runtime::look().shown;
    readShop(s);
    s.show.zeny = int32_t(read32(kZeny));
    s.layout = vs::layout(targetWidth, targetHeight, s.scale, s.rows.size());
    s.show.scroll = std::clamp(s.show.scroll, 0, vs::mostScroll(s.layout, s.rows.size()));
    if (s.padNow && !s.rows.empty()) {
        s.show.focus = std::clamp(s.show.focus, 0, int(s.rows.size()) - 1);
        if (s.show.focus < s.show.scroll) s.show.scroll = s.show.focus;
        if (s.show.focus >= s.show.scroll + int(s.layout.lines.size()))
            s.show.scroll = s.show.focus - int(s.layout.lines.size()) + 1;
    } else if (!s.padNow) {
        s.show.focus = -1;
    }
    s.show.hoverRow = -1;
    s.show.hoverPart = 0;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const vs::Hit h = vs::hitTest(s.layout, s.show.scroll, s.rows.size(), float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        s.show.hoverRow = h.row;
        s.show.hoverPart = int(h.part);
    }
    std::string key = std::to_string(account) + ';' + std::to_string(s.show.scroll) + ',' +
                      std::to_string(s.show.hoverRow) + ',' + std::to_string(s.show.hoverPart) + ',' +
                      std::to_string(s.show.focus) + ';' + std::to_string(s.show.zeny) + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + (s.padNow ? "P" : "") + '#' +
                      std::to_string(uistyle::generation());
    for (const vs::Row& r : s.rows)
        key += ';' + std::to_string(r.id) + 'x' + std::to_string(r.stock) + '/' + std::to_string(r.chosen) +
               (r.icon ? "i" : "");
    if (const auto sign = s.signs.find(account); sign != s.signs.end()) key += '|' + sign->second;
    if (key != s.key) {
        s.key = key;
        render(s, account);
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
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 14 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
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
        s.show.scroll = std::clamp(s.show.scroll + by, 0, vs::mostScroll(s.layout, s.rows.size()));
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const vs::Hit h = vs::hitTest(s.layout, s.show.scroll, s.rows.size(), lx, ly);
    const int many = (GetKeyState(VK_SHIFT) & 0x8000) ? kShiftMany : 1;  // Shift: ten at a time
    switch (h.part) {
        case vs::Part::Plus: more(s, h.row, many); break;
        case vs::Part::Minus: less(s, h.row, many); break;
        case vs::Part::Row:
            if (m == WM_LBUTTONDBLCLK) more(s, h.row, many);  // twice on a row: one more of it
            break;
        case vs::Part::Buy:
            if (vs::total(s.rows) > 0 && vs::total(s.rows) <= s.show.zeny) press(s, kBuyButton);
            break;
        case vs::Part::Cancel:
        case vs::Part::Close: press(s, kCancelButton); break;
        default: break;
    }
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    press(s, kCancelButton);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const int n = int(s.rows.size());
    switch (c) {
        case PadCommand::Up: s.show.focus = std::max(0, s.show.focus - 1); break;
        case PadCommand::Down: s.show.focus = std::min(std::max(0, n - 1), s.show.focus + 1); break;
        case PadCommand::More: more(s, s.show.focus, 1); break;
        case PadCommand::Less: less(s, s.show.focus, 1); break;
        case PadCommand::Buy:
            if (vs::total(s.rows) > 0 && vs::total(s.rows) <= s.show.zeny) press(s, kBuyButton);
            break;
        case PadCommand::Cancel: press(s, kCancelButton); break;
    }
    s.key.clear();
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

}  // namespace uagfx::win::vendshopwin
