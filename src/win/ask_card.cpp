#include "win/ask_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/char_status.hpp"
#include "core/weight_fit.hpp"
#include "core/window_stack.hpp"
#include "core/ask_card.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/reference_load.hpp"
#include "win/cart_window.hpp"
#include "win/storage_window.hpp"
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
constexpr uint32_t kDragPlace = 0x330;  // 3.0.18-beta.156: the drag's thing's place (FUN_005707d0 0x3C: [0xCC])
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

// 3.0.18-beta.81 (item 4): the request or input card a live window - its message wrapped, its buttons a flex row
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw mw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<div class="msg" data-if="message != ''">{{message}}</div>
<div class="field" data-if="input">{{field}}<span class="caret" data-if="caret"></span></div>
<div class="quick" data-if="quick.size > 0"><div class="btn ctl" data-for="q, i : quick" data-event-click="ask_quick(i)">{{q}}</div></div>
<div class="btns"><div class="btn ctl" data-for="b, i : buttons" data-class-main="b.main" data-event-click="ask_press(i)">
  {{b.label}}<span class="key" data-if="b.key != ''">{{b.key}}</span></div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
struct AskButtonView {
    Rml::String label, key;
    bool main = false;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<AskButtonView> buttonViews;
    std::vector<Rml::String> quickViews;
    Rml::String title, message, field, ground = "#10141cf0";
    bool input = false, caret = false;
    uisheet::Sheet sheet;  // 3.0.18-beta.31: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
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

// 3.0.18-beta.156 (the owner: how many can be carried counted ahead, potions by weight): a stack taken into the bag
// (the count's purpose 6) - how many of it the bag's weight takes: the thing by the game mode's drag (its place
// +0x330) in the storage or the cart, its weight the item DB's (the reference pack); -1 unknown
int fitNow(uint32_t mode, uint32_t purpose) {
    if (!mode || purpose != 6) return -1;
    const int place = int(read32(mode + kDragPlace));
    int id = storagewin::idAtPlace(place);
    if (!id) id = cartwin::idAtPlace(place);
    const auto refs = reference::index();
    if (!id || !refs) return -1;
    const auto it = refs->items.find(id);
    charstatus::Status st;
    if (it == refs->items.end() || !readOwnStatus(st)) return -1;
    return weightfit::fits(st.weight, st.weightMost, it->second.weight);
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
        const int fit = fitNow(mode, read32(window + kCountFor));  // 3.0.18-beta.156: by the weight
        if (fit >= 0 && fit < c.most) {
            c.message = settings::fmt(say(settings::Text::CountFits), {std::to_string(fit), std::to_string(c.most)});
            c.most = fit;
        }
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

static void frameInner(HWND game, bool on, bool counts, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
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
    if (readHoverCursor(cx, cy)) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        s.show.hover = ac::buttonAt(s.layout, lx, ly);
        s.show.quickHover = ac::quickAt(s.layout, lx, ly);
    }
    const std::string key = std::to_string(int(kind)) + ';' + s.card.title + ';' + s.card.message + ';' +
                            s.card.field + (s.card.caret ? "|" : "") + ';' + std::to_string(s.layout.x) + ',' +
                            std::to_string(s.layout.y) + ';' + std::to_string(s.show.hover) + ',' +
                            std::to_string(s.show.quickHover) + (s.padNow ? "P" : "") + '#' +
                            std::to_string(uistyle::generation());
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.31
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("ask", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<AskButtonView>()) {
            v.RegisterMember("label", &AskButtonView::label);
            v.RegisterMember("key", &AskButtonView::key);
            v.RegisterMember("main", &AskButtonView::main);
        }
        m.RegisterArray<std::vector<AskButtonView>>();
        m.RegisterArray<std::vector<Rml::String>>();
        m.Bind("buttons", &s.buttonViews);
        m.Bind("quick", &s.quickViews);
        m.Bind("title", &s.title);
        m.Bind("message", &s.message);
        m.Bind("field", &s.field);
        m.Bind("input", &s.input);
        m.Bind("caret", &s.caret);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("ask_press", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            const std::vector<ac::Button> bs = ac::buttons(st.kind);
            if (!a.empty() && a[0].Get<int>() >= 0 && a[0].Get<int>() < int(bs.size()))
                press(st, bs[size_t(a[0].Get<int>())]);
        });
        m.BindEventCallback("ask_quick", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (!a.empty() && st.kind == ac::Kind::Count)
                quick(st, ac::quickValue(ac::Quick(std::clamp(a[0].Get<int>(), 0, 2)), st.card.most));
        });
    });
    if (!s.live.made()) return;
    const ac::Texts t = texts(s);
    std::vector<AskButtonView> views;
    for (const ac::Button b : ac::buttons(s.kind)) {
        AskButtonView v;
        v.label = b == ac::Button::Accept    ? t.accept
                  : b == ac::Button::Decline ? t.decline
                  : b == ac::Button::Cancel  ? t.cancel
                                             : t.ok;
        v.main = b != ac::Button::Decline && b != ac::Button::Cancel;
        v.key = b == ac::Button::Accept ? t.acceptKey : v.main ? t.okKey : t.declineKey;
        views.push_back(v);
    }
    std::vector<Rml::String> quick;
    if (s.kind == ac::Kind::Count) quick = {t.quick[0], t.quick[1], t.quick[2]};
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const bool sameButtons = views.size() == s.buttonViews.size() &&
                             std::equal(views.begin(), views.end(), s.buttonViews.begin(),
                                        [](const AskButtonView& a, const AskButtonView& b) {
                                            return a.label == b.label && a.key == b.key && a.main == b.main;
                                        });
    const bool input = ac::input(s.kind);
    if (!sameButtons || quick != s.quickViews || s.card.title != s.title || s.card.message != s.message ||
        s.card.field != s.field || input != s.input || s.card.caret != s.caret || ground != s.ground) {
        s.buttonViews = std::move(views), s.quickViews = quick, s.title = s.card.title, s.message = s.card.message;
        s.field = s.card.field, s.input = input, s.caret = s.card.caret, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    int x = s.layout.x, y = s.layout.y, bx = 0, by = 0, bw = 0, bh = 0;
    if (s.live.box(bx, by, bw, bh)) x = (s.targetW - bw) / 2, y = std::max(0, s.targetH / 3 - bh / 2);
    s.live.frame(s.shown, x, y, int(winstack::Win::Ask), ".btn");
}

void frame(HWND game, bool on, bool counts, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, counts, codePage, k, targetWidth, targetHeight);
    State& s = state();
    liveFrame(s);  // the old edition (the sheet) gone: the live window alone
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.live.shown) return;  // 3.0.18-beta.81
    s.live.draw(s.shadow, record, renderer, s.scale, s.k);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (s.live.shown) {  // 3.0.18-beta.81: RmlUi's
        if (!s.live.mouse(game, m, wp, lp)) return false;
        result = 0;
        return true;
    }
    return false;  // the old edition gone: the live window alone
}

bool countAll() {
    const uint32_t window = gameCount(), mode = read32(kModeNow);
    int most = mode ? int(read32(mode + kDragCount)) : 0;
    const int fit = window ? fitNow(mode, read32(window + kCountFor)) : -1;  // 3.0.18-beta.156: what the weight lets
    if (fit >= 0 && fit < most) most = fit;
    if (window && fit == 0) {  // nothing more goes in: the game's «how many» given up
        cancelCount(state());
        return true;
    }
    if (!window || most <= 0) return false;
    setCountField(window, std::to_string(most));
    reinterpret_cast<SendMsgFn>(uintptr_t(kCountSend))(window, 0, kButtonMsg, 0x76, 0, 0);
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
    if (s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.81
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::askcardwin
