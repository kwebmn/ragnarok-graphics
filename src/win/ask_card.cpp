#include "win/ask_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/ask_card.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::askcardwin {
namespace {

namespace ac = uagfx::askcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the windows' manager's small windows - where each is kept, its class's vtable and its SendMsg (message
// 6: 0x76 accept / OK, 0x77 decline - each deletes its window and tells the game mode); the inputs first
struct GameAsk {
    ac::Kind kind;
    uint32_t at, cls, send;
};
constexpr uint32_t kWindowMgr = 0x727ED8;
constexpr GameAsk kAsks[] = {{ac::Kind::Number, kWindowMgr + 0x160, 0x6989F8, 0x4759E0},
                             {ac::Kind::Text, kWindowMgr + 0x164, 0x698A90, 0x475DB0},
                             {ac::Kind::Trade, kWindowMgr + 0x110, 0x6994A8, 0x47E8E0},
                             {ac::Kind::Party, kWindowMgr + 0x120, 0x6995D8, 0x47FDB0},
                             {ac::Kind::Guild, kWindowMgr + 0x134, 0x6998D0, 0x480B80},
                             {ac::Kind::Friend, kWindowMgr + 0x1DC, 0x69A258, 0x48D610}};
constexpr uint32_t kShown = 0x38, kRect = 0x14, kSendMsgSlot = 0x94, kEdit = 0x60, kGetText = 0x443300;
constexpr int kButtonMsg = 6;
constexpr int kAway = -4000;  // an input's window moved out of sight, its field still the keys'
constexpr ULONGLONG kNameMs = 60000, kBlinkMs = 530;
// 3.0.11-beta.155: the game's «how many» window (UI window 0xF, «UIItemDropCntWnd»: its class, its SendMsg
// FUN_004ad2e0 - 6 / 0x76 its only button, OK; what it is for + 0x30 - 0 the ground, 6 the bag, 7 the storage, 8 the
// cart ...) - made once and shown or hidden (the manager's FUN_004d4090(0xF, 1 / 0)); the most from the game mode's
// drag (its count + 0x33C, as the game fills the field); an edit's SetText at its vtable + 0x98
constexpr uint32_t kCountAt = kWindowMgr + 0xA0, kCountClass = 0x69B2B0, kCountSend = 0x4AD2E0, kCountFor = 0x30;
constexpr uint32_t kShowWindow = 0x4D4090, kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kDragCount = 0x33C;
constexpr uint32_t kSetTextSlot = 0x98;
constexpr int kCountWindow = 0xF, kEndDragMsg = 0x13;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetTextFn = void(__attribute__((thiscall)) *)(uint32_t edit, const char* text);
using ShowFn = int(__attribute__((thiscall)) *)(uint32_t manager, int id, int show);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);

struct Named {
    std::string name;  // in the game's code page
    ULONGLONG at = 0;
};

struct State {
    WindowShadow shadow;
    bool on = false, shown = false, hid = false, moved = false, padNow = false;
    bool counts = false, countMoved = false;  // 3.0.11-beta.155: [interface] counts; its window out of sight
    int countX = 0, countY = 0;               // its own place (back when the switch goes off)
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    ac::Kind kind = ac::Kind::Trade;
    uint32_t window = 0;
    int oldX = 0, oldY = 0;  // an input window's own place
    Named names[4];          // the last request's of each kind (trade, party, guild, friend)
    ac::Card card;
    ac::Show show;
    ac::Layout layout;
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

const char* nameOf(ac::Kind k) {
    switch (k) {
        case ac::Kind::Trade: return "a trade's request";
        case ac::Kind::Party: return "a party's invitation";
        case ac::Kind::Guild: return "a guild's invitation";
        case ac::Kind::Friend: return "a friend's request";
        case ac::Kind::Number: return "an NPC's number";
        case ac::Kind::Text: return "an NPC's words";
        case ac::Kind::Count: return "the game's «how many»";
        default: break;  // the game's messages (win/msg_box)
    }
    return "a message";
}

// The game's small window made now (the inputs first), its kind
uint32_t gameAsk(ac::Kind& kind) {
    for (const GameAsk& g : kAsks) {
        const uint32_t w = read32(g.at);
        if (w && read32(w) == g.cls && read32(read32(w) + kSendMsgSlot) == g.send) {
            kind = g.kind;
            return w;
        }
    }
    return 0;
}

// Held out of sight while ours shows: a request's window hidden (its shown flag 0); an input's moved off the screen,
// its field still the keys' (and the controller's typing); back when ours is off
void hold(State& s, uint32_t window, ac::Kind kind, bool on) {
    if (!window) {
        s.hid = s.moved = false;
        return;
    }
    if (ac::request(kind)) {
        if (on) {
            if (read32(window + kShown) && write32(window + kShown, 0) && !s.hid) s.hid = true;
        } else if (s.hid) {
            write32(window + kShown, 1);
            s.hid = false;
        }
        return;
    }
    const int32_t x = int32_t(read32(window + kRect)), y = int32_t(read32(window + kRect + 4));
    if (on && x != kAway) {
        s.oldX = x;
        s.oldY = y;
        write32(window + kRect, uint32_t(kAway));
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        write32(window + kRect + 4, uint32_t(s.oldY));
        s.moved = false;
    }
}

// 3.0.11-beta.155: the «how many» window (made once: there whether shown or not)
uint32_t gameCount() {
    const uint32_t w = read32(kCountAt);
    return w && read32(w) == kCountClass && read32(read32(w) + kSendMsgSlot) == kCountSend ? w : 0;
}

// Out of sight while the switch is on (its field still the keys' - Enter its OK); back where it was when it goes off
void holdCount(State& s, uint32_t window, bool on) {
    if (!window) return;
    const int32_t x = int32_t(read32(window + kRect)), y = int32_t(read32(window + kRect + 4));
    if (on && x != kAway) {
        s.countX = x;
        s.countY = y;
        write32(window + kRect, uint32_t(kAway));
        s.countMoved = true;
    } else if (!on && s.countMoved) {
        write32(window + kRect, uint32_t(s.countX));
        write32(window + kRect + 4, uint32_t(s.countY));
        s.countMoved = false;
    }
}

settings::Text countTitle(uint32_t purpose) {
    switch (purpose) {
        case 0: return settings::Text::CountDrop;
        case 1: return settings::Text::CountBuy;
        case 2:
        case 3:
        case 4: return settings::Text::CountSell;
        case 5: return settings::Text::CountTrade;
        case 6: return settings::Text::CountBag;
        case 7: return settings::Text::CountStorage;
        case 8: return settings::Text::CountCart;
        case 9:
        case 10: return settings::Text::CountShop;
        case 0xB: return settings::Text::CountUnbuy;
        case 0xC: return settings::Text::CountToBuy;
        case 0xD: return settings::Text::CountMail;
        default: return settings::Text::CountTitle;
    }
}

int requestIndex(ac::Kind k) {
    return k == ac::Kind::Trade ? 0 : k == ac::Kind::Party ? 1 : k == ac::Kind::Guild ? 2 : 3;
}

std::string utf8(const State& s, const std::string& game) { return utf8Of(widened(game, s.codePage)); }

ac::Card cardOf(State& s, ac::Kind kind, uint32_t window) {
    ac::Card c;
    c.kind = kind;
    if (ac::request(kind)) {
        const Named& n = s.names[requestIndex(kind)];
        const bool known = !n.name.empty() && GetTickCount64() - n.at < kNameMs;
        const std::string name = known ? utf8(s, n.name) : "";
        switch (kind) {
            case ac::Kind::Trade:
                c.title = say(settings::Text::AskTradeTitle);
                c.message = known ? settings::fmt(say(settings::Text::AskTrade), {name})
                                  : say(settings::Text::AskTradeAny);
                break;
            case ac::Kind::Party:
                c.title = say(settings::Text::AskPartyTitle);
                c.message = known ? settings::fmt(say(settings::Text::AskParty), {name})
                                  : say(settings::Text::AskPartyAny);
                break;
            case ac::Kind::Guild:
                c.title = say(settings::Text::AskGuildTitle);
                c.message = known ? settings::fmt(say(settings::Text::AskGuild), {name})
                                  : say(settings::Text::AskGuildAny);
                break;
            default:
                c.title = say(settings::Text::AskFriendTitle);
                c.message = known ? settings::fmt(say(settings::Text::AskFriend), {name})
                                  : say(settings::Text::AskFriendAny);
                break;
        }
        return c;
    }
    if (kind == ac::Kind::Count) {  // 3.0.11-beta.155: what for, the most
        c.title = say(countTitle(read32(window + kCountFor)));
        const uint32_t mode = read32(kModeNow);
        c.most = mode ? int(read32(mode + kDragCount)) : 0;
        c.message = settings::fmt(say(settings::Text::CountHint), {std::to_string(c.most)});
    } else {
        c.title = say(kind == ac::Kind::Number ? settings::Text::AskNumberTitle : settings::Text::AskTextTitle);
        c.message = say(settings::Text::AskTypeHint);
    }
    const uint32_t edit = read32(window + kEdit);
    if (edit) {
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
        char buf[96] = {};
        if (t && readGame(uint32_t(reinterpret_cast<uintptr_t>(t)), buf, sizeof buf - 1)) {
            buf[sizeof buf - 1] = 0;
            c.field = utf8(s, buf);
        }
    }
    c.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
    return c;
}

ac::Texts texts(const State& s) {
    ac::Texts t;
    t.accept = say(settings::Text::AskAccept);
    t.decline = say(settings::Text::AskDecline);
    t.ok = say(settings::Text::AskOk);
    t.acceptKey = s.padNow ? uapad::buttonLabel(0) : "";
    t.declineKey = s.padNow ? uapad::buttonLabel(1) : "Esc";
    t.okKey = s.padNow ? "" : "Enter";
    t.cancel = say(settings::Text::MsgCancel);  // 3.0.11-beta.155: a count's
    t.quick[0] = "10";  // 3.0.15-beta.7 (a player: «там де 1 шт зробити десятки 10»)
    t.quick[1] = say(settings::Text::CountHalf);
    t.quick[2] = say(settings::Text::CountAll);
    return t;
}

std::vector<Words> gdiWords(const std::vector<ac::Words>& in) {
    std::vector<Words> out;
    for (const ac::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ac::Align::Right    ? DT_RIGHT
                                 : w.align == ac::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("a request", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    const ac::Texts t = texts(s);
    ac::paint(s.picture, s.layout, s.card, s.show);
    drawWords(s.picture, gdiWords(ac::words(s.layout, s.card, s.show, t)));
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
    uapad::gamewindow::setBlocked(Blocker::Ask, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Ask, nullptr, nullptr, false);
    s.shown = false;
}

// 3.0.11-beta.155: a count given up - the game's drag ended (its mode's 0x13) and its window hidden, as nothing taken
void cancelCount(State& s) {
    const uint32_t mode = read32(kModeNow);
    if (mode) reinterpret_cast<ModeMsgFn>(uintptr_t(read32(read32(mode) + kModeMsgSlot)))(mode, kEndDragMsg, 0, 0, 0);
    reinterpret_cast<ShowFn>(uintptr_t(kShowWindow))(kWindowMgr, kCountWindow, 0);
    if (s.told++ < 30) LOG_INFO("asks: the game's «how many» given up - its drag ended, its window hidden");
    s.key.clear();
}

// 3.0.15-beta.7 (a player: «можно убрати щоб сразу 9209 само ставилось ... приходиться стирать в ручну 9209 кожен
// раз»): the game's count field set - emptied as the card shows, the most put back when OK finds it empty
void setCountField(uint32_t window, const std::string& text) {
    const uint32_t edit = window ? read32(window + kEdit) : 0;
    if (!edit) return;
    reinterpret_cast<SetTextFn>(uintptr_t(read32(read32(edit) + kSetTextSlot)))(edit, text.c_str());
}

// 3.0.11-beta.155: a count's quick choice - the game's field set to it and its OK pressed
void quick(State& s, int value) {
    const uint32_t window = gameCount(), edit = window ? read32(window + kEdit) : 0;
    if (!edit) return;
    const std::string digits = std::to_string(value);
    reinterpret_cast<SetTextFn>(uintptr_t(read32(read32(edit) + kSetTextSlot)))(edit, digits.c_str());
    if (s.told++ < 30) LOG_INFO("asks: the game's «how many» - %d chosen, its OK", value);
    reinterpret_cast<SendMsgFn>(uintptr_t(kCountSend))(window, 0, kButtonMsg, 0x76, 0, 0);
    s.key.clear();
}

// Its button pressed: the game's window's own (message 6, 0x76 or 0x77) - the game does the rest
void press(State& s, ac::Button b) {
    if (s.kind == ac::Kind::Count && s.shown) {  // 3.0.11-beta.155: OK its own, cancel ours (it has none)
        const uint32_t window = gameCount();
        if (!window) return;
        if (b == ac::Button::Cancel || b == ac::Button::Decline) {
            cancelCount(s);
            return;
        }
        if (s.card.field.empty() && s.card.most > 0) setCountField(window, std::to_string(s.card.most));  // .7
        if (s.told++ < 30) LOG_INFO("asks: the game's «how many» - typed, OK (0x76)");
        reinterpret_cast<SendMsgFn>(uintptr_t(kCountSend))(window, 0, kButtonMsg, 0x76, 0, 0);
        s.key.clear();
        return;
    }
    ac::Kind kind = ac::Kind::Trade;
    const uint32_t window = gameAsk(kind);
    if (!window) return;
    const uint32_t send = read32(read32(window) + kSendMsgSlot);
    if (s.told++ < 30)
        LOG_INFO("asks: %s - %s (the game's button 0x%X)", nameOf(kind),
                 b == ac::Button::Accept ? "accepted" : b == ac::Button::Decline ? "declined" : "typed, OK",
                 unsigned(ac::gameButton(b)));
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kButtonMsg, ac::gameButton(b), 0, 0);
    s.key.clear();
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (const auto* t = std::get_if<packets::TradeRequest>(&e)) s.names[0] = Named{t->from, now};
    else if (const auto* p = std::get_if<packets::PartyInvite>(&e)) s.names[1] = Named{p->party, now};
    else if (const auto* g = std::get_if<packets::GuildInvite>(&e)) s.names[2] = Named{g->guild, now};
    else if (const auto* f = std::get_if<packets::FriendRequest>(&e)) s.names[3] = Named{f->from, now};
}

void frame(HWND game, bool on, bool counts, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("asks: [interface] asks = %d", on ? 1 : 0);
    }
    if (counts != s.counts) {
        s.counts = counts;
        s.key.clear();
        LOG_INFO("asks: [interface] counts = %d", counts ? 1 : 0);
    }
    const bool world = inGameWorld();
    ac::Kind kind = ac::Kind::Trade;
    uint32_t window = world ? gameAsk(kind) : 0;
    if (world) hold(s, window, kind, on);
    const uint32_t count = world ? gameCount() : 0;  // 3.0.11-beta.155: the «how many» window
    if (world) holdCount(s, count, counts);
    if (!on) window = 0;
    if (!window && counts && count && read32(count + kShown)) {
        window = count;
        kind = ac::Kind::Count;
    }
    if (!window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    if (!s.shown || window != s.window || kind != s.kind) {
        if (s.told++ < 30) LOG_INFO("asks: %s - the new card shows it", nameOf(kind));
        s.show = ac::Show{};
        if (kind == ac::Kind::Count) setCountField(window, "");  // 3.0.15-beta.7: typed from empty, not the most
    }
    s.window = window;
    s.kind = kind;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.padNow = uapad::runtime::look().shown;
    s.card = cardOf(s, kind, window);
    const ac::Texts t = texts(s);
    s.layout = ac::layout(s.card, targetWidth, targetHeight, s.scale, t);
    s.show.hover = -1;
    s.show.quickHover = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        s.show.hover = ac::buttonAt(s.layout, lx, ly);
        s.show.quickHover = ac::quickAt(s.layout, lx, ly);
    }
    const std::string key = std::to_string(int(kind)) + ';' + s.card.title + ';' + s.card.message + ';' +
                            s.card.field + (s.card.caret ? "|" : "") + ';' + std::to_string(s.layout.x) + ',' +
                            std::to_string(s.layout.y) + ';' + std::to_string(s.show.hover) + ',' +
                            std::to_string(s.show.quickHover) + (s.padNow ? "P" : "") + '#' +
                            std::to_string(uistyle::generation());
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
    (void)wp;
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        const int q = ac::quickAt(s.layout, lx, ly);
        if (q >= 0 && s.kind == ac::Kind::Count) {  // 3.0.11-beta.155: one, half, all - set and OK
            quick(s, ac::quickValue(ac::Quick(q), s.card.most));
            return true;
        }
        const int i = ac::buttonAt(s.layout, lx, ly);
        const std::vector<ac::Button> bs = ac::buttons(s.kind);
        if (i >= 0 && i < int(bs.size())) press(s, bs[size_t(i)]);
    }
    return true;
}

bool shown() { return state().shown; }

bool asking() {
    const State& s = state();
    return s.shown && (ac::request(s.kind) || s.kind == ac::Kind::Count);
}

bool close() {
    State& s = state();
    if (!s.shown || (!ac::request(s.kind) && s.kind != ac::Kind::Count)) return false;
    press(s, s.kind == ac::Kind::Count ? ac::Button::Cancel : ac::Button::Decline);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!inGameWorld()) return;
    if (s.counts) holdCount(s, gameCount(), true);
    if (!s.on) return;
    ac::Kind kind = ac::Kind::Trade;
    if (const uint32_t window = gameAsk(kind)) hold(s, window, kind, true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown || (!ac::request(s.kind) && s.kind != ac::Kind::Count)) return;
    if (s.kind == ac::Kind::Count)  // 3.0.11-beta.155: A the count the field has (the most at first), B gives up
        press(s, c == PadCommand::Accept ? ac::Button::Ok : ac::Button::Cancel);
    else
        press(s, c == PadCommand::Accept ? ac::Button::Accept : ac::Button::Decline);
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

}  // namespace uagfx::win::askcardwin
