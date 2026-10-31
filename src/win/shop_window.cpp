#include "win/shop_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/char_status.hpp"
#include "core/hotbar_panel.hpp"
#include "core/layout.hpp"
#include "core/reference_index.hpp"
#include "core/settings/strings.hpp"
#include "core/shop_window.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/chat_window.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win::shopwin {
namespace {

namespace sw = uagfx::shopwin;

// uaRO.exe's deal windows in the windows' manager: the choice of buying or selling (UI window 0x19: its SendMsg
// FUN_0047e430 - message 6 with 0x9E buy, 0x9D sell, 0x77 cancel; the NPC at +0x60), the shop's list (0x16), the buy
// cart (0x17: FUN_004b35d0 - 6 / 0x76 its OK, sent as an NPC's shop's while its [0x28] is 0; 0x77 its cancel) and the
// sell cart (0x18: FUN_004b6b70, the same buttons); a window's shown flag +0x38, its SendMsg the vtable's +0x94
constexpr uint32_t kWindowMgr = 0x727ED8;
constexpr uint32_t kListAt = kWindowMgr + 0xEC, kBuyAt = kWindowMgr + 0xF0, kSellAt = kWindowMgr + 0xF4;
constexpr uint32_t kChooseAt = kWindowMgr + 0xF8;
constexpr uint32_t kListClass = 0x69B3E0, kBuyClass = 0x69B5A8, kSellClass = 0x69B6D8, kChooseClass = 0x699410;
constexpr uint32_t kBuySendMsg = 0x4B35D0, kSellSendMsg = 0x4B6B70, kChooseSendMsg = 0x47E430;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kCartKind = 0xA0, kChooseNpc = 0x60;
constexpr int kButtonMsg = 6, kOk = 0x76, kCancel = 0x77, kChooseSell = 0x9D, kChooseBuy = 0x9E;
// The session's deal list (a list's head; a node's ITEM_INFO at +8, 72 bytes: +0x08 the bag's place, +0x10 the count,
// +0x18 the price after Discount / Overcharge, +0x30 / +0x34 its name - the item's number as text, +0x3C identified,
// +0x40 the refine), the purchase and sale lists' counts, and the functions putting an ITEM_INFO into those lists
// (thiscall, the session in ECX: FUN_00629750 by name, FUN_0062a4b0 by the bag's place; its +0x10 the amount)
constexpr uint32_t kSession = 0x771148, kDeals = kSession + 0xF28, kPurchaseCount = kSession + 0xF5C;
constexpr uint32_t kSaleCount = kSession + 0xF38, kInfo = 8, kCount = 0x10;
constexpr size_t kInfoSize = 0x48, kMostNodes = 512;
constexpr uint32_t kAddPurchase = 0x629750, kAddSale = 0x62A4B0;
const uint8_t kAddPurchaseCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10, 0x53, 0x56, 0x57, 0x8B, 0xF9,
                                    0x89, 0x7D, 0xF0, 0x8B, 0x87, 0x58, 0x0F, 0x00, 0x00};
const uint8_t kAddSaleCode[] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0x48, 0x24, 0x69, 0x00};
// The game mode's SendMsg: 0x28 sell, 0x29 buy from the NPC (its a) - the packet 0x00c5
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kAskSell = 0x28, kAskBuy = 0x29;
constexpr ULONGLONG kReadMs = 200, kWaitMs = 3000;
constexpr size_t kMostTyped = 24;
// 3.0.11-beta.101 (the owner: «и чтобы описание вещи можно было посмотреть»): the game's item card (UI window 0xC at
// the manager's +0xC8, made by MakeWindow, its message 0x18 an ITEM_INFO - as its own shop shows one on a right
// click; gone with the deal's end, the game mode's 0x2B), moved beside the shop (a window's vtable +0x18: x, y; its
// place +0x14 / +0x18, its size +0x1C / +0x20) - the driver's parts are drawn over the game's windows
constexpr uint32_t kCardAt = kWindowMgr + 0xC8, kMakeWindow = 0x4D42D0, kDeleteWindow = 0x4D1E60;
constexpr uint32_t kMoveSlot = 0x18, kRect = 0x14;
constexpr int kItemCard = 0xC, kShowItem = 0x18;

using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using AddFn = void(__attribute__((thiscall)) *)(uint32_t session, uint32_t info);
using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
using DeleteFn = void(__attribute__((thiscall)) *)(uint32_t manager, int id);
using MoveFn = void(__attribute__((thiscall)) *)(uint32_t window, int x, int y);

struct Deal {  // the game's deal windows now (0: none)
    uint32_t choose = 0, list = 0, buy = 0, sell = 0;
};

struct State {
    WindowShadow shadow;
    WindowEdge edge;  // 3.0.11-beta.108: the top of two or more windows (win/window_stack)
    bool on = false, shown = false, hid = false, stateRead = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    float fx = 0.5f, fy = 0.3f;
    unsigned codePage = 0;
    sw::Data data;
    sw::Show show;
    sw::Layout layout;
    std::vector<int> shownList;
    std::vector<uint32_t> infos;  // each ware's ITEM_INFO in the game's deal list
    std::string listKey;          // the list's wares (a new list empties the cart)
    uint32_t npc = 0, chosen = 0;
    std::map<uint32_t, std::string> names;  // an NPC's name matched by its wares
    sw::Tab want = sw::Tab::Buy;            // the tab the next choice asks for
    ULONGLONG readAt = 0, waitUntil = 0;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool moving = false;
    float grabX = 0, grabY = 0;
    WPARAM skipChar = 0;
    bool eatChar = false;  // 3.0.11-beta.101: the next character is a key the shop took (not the chat's)
    int cardWare = -1;     // the ware whose item card is open
    int told = 0, pressesTold = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t rd32(const uint8_t* p, size_t at) {
    return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16 | uint32_t(p[at + 3]) << 24;
}

uint32_t window(uint32_t slot, uint32_t cls) {
    const uint32_t w = read32(slot);
    return w && read32(w) == cls ? w : 0;
}

Deal dealNow() {
    Deal d;
    d.choose = window(kChooseAt, kChooseClass);
    d.list = window(kListAt, kListClass);
    d.buy = window(kBuyAt, kBuyClass);
    d.sell = window(kSellAt, kSellClass);
    if (d.buy && read32(d.buy + kCartKind) != 0) d.buy = 0;  // not an NPC's shop: the game's own
    return d;
}

bool ours(const Deal& d) { return d.choose || (d.list && (d.buy || d.sell)); }

void hold(State& s, const Deal& d, bool hidden) {  // the game's deal windows out of sight while ours shows
    for (const uint32_t w : {d.choose, d.list, d.buy, d.sell}) {
        if (!w) continue;
        const uint32_t want = hidden ? 0 : 1;
        if (read32(w + kShown) != want && (hidden || s.hid)) write32(w + kShown, want);
    }
    if (hidden && !s.hid && ours(d) && s.told++ < 30)
        LOG_INFO("shop: the game's deal windows hidden for the shop (their shown flags 0)");
    s.hid = hidden && ours(d);
}

bool press(uint32_t window, uint32_t sendMsg, int button) {
    if (!window || read32(read32(window) + kSendMsgSlot) != sendMsg) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(sendMsg))(window, 0, kButtonMsg, button, 0, 0);
    return true;
}

bool modeMsg(int msg, int a, int b, int c) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, c);
    return true;
}

bool codeIs(uint32_t at, const uint8_t* bytes, size_t size) {
    uint8_t have[32] = {};
    return size <= sizeof have && readGame(at, have, size) && std::memcmp(have, bytes, size) == 0;
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    float x = 0, y = 0;
    if (readTextFile(layout::join(gameDir(), layout::kShopState), text) &&
        std::sscanf(text.c_str(), "at %f %f", &x, &y) == 2 && x >= 0 && x <= 1 && y >= 0 && y <= 1) {
        s.fx = x;
        s.fy = y;
    }
}

void keepState(const State& s) {
    char text[48];
    std::snprintf(text, sizeof text, "at %.4f %.4f\n", s.fx, s.fy);
    const std::string path = layout::join(gameDir(), layout::kShopState);
    if (!writeFileAtomic(path, std::string(text))) LOG_INFO("shop: %s cannot be written", path.c_str());
}

std::string nameOf(const State& s, int id, bool identified) {
    const std::string key = std::to_string(id);
    std::string words;
    if (!identified) {
        const auto& unknown = clienttables::unknownNames();
        const auto u = unknown.find(key);
        if (u != unknown.end()) words = u->second;
    }
    if (words.empty()) {
        const auto& names = clienttables::itemNames();
        const auto w = names.find(key);
        if (w != names.end()) words = w->second;
    }
    return words.empty() ? "#" + key : utf8Of(widened(hotbar::spaced(words), s.codePage));
}

const panel::Picture* iconOf(const State& s, int id) {
    const auto& files = clienttables::itemFiles();
    const auto f = files.find(std::to_string(id));
    return f == files.end() ? nullptr : clienttables::icon(f->second, sw::iconSide(s.scale), 4 * s.scale);
}

void emptyCart(State& s) {
    s.data.cart.clear();
    s.show.edit = sw::Edit{};
    s.show.cartScroll = 0;
    s.show.line = 0;
    s.show.cartFocus = false;
}

// The game's deal list as the tab's wares; a list not as before empties the cart
void readDeal(State& s, sw::Tab tab) {
    std::vector<sw::Ware> wares;
    std::vector<uint32_t> infos;
    std::string key = tab == sw::Tab::Buy ? "b" : "s";
    const auto refs = reference::index();
    const uint32_t head = read32(kDeals);
    uint32_t node = head ? read32(head) : 0;
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kInfoSize] = {};
        if (!readGame(node + kInfo, raw, sizeof raw)) break;
        const uint32_t ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
        char digits[17] = {};
        if (!ptr || !len || len > 16 || !readGame(ptr, digits, len)) continue;
        sw::Ware w;
        w.id = std::atoi(digits);
        w.price = int(rd32(raw, 0x18));
        w.count = tab == sw::Tab::Sell ? int(rd32(raw, kCount)) : 0;
        const int refine = int(rd32(raw, 0x40));
        w.refine = tab == sw::Tab::Sell && refine > 0 && refine <= 20 ? refine : 0;
        w.identified = raw[0x3C] != 0;
        w.name = nameOf(s, w.id, w.identified);
        w.icon = iconOf(s, w.id);
        if (refs) {
            const auto it = refs->items.find(w.id);
            if (it != refs->items.end()) w.weight = it->second.weight;
        }
        key += std::to_string(rd32(raw, 0x08)) + ':' + std::to_string(w.id) + 'x' + std::to_string(w.count) + ',';
        wares.push_back(std::move(w));
        infos.push_back(node + kInfo);
    }
    if (key != s.listKey) {
        s.listKey = key;
        emptyCart(s);
        s.show.scroll = 0;
        s.show.ware = 0;
    }
    s.data.wares = std::move(wares);
    s.infos = std::move(infos);
}

std::string shopName(State& s, sw::Tab tab) {
    if (const auto it = s.names.find(s.npc); it != s.names.end()) return it->second;
    const auto refs = reference::index();
    if (!refs) return "";
    std::vector<int> ids;
    if (tab == sw::Tab::Buy)
        for (const sw::Ware& w : s.data.wares) ids.push_back(w.id);
    int x = 0, y = 0, width = 0, height = 0;
    if (!readPlayerCell(x, y, width, height)) return "";
    const int i = sw::matchShop(refs->shops, readGameMap(), ids, x, y);
    if (i < 0) return "";
    const std::string name = refs->shops[size_t(i)].name;
    if (!ids.empty() && s.npc) s.names[s.npc] = name;  // by its wares: kept for the sale's tab
    return name;
}

sw::Texts texts() {
    sw::Texts t;
    t.title = say(settings::Text::ShopTitle);
    t.buy = say(settings::Text::ShopBuy);
    t.sell = say(settings::Text::ShopSell);
    t.search = say(settings::Text::ShopSearch);
    t.ware = say(settings::Text::ShopWare);
    t.price = say(settings::Text::ShopPrice);
    t.weight = say(settings::Text::ShopWeight);
    t.cart = say(settings::Text::ShopCart);
    t.amount = say(settings::Text::ShopAmount);
    t.sum = say(settings::Text::ShopSum);
    t.total = say(settings::Text::ShopTotal);
    t.left = say(settings::Text::ShopLeft);
    t.after = say(settings::Text::ShopAfter);
    t.weightBuy = say(settings::Text::ShopWeightBuy);
    t.weightSell = say(settings::Text::ShopWeightSell);
    t.clear = say(settings::Text::ShopClear);
    t.note = say(settings::Text::ShopNote);
    t.empty = say(settings::Text::ShopEmpty);
    t.waiting = say(settings::Text::ShopWaiting);
    t.choose = say(settings::Text::ShopChoose);
    t.toCart = say(settings::Text::ShopToCart);
    t.plus = "+1";
    t.minus = "\xE2\x88\x92" "1";
    t.typeAmount = say(settings::Text::ShopKeyAmount);
    t.remove = say(settings::Text::ShopRemove);
    t.info = say(settings::Text::ShopInfo);
    t.close = say(settings::Text::SkillsPadClose);
    t.space = say(settings::Text::ShopSpace);
    t.editHint = say(settings::Text::ShopEditHint);
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

std::vector<Words> gdiWords(const std::vector<sw::Words>& in) {
    std::vector<Words> out;
    for (const sw::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == sw::Align::Right    ? DT_RIGHT
                                 : w.align == sw::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the shop", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    sw::paint(s.picture, s.layout, s.data, s.shownList, s.show, measure);
    drawWords(s.picture, gdiWords(sw::words(s.layout, s.data, s.shownList, s.show, texts(), measure)));
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
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Shop, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Shop, nullptr, nullptr, false);
    s.shown = false;
    s.moving = false;
    s.show.typing = false;
}

// The cart into the game's own purchase or sale list, sent by the cart window's own OK
void confirm(State& s, const char* by) {
    if (s.show.edit.ware >= 0) sw::endEdit(s.data, s.show.edit);  // the amount typed, as typed
    const Deal d = dealNow();
    const bool buy = s.data.tab == sw::Tab::Buy;
    const uint32_t cart = buy ? d.buy : d.sell;
    readDeal(s, s.data.tab);  // the ITEM_INFOs as the game keeps them now (a new list empties the cart)
    const sw::Sums sum =
        sw::sums(s.data.tab, s.data.cart, s.data.wares, s.data.zeny, s.data.weight, s.data.weightMost);
    if (!cart || !sum.ok || s.data.waiting) return;
    const uint32_t add = buy ? kAddPurchase : kAddSale;
    const bool known = buy ? codeIs(add, kAddPurchaseCode, sizeof kAddPurchaseCode)
                           : codeIs(add, kAddSaleCode, sizeof kAddSaleCode);
    if (!known) {
        LOG_INFO("shop: %s - the game's %s list is not where this build has it: not sent", by,
                 buy ? "purchase" : "sale");
        return;
    }
    int rows = 0;
    for (const sw::Line& l : s.data.cart) {
        if (l.ware < 0 || l.ware >= int(s.infos.size()) || l.amount <= 0) continue;
        const uint32_t info = s.infos[size_t(l.ware)];
        const uint32_t was = read32(info + kCount);
        if (!write32(info + kCount, uint32_t(l.amount))) continue;
        reinterpret_cast<AddFn>(uintptr_t(add))(kSession, info);
        write32(info + kCount, was);
        ++rows;
    }
    const int listed = int(read32(buy ? kPurchaseCount : kSaleCount));
    const bool sent = listed > 0 && press(cart, buy ? kBuySendMsg : kSellSendMsg, kOk);
    LOG_INFO("shop: %s - %d row(s) %s for %lld z: the game's %s list holds %d, %s", by, rows, buy ? "bought" : "sold",
             sum.total, buy ? "purchase" : "sale", listed,
             sent ? (buy ? "the cart's own OK (the game mode's 0x25, the packet 0x00c8)"
                         : "the cart's own OK (the game mode's 0x26, the packet 0x00c9)")
                  : "not sent");
    emptyCart(s);
    s.key.clear();
}

void closeDeal(State& s, const char* by) {
    const Deal d = dealNow();
    const bool done = press(d.buy, kBuySendMsg, kCancel) || press(d.sell, kSellSendMsg, kCancel) ||
                      press(d.choose, kChooseSendMsg, kCancel);
    s.waitUntil = 0;
    s.data.waiting = false;
    if (s.told++ < 30) LOG_INFO("shop: %s - %s", by, done ? "the deal closed by its own cancel (0x77)" : "not closed");
}

void switchTab(State& s, sw::Tab want, const char* by) {
    if (want == s.data.tab && !s.data.waiting) return;
    if (!s.npc) {
        if (s.told++ < 30) LOG_INFO("shop: %s - the other tab needs the NPC, not known", by);
        return;
    }
    const Deal d = dealNow();
    if (!press(d.buy, kBuySendMsg, kCancel)) press(d.sell, kSellSendMsg, kCancel);  // the game's own cancel
    const bool asked = modeMsg(want == sw::Tab::Sell ? kAskSell : kAskBuy, int(s.npc), 0, 0);
    s.want = want;
    s.data.tab = want;
    s.data.waiting = asked;
    s.waitUntil = asked ? GetTickCount64() + kWaitMs : 0;
    s.listKey.clear();
    emptyCart(s);
    s.show.search.clear();
    s.show.typing = false;
    s.key.clear();
    if (s.told++ < 30)
        LOG_INFO("shop: %s - %s %s", by, want == sw::Tab::Sell ? "«Продати»" : "«Купити»",
                 asked ? (want == sw::Tab::Sell ? "asked of the NPC (the game mode's 0x28, the packet 0x00c5)"
                                                : "asked of the NPC (the game mode's 0x29, the packet 0x00c5)")
                       : "- not asked");
}

int lineOf(const State& s, int ware) {
    for (size_t i = 0; i < s.data.cart.size(); ++i)
        if (s.data.cart[i].ware == ware) return int(i);
    return -1;
}

// 3.0.11-beta.101: the ware under the frame - the list's (a place in the shown list) or the cart's row's; -1 none
int framedWare(const State& s) {
    if (s.show.cartFocus)
        return s.show.line >= 0 && s.show.line < int(s.data.cart.size()) ? s.data.cart[size_t(s.show.line)].ware : -1;
    return s.show.ware >= 0 && s.show.ware < int(s.shownList.size()) ? s.shownList[size_t(s.show.ware)] : -1;
}

void keepFrameInSight(State& s) {
    const int count = int(s.shownList.size()), lines = int(s.data.cart.size());
    s.show.ware = std::clamp(s.show.ware, 0, std::max(0, count - 1));
    s.show.line = std::clamp(s.show.line, 0, std::max(0, lines - 1));
    if (!lines) s.show.cartFocus = false;
    if (s.show.cartFocus)
        s.show.cartScroll = sw::scrollFor(s.show.line, s.show.cartScroll, sw::kCartRows, lines);
    else
        s.show.scroll = sw::scrollFor(s.show.ware, s.show.scroll, sw::kListRows, count);
    s.show.cartScroll = std::clamp(s.show.cartScroll, 0, std::max(0, lines - sw::kCartRows));
}

void lineInSight(State& s, int ware) {  // the cart's row of the ware changed: in sight (the frame stays the frame)
    const int at = lineOf(s, ware), lines = int(s.data.cart.size());
    if (at >= 0 && !s.show.cartFocus) s.show.cartScroll = sw::scrollFor(at, s.show.cartScroll, sw::kCartRows, lines);
    keepFrameInSight(s);
    s.key.clear();
}

void putIn(State& s, int ware, int by) {
    if (s.show.edit.ware >= 0) sw::endEdit(s.data, s.show.edit);
    sw::put(s.data, ware, by);
    lineInSight(s, ware);
}

void takeOut(State& s, int ware, const char* by) {  // its whole row back
    if (ware < 0) return;
    if (s.show.edit.ware == ware) s.show.edit = sw::Edit{};
    const int amount = sw::amountOf(s.data.cart, ware);
    sw::takeBack(s.data, ware);
    keepFrameInSight(s);
    s.key.clear();
    if (amount > 0 && s.told++ < 30)
        LOG_INFO("shop: %s - %d of the row %d taken back out of the cart", by, amount, ware);
}

void moveFrame(State& s, int step) {
    if (s.show.edit.ware >= 0) sw::endEdit(s.data, s.show.edit);
    s.show.framed = true;
    if (s.show.cartFocus)
        s.show.line += step;
    else
        s.show.ware += step;
    keepFrameInSight(s);
    s.key.clear();
}

void switchPane(State& s, bool toCart) {  // the frame onto the same ware's row on the other side when it has one
    if (s.show.edit.ware >= 0) sw::endEdit(s.data, s.show.edit);
    const int ware = framedWare(s);
    s.show.framed = true;
    s.show.cartFocus = toCart && !s.data.cart.empty();
    if (s.show.cartFocus) {
        if (const int at = lineOf(s, ware); at >= 0) s.show.line = at;
    } else {
        for (size_t i = 0; i < s.shownList.size(); ++i)
            if (s.shownList[i] == ware) s.show.ware = int(i);
    }
    keepFrameInSight(s);
    s.key.clear();
}

bool cardOpen() { return read32(kCardAt) != 0; }

void closeCard(State& s) {
    if (cardOpen()) reinterpret_cast<DeleteFn>(uintptr_t(kDeleteWindow))(kWindowMgr, kItemCard);
    s.cardWare = -1;
}

// 3.0.11-beta.101: the game's own item card of the ware (its ITEM_INFO of the deal list) beside the shop; again for the
// same ware closes it
void toggleCard(State& s, int ware, const char* by) {
    if (ware < 0 || ware >= int(s.infos.size())) return;
    if (cardOpen() && s.cardWare == ware) {
        closeCard(s);
        return;
    }
    const uint32_t window = reinterpret_cast<MakeFn>(uintptr_t(kMakeWindow))(kWindowMgr, kItemCard);
    const uint32_t send = window ? read32(read32(window) + kSendMsgSlot) : 0;
    if (!send) return;
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kShowItem, int(s.infos[size_t(ware)]), 0, 0);
    s.cardWare = ware;
    int32_t r[4] = {};  // its place and size in the game's pixels: on the shop's right, else on its left
    const uint32_t move = read32(read32(window) + kMoveSlot);
    bool moved = false;
    if (s.k > 0 && move && readGame(window + kRect, r, sizeof r) && r[2] > 0 && r[3] > 0) {
        const int gameW = int(float(s.targetW) / s.k), gameH = int(float(s.targetH) / s.k);
        const int left = int(float(s.layout.x) / s.k), right = int(float(s.layout.x + s.layout.w) / s.k);
        int x = right + 8;
        if (x + r[2] > gameW) x = left - 8 - r[2];
        if (x < 0) x = std::max(0, gameW - r[2]);
        const int y = std::clamp(int(float(s.layout.y) / s.k), 0, std::max(0, gameH - r[3]));
        reinterpret_cast<MoveFn>(uintptr_t(move))(window, x, y);
        moved = true;
    }
    if (s.told++ < 30)
        LOG_INFO("shop: %s - the item card of #%d (the game's own window 0xC, %dx%d)%s", by,
                 s.data.wares[size_t(ware)].id, r[2], r[3], moved ? " beside the shop" : "");
}

char letterOf(WPARAM vk, bool shift) {
    if (vk >= 'A' && vk <= 'Z') return char(shift ? vk : vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9' && !shift) return char(vk);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return char('0' + (vk - VK_NUMPAD0));
    switch (vk) {
        case VK_SPACE: return ' ';
        case VK_OEM_MINUS: return shift ? '_' : '-';
        case VK_OEM_7: return '\'';
        case VK_OEM_PERIOD: return '.';
        default: return 0;
    }
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        if (!on && inGameWorld()) hold(s, dealNow(), false);
        LOG_INFO("shop: [interface] shop = %d", on ? 1 : 0);
    }
    const ULONGLONG now = GetTickCount64();
    const Deal deal = inGameWorld() ? dealNow() : Deal{};
    const bool buy = deal.list && deal.buy, sell = deal.list && deal.sell;
    const bool waiting = !buy && !sell && (deal.choose || now < s.waitUntil);
    if (on && ours(deal)) hold(s, deal, true);
    if (!on || (!buy && !sell && !waiting) || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        if (!buy && !sell && !waiting && !s.data.wares.empty()) {  // the deal over
            s.data.wares.clear();
            s.infos.clear();
            s.listKey.clear();
            emptyCart(s);
            s.show.search.clear();
            s.want = sw::Tab::Buy;
        }
        hide(s);
        return;
    }
    readState(s);
    const float scale = uistyle::readingScale(k);
    const bool fresh = scale != s.scale || codePage != s.codePage;
    s.k = float(k);
    s.scale = scale;
    s.codePage = codePage;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    if (buy || sell) {
        const sw::Tab tab = buy ? sw::Tab::Buy : sw::Tab::Sell;
        if (tab != s.data.tab || s.data.waiting) {
            s.data.tab = tab;
            s.data.waiting = false;
            s.waitUntil = 0;
            s.readAt = 0;
        }
        if (fresh || now - s.readAt >= kReadMs) {
            s.readAt = now;
            readDeal(s, tab);
            s.data.name = shopName(s, tab);
        }
    } else {
        s.data.waiting = true;
        s.data.wares.clear();
        s.infos.clear();
    }
    charstatus::Status st;
    if (readOwnStatus(st)) {
        s.data.zeny = st.zeny;
        s.data.weight = st.weight;
        s.data.weightMost = st.weightMost;
    }
    s.data.place = readGameMap();
    const uapad::runtime::Look pad = uapad::runtime::look();
    s.show.pad = pad.shown;
    s.show.sony = pad.sony;
    s.shownList = sw::shownOf(s.data.wares, s.show.search);
    if (!s.shown) {  // just open: no frame until the keys, the controller or a click move it
        s.show.framed = false;
        s.show.ware = s.show.line = 0;
        s.show.cartFocus = false;
    }
    const int count = int(s.shownList.size()), lines = int(s.data.cart.size());
    s.show.ware = std::clamp(s.show.ware, 0, std::max(0, count - 1));
    s.show.line = std::clamp(s.show.line, 0, std::max(0, lines - 1));
    if (!lines) s.show.cartFocus = false;
    s.show.scroll = std::clamp(s.show.scroll, 0, std::max(0, count - sw::kListRows));
    s.show.cartScroll = std::clamp(s.show.cartScroll, 0, std::max(0, lines - sw::kCartRows));
    s.show.caretOn = (s.show.typing || s.show.edit.ware >= 0) && (now / 530) % 2 == 0;
    const sw::Texts t = texts();
    s.layout = sw::layout(0, 0, scale, s.data, s.shownList, s.show, t, measure);
    s.layout.x = int(std::lround(s.fx * float(std::max(0, targetWidth - s.layout.w))));
    s.layout.y = int(std::lround(s.fy * float(std::max(0, targetHeight - s.layout.h))));
    s.show.hoverTab = s.show.hoverWare = s.show.hoverLine = s.show.hoverLess = s.show.hoverMore = -1;
    s.show.hoverAmount = s.show.hoverTakeBack = -1;
    s.show.hoverConfirm = s.show.hoverClear = s.show.hoverClose = s.show.hoverSearch = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy) && !s.moving) {
        const sw::Hit h = sw::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        switch (h.part) {
            case sw::Part::Tab: s.show.hoverTab = h.index; break;
            case sw::Part::Ware: s.show.hoverWare = h.index; break;
            case sw::Part::Line: s.show.hoverLine = h.index; break;
            case sw::Part::Less: s.show.hoverLess = h.index; break;
            case sw::Part::More: s.show.hoverMore = h.index; break;
            case sw::Part::Amount: s.show.hoverAmount = h.index; break;
            case sw::Part::TakeBack: s.show.hoverTakeBack = h.index; break;
            case sw::Part::Confirm: s.show.hoverConfirm = true; break;
            case sw::Part::Clear: s.show.hoverClear = true; break;
            case sw::Part::Close: s.show.hoverClose = true; break;
            case sw::Part::Search: s.show.hoverSearch = true; break;
            default: break;
        }
    }
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(int(s.data.tab)) + (s.data.waiting ? "w" : "") + ';' +
                      std::to_string(s.show.hoverTab) + ',' + std::to_string(s.show.hoverWare) + ',' +
                      std::to_string(s.show.hoverLine) + ',' + std::to_string(s.show.hoverLess) + ',' +
                      std::to_string(s.show.hoverMore) + ',' + std::to_string(s.show.hoverAmount) + ',' +
                      std::to_string(s.show.hoverTakeBack) + (s.show.hoverConfirm ? "c" : "") +
                      (s.show.hoverClear ? "l" : "") + (s.show.hoverClose ? "x" : "") +
                      (s.show.hoverSearch ? "f" : "") + ';' + std::to_string(s.show.scroll) + ',' +
                      std::to_string(s.show.cartScroll) + ';' + (s.show.typing ? "t" : "") +
                      (s.show.caretOn ? "|" : "") + s.show.search + ';' +
                      (s.show.pad ? (s.show.sony ? "S" : "X") : "") + (s.show.cartFocus ? "C" : "") +
                      (s.show.framed ? "F" : "") + std::to_string(s.show.ware) + ',' + std::to_string(s.show.line) +
                      ";e" + std::to_string(s.show.edit.ware) + ':' + s.show.edit.digits + ';' + s.data.name +
                      ';' + s.data.place + ';' + std::to_string(s.data.zeny) + ',' + std::to_string(s.data.weight) +
                      '/' + std::to_string(s.data.weightMost) + ';' + s.listKey + ';';
    for (const sw::Ware& w : s.data.wares) key += w.icon ? "i" : "-";
    key += ';';
    for (const sw::Line& l : s.data.cart) key += std::to_string(l.ware) + 'x' + std::to_string(l.amount) + ',';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {
        s.key = drawn;
        render(s);
    }
    if (!s.shown && s.told < 30) {
        ++s.told;
        LOG_INFO("shop: the shop's window open - %s, %zu ware(s), the NPC %u%s%s",
                 buy ? "buying" : sell ? "selling" : "waiting for its list", s.data.wares.size(), s.npc,
                 s.data.name.empty() ? "" : " - ", s.data.name.c_str());
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
    if (windowstack::marked(winstack::Win::Shop))  // 3.0.11-beta.108: its gold edge, the top of two or more
        s.edge.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                    s.scale, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool shown() { return state().shown; }

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    bool fromGame = false;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, &fromGame)) return false;
    if (s.moving && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {
        const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
        s.fx = std::clamp((tx - s.grabX) / float(roomX), 0.0f, 1.0f);
        s.fy = std::clamp((ty - s.grabY) / float(roomY), 0.0f, 1.0f);
        if (m == WM_LBUTTONUP) {
            s.moving = false;
            keepState(s);
        }
        result = 0;
        return true;
    }
    const sw::Hit h = sw::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if ((m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) && s.pressesTold < 8) {
        ++s.pressesTold;
        LOG_INFO("shop: a press (0x%X) at the window's (%d, %d) - the target's (%.0f, %.0f) by the %s; the shop at %d, "
                 "%d %dx%d: part %d, index %d", m, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), tx, ty,
                 fromGame ? "game's cursor" : "message's point", s.layout.x, s.layout.y, s.layout.w, s.layout.h,
                 int(h.part), h.index);
    }
    if (h.part == sw::Part::None) {
        if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) {
            s.show.typing = false;
            if (s.show.edit.ware >= 0) sw::endEdit(s.data, s.show.edit);
        }
        return false;
    }
    if (m == WM_MOUSEMOVE) return false;
    result = 0;
    const WORD keys = GET_KEYSTATE_WPARAM(wp);
    const bool shift = (keys & MK_SHIFT) != 0, ctrl = (keys & MK_CONTROL) != 0;
    if (m == WM_MOUSEWHEEL) {
        const int steps = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -3 : 3;
        if (h.part == sw::Part::Line || h.part == sw::Part::Less || h.part == sw::Part::More ||
            h.part == sw::Part::Amount || h.part == sw::Part::TakeBack || h.part == sw::Part::Cart)
            s.show.cartScroll = std::clamp(s.show.cartScroll + (steps > 0 ? 1 : -1), 0,
                                           std::max(0, int(s.data.cart.size()) - sw::kCartRows));
        else
            s.show.scroll =
                std::clamp(s.show.scroll + steps, 0, std::max(0, int(s.shownList.size()) - sw::kListRows));
        s.key.clear();
        return true;
    }
    const auto wareOf = [&](int at) {
        return at >= 0 && at < int(s.shownList.size()) ? s.shownList[size_t(at)] : -1;
    };
    const auto lineWare = [&](int at) {
        return at >= 0 && at < int(s.data.cart.size()) ? s.data.cart[size_t(at)].ware : -1;
    };
    if (m == WM_RBUTTONDOWN) {  // 3.0.11-beta.101: its description, as the game's own shop shows it
        if (h.part == sw::Part::Ware) toggleCard(s, wareOf(h.index), "a right click");
        if (h.part == sw::Part::Line || h.part == sw::Part::Amount) toggleCard(s, lineWare(h.index), "a right click");
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    if (h.part != sw::Part::Search) s.show.typing = false;
    if (h.part != sw::Part::Amount && s.show.edit.ware >= 0) sw::endEdit(s.data, s.show.edit);
    const int many = s.data.tab == sw::Tab::Sell ? sw::kMostBought : 10;  // Shift: ten bought, all of it sold
    switch (h.part) {
        case sw::Part::Close: closeDeal(s, "its close"); break;
        case sw::Part::Tab: switchTab(s, h.index == 1 ? sw::Tab::Sell : sw::Tab::Buy, "a click on its tab"); break;
        case sw::Part::Search: s.show.typing = true; break;
        case sw::Part::Ware:  // one more (Shift ten, Ctrl one fewer); the frame on it
            s.show.framed = true;
            s.show.cartFocus = false;
            s.show.ware = h.index;
            putIn(s, wareOf(h.index), ctrl ? -1 : shift ? many : 1);
            break;
        case sw::Part::Line:
            s.show.framed = true;
            s.show.cartFocus = true;
            s.show.line = h.index;
            break;
        case sw::Part::Less: putIn(s, lineWare(h.index), shift ? -10 : -1); break;
        case sw::Part::More: putIn(s, lineWare(h.index), shift ? 10 : 1); break;
        case sw::Part::Amount: {  // the amount typed from the keys: the digits replace it
            const int ware = lineWare(h.index);
            if (ware < 0) break;
            if (s.show.edit.ware != ware) {
                sw::endEdit(s.data, s.show.edit);
                s.show.edit = sw::Edit{ware, "", sw::amountOf(s.data.cart, ware)};
            }
            s.show.framed = true;
            s.show.cartFocus = true;
            s.show.line = h.index;
            break;
        }
        case sw::Part::TakeBack: takeOut(s, lineWare(h.index), "its cross"); break;
        case sw::Part::Confirm: confirm(s, "a click on its button"); break;
        case sw::Part::Clear: emptyCart(s); break;
        case sw::Part::Title:
            s.moving = true;
            s.grabX = tx - float(s.layout.x);
            s.grabY = ty - float(s.layout.y);
            break;
        default: break;
    }
    s.key.clear();
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (!s.shown) return false;
    if (m == WM_CHAR || m == WM_SYSCHAR) {
        if (s.skipChar && wp == s.skipChar) {  // Enter's, Esc's, Tab's own character
            s.skipChar = 0;
            return true;
        }
        if (s.eatChar) {  // a digit's, Space's, + or -'s: the shop's, not the chat's
            s.eatChar = false;
            return true;
        }
    }
    if (s.show.typing) {  // the search
        if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
            const bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0;
            if (alt) return false;
            switch (wp) {
                case VK_ESCAPE:
                case VK_RETURN:
                    s.show.typing = false;
                    s.skipChar = wp == VK_ESCAPE ? 27 : 13;
                    s.key.clear();
                    return true;
                case VK_BACK:
                    if (!s.show.search.empty()) s.show.search.pop_back();
                    s.show.scroll = 0;
                    s.skipChar = 8;
                    return true;
                case VK_DELETE:
                    s.show.search.clear();
                    s.show.scroll = 0;
                    return true;
                case VK_DOWN:  // into the list
                case VK_TAB:
                    s.show.typing = false;
                    s.show.framed = true;
                    s.show.cartFocus = false;
                    s.show.ware = 0;
                    keepFrameInSight(s);
                    s.skipChar = wp == VK_TAB ? 9 : 0;
                    s.key.clear();
                    return true;
                default: break;
            }
            const char c = ctrl ? 0 : letterOf(wp, GetKeyState(VK_SHIFT) < 0);
            if (c && s.show.search.size() < kMostTyped) {
                s.show.search += c;
                s.show.scroll = 0;
                s.show.ware = 0;
            }
            return true;  // every key the search's while it types
        }
        return m == WM_KEYUP || m == WM_CHAR || m == WM_DEADCHAR;
    }
    if (m != WM_KEYDOWN) return false;
    s.eatChar = false;  // a new key: the one before brought no character
    const bool shift = GetKeyState(VK_SHIFT) < 0, ctrl = GetKeyState(VK_CONTROL) < 0;
    if (GetKeyState(VK_MENU) < 0) return false;  // Alt+...: the game's
    const bool editing = s.show.edit.ware >= 0;
    if (!editing && !chatwin::inputEmpty()) return false;  // words in the chat's line: its keys
    const int digit = wp >= '0' && wp <= '9' && !shift      ? int(wp - '0')
                      : wp >= VK_NUMPAD0 && wp <= VK_NUMPAD9 ? int(wp - VK_NUMPAD0)
                                                             : -1;
    if (digit >= 0 && !ctrl) {  // the amount typed for the framed ware
        const int ware = editing ? s.show.edit.ware : framedWare(s);
        if (ware < 0) return false;
        s.show.framed = true;
        sw::typeDigit(s.data, s.show.edit, ware, char('0' + digit));
        lineInSight(s, ware);
        s.eatChar = true;
        return true;
    }
    const int page = s.show.cartFocus ? sw::kCartRows : sw::kListRows;
    switch (wp) {
        case VK_BACK:
            if (!editing) return false;
            sw::eraseDigit(s.data, s.show.edit);
            s.eatChar = true;
            s.key.clear();
            return true;
        case VK_RETURN:
            if (editing) {
                sw::endEdit(s.data, s.show.edit);
                keepFrameInSight(s);
            } else if (s.data.cart.empty()) {
                return false;  // the chat's Enter
            } else {
                confirm(s, "Enter");
            }
            s.skipChar = 13;
            s.key.clear();
            return true;
        case VK_ESCAPE:  // the typing back as it was, else the item card, else the deal
            if (editing)
                sw::cancelEdit(s.data, s.show.edit);
            else if (cardOpen())
                closeCard(s);
            else
                closeDeal(s, "Esc");
            keepFrameInSight(s);
            s.skipChar = 27;
            s.key.clear();
            return true;
        case VK_UP: moveFrame(s, -1); return true;
        case VK_DOWN: moveFrame(s, 1); return true;
        case VK_PRIOR: moveFrame(s, -page); return true;
        case VK_NEXT: moveFrame(s, page); return true;
        case VK_HOME: moveFrame(s, -100000); return true;
        case VK_END: moveFrame(s, 100000); return true;
        case VK_LEFT:
        case VK_RIGHT: switchPane(s, wp == VK_RIGHT); return true;
        case VK_TAB:
            switchPane(s, !s.show.cartFocus);
            s.skipChar = 9;
            return true;
        case VK_SPACE:
        case VK_ADD:
        case VK_OEM_PLUS:
        case VK_SUBTRACT:
        case VK_OEM_MINUS: {
            const int ware = framedWare(s);
            if (ware < 0) return false;
            const bool less = wp == VK_SUBTRACT || wp == VK_OEM_MINUS;
            s.show.framed = true;
            putIn(s, ware, (less ? -1 : 1) * (wp == VK_SPACE && shift ? 10 : 1));
            s.eatChar = true;
            return true;
        }
        case VK_DELETE: takeOut(s, editing ? s.show.edit.ware : framedWare(s), "Del"); return true;
        case VK_F1: toggleCard(s, framedWare(s), "F1"); return true;
        default: return false;
    }
}

void padCommand(PadCommand c, int times) {
    State& s = state();
    if (!s.shown) return;
    s.show.framed = true;
    const bool editing = s.show.edit.ware >= 0;
    switch (c) {
        case PadCommand::Up: moveFrame(s, -1); break;
        case PadCommand::Down: moveFrame(s, 1); break;
        case PadCommand::Left:
        case PadCommand::Right: switchPane(s, c == PadCommand::Right); break;
        case PadCommand::Plus:
        case PadCommand::Minus:
            putIn(s, framedWare(s), (c == PadCommand::Plus ? 1 : -1) * std::max(1, times));
            break;
        case PadCommand::TakeBack: takeOut(s, framedWare(s), "the controller's LT"); break;
        case PadCommand::Info: toggleCard(s, framedWare(s), "the controller's Y"); break;
        case PadCommand::Confirm: confirm(s, "the controller's Start"); break;
        case PadCommand::TabBack:
        case PadCommand::TabOn:
            switchTab(s, s.data.tab == sw::Tab::Buy ? sw::Tab::Sell : sw::Tab::Buy,
                      c == PadCommand::TabBack ? "the controller's LB" : "the controller's RB");
            break;
        case PadCommand::Close:  // the typing, else the item card, else the deal
            if (editing)
                sw::cancelEdit(s.data, s.show.edit);
            else if (cardOpen())
                closeCard(s);
            else
                closeDeal(s, "the controller's B");
            break;
    }
    keepFrameInSight(s);
    s.key.clear();
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    const Deal d = dealNow();
    if (ours(d)) hold(s, d, true);
    if (!d.choose) {
        s.chosen = 0;
        return;
    }
    if (d.choose == s.chosen) return;
    // the deal's choice: its own «Купити» (or the tab asked for) at once - the list comes as the shop's tab
    s.chosen = d.choose;
    s.npc = read32(d.choose + kChooseNpc);
    const auto known = s.names.find(s.npc);  // 3.0.11-beta.101: not the shop before's name meanwhile
    s.data.name = known != s.names.end() ? known->second : "";
    const bool sell = s.want == sw::Tab::Sell;
    const bool done = press(d.choose, kChooseSendMsg, sell ? kChooseSell : kChooseBuy);
    s.data.tab = s.want;
    s.data.waiting = done;
    s.waitUntil = done ? GetTickCount64() + kWaitMs : 0;
    if (s.told++ < 30)
        LOG_INFO("shop: the deal's choice with the NPC %u - %s %s", s.npc, sell ? "«Продати»" : "«Купити»",
                 done ? (sell ? "pressed (its own button: the game mode's 0x28)"
                              : "pressed (its own button: the game mode's 0x29)")
                      : "- not pressed");
}

}  // namespace uagfx::win::shopwin
