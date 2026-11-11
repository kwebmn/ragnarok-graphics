#include "win/msg_box.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/ask_card.hpp"
#include "core/game_words.hpp"
#include "core/login_refusals.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/code_detour.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/login_card.hpp"
#include "win/login_refusal.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

namespace uagfx::win::msgboxwin {
namespace {

namespace ac = uagfx::askcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: ErrorMsg (thiscall, the manager; the words, the kind: 0 OK, 2 Yes and No, 4 OK and Cancel; three more)
// and the wait (the manager; the words, one more) - their first instructions, whole; the box at the manager's + 0x70,
// the error's class and the wait's, its SendMsg (message 6 with a button's id: the game's + 0x224 set, its wait ends);
// its Esc's answer + 0x58, its Enter's + 0x5C
constexpr uint32_t kErrorMsg = 0x4CEDF0, kWaitMsg = 0x4CEFA0, kWindowMgr = 0x727ED8, kBoxAt = kWindowMgr + 0x70;
const uint8_t kErrorCode[] = {0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00};
const uint8_t kWaitCode[] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF};
constexpr uint32_t kErrorClass = 0x697940, kWaitClass = 0x6978A8, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kEscAnswer = 0x58, kEnterAnswer = 0x5C;
// 3.0.11-beta.172: the words the game asks with to open ragnarok.co.kr (FUN_005A40C0); the answers ErrorMsg returns
constexpr uintptr_t kKoreanSiteAsk = 0x6D8258, kKoreanSiteAsk2 = 0x6D8210;
constexpr int kOkAnswer = 0x76, kNoAnswer = 0x77;
// 3.0.11-beta.173: msgstringtable's line by its number (cdecl, the line's words) and the «Failed to Connect» one
constexpr uint32_t kMsgString = 0x504FB0;
constexpr int kNoServerLine = 1;
using MsgStringFn = const char*(__cdecl*)(int line);
constexpr int kButtonMsg = 6;
// 3.0.11-beta.196 (the tester: «віконечко пліс вейт, теж треба в загальний стиль переробити» - and the ones «при зміні
// на "інший персонаж"»): the game's own «Please wait...» - UI window 6 (class 0x698370, MakeWindow puts it in the
// manager's list + 0x38 with no field of its own; its words msgstringtable's 0x79) - as the wait's card too
constexpr uint32_t kPleaseWaitClass = 0x698370, kWindowsList = kWindowMgr + 0x38;
constexpr int kPleaseWaitLine = 0x79;
constexpr size_t kMostWindows = 512;
using ErrorFn = int(__attribute__((thiscall)) *)(uint32_t manager, const char* text, int kind, int a, int b, int c);
using WaitFn = int(__attribute__((thiscall)) *)(uint32_t manager, const char* text, int a);

uintptr_t g_error = 0, g_wait = 0;  // the copies of their first instructions, then on to the rest

// 3.0.18-beta.74 (the big six, F): the box a live document - its message wrapped by RmlUi, its buttons a row by
// flexbox, RmlUi's clicks
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw mw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<div class="msg">{{message}}</div>
<div class="btns"><div class="btn ctl" data-for="b, i : buttons" data-class-main="b.main" data-event-click="press_btn(i)">
  {{b.label}}<span class="key" data-if="b.key != ''">{{b.key}}</span></div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";

struct ButtonView {
    Rml::String label, key;
    bool main = false;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.74
    std::vector<ButtonView> buttonViews;
    Rml::String title, message, ground = "#10141cf0";
    uisheet::Sheet sheet;  // 3.0.18-beta.37: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, hid = false, padNow = false;
    uint32_t waitHidden = 0;  // 3.0.18-beta.4: a «Please wait...» held hidden while the card shows another box
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::string text;  // the box's words as the game gave them (its code page)
    ac::Kind kind = ac::Kind::Message;
    uint32_t box = 0;
    ac::Card card;
    ac::Show show;
    ac::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    bool toldAgain = false;  // a call while its box is up logged (once a box)
    bool ownWords = false;   // 3.0.11-beta.172: `text` is the card's own (UTF-8), not the game's
};

State& state() {
    static State s;
    return s;
}

std::string copied(const char* text) {
    if (!text) return "";
    char buf[600] = {};
    if (!readGame(uint32_t(reinterpret_cast<uintptr_t>(text)), buf, sizeof buf - 1)) return "";
    buf[sizeof buf - 1] = 0;
    return buf;
}

int __attribute__((thiscall)) errorMsg(uint32_t manager, const char* text, int kind, int a, int b, int c) {
    State& s = state();
    // 3.0.11-beta.172: the game's offers to open ragnarok.co.kr (after three wrong passwords, and its code 0x10) -
    // answered «no» unseen: a yes there opens a Korean site and closes the game
    if (uintptr_t(text) == kKoreanSiteAsk || uintptr_t(text) == kKoreanSiteAsk2) {
        if (s.told++ < 30) LOG_INFO("messages: the game's offer to open ragnarok.co.kr - answered no unseen");
        return kNoAnswer;
    }
    // 3.0.11-beta.173: «Failed to Connect to Server.» for a login the new card took - answered unseen, the card's
    // status line says it and the login is sent again by itself
    if (loginwin::active() && !read32(kBoxAt) && text == reinterpret_cast<MsgStringFn>(kMsgString)(kNoServerLine)) {
        if (s.told++ < 30) LOG_INFO("messages: «Failed to Connect to Server.» for the login - on the card's line");
        loginwin::connectFailed();
        return kOkAnswer;
    }
    // 3.0.11-beta.172: the server's refusal of a login the new login card shows in words - its box answered unseen;
    // the others in the card's words
    const int refusal = loginrefusal::handlingCode();
    if (refusal >= 0 && loginwin::active() && !read32(kBoxAt)) {
        const loginerr::Refusal r = loginerr::refusal(refusal);
        if (r.inlineOnly) {
            if (s.told++ < 30)
                LOG_INFO("messages: the server's refusal (code %d) - in words on the login card", refusal);
            return kOkAnswer;
        }
        s.text = loginerr::words(r, language(), loginwin::account(),
                                 utf8Of(widened(loginrefusal::handlingUntil(), s.codePage)));
        s.ownWords = true;
        s.kind = ac::Kind::Message;
        s.toldAgain = false;
        if (s.told++ < 30) LOG_INFO("messages: the server's refusal (code %d) - the box in the card's words", refusal);
        return reinterpret_cast<ErrorFn>(g_error)(manager, text, kind, a, b, c);
    }
    const std::string words = copied(text);
    const bool up = read32(kBoxAt) != 0;
    // 3.0.11-beta.170 (R-KD2UQQ): a box up already - the game adds the line only when the call asks (b), else it
    // answers the call at once (its login asks again each turn while «Failed to Connect to Server.» is up)
    s.text = ac::boxWords(s.text, words, up, b != 0);
    if (!up) s.ownWords = false;
    if (!up) {
        s.kind = kind == 2 ? ac::Kind::Question : kind == 4 ? ac::Kind::Choice : ac::Kind::Message;
        s.toldAgain = false;
        if (s.told++ < 30) LOG_INFO("messages: the game's box (kind %d) - «%s»", kind, words.c_str());
    } else if (b) {
        if (s.told++ < 30) LOG_INFO("messages: a line added to the game's box - «%s»", words.c_str());
    } else if (!s.toldAgain) {
        s.toldAgain = true;
        LOG_INFO("messages: the game asks for its box again while it is up («%s») - answered at once, as the game does",
                 words.c_str());
    }
    return reinterpret_cast<ErrorFn>(g_error)(manager, text, kind, a, b, c);
}

int __attribute__((thiscall)) waitMsg(uint32_t manager, const char* text, int a) {
    State& s = state();
    if (!read32(kBoxAt)) {
        s.text = copied(text);
        s.ownWords = false;
        s.kind = ac::Kind::Wait;
        if (s.told++ < 30) LOG_INFO("messages: the game's wait - «%s»", s.text.c_str());
    }
    return reinterpret_cast<WaitFn>(g_wait)(manager, text, a);
}

// The game's box up now: the error's or the wait's
uint32_t gameBox(bool& wait) {
    const uint32_t b = read32(kBoxAt);
    if (!b) return 0;
    const uint32_t cls = read32(b);
    wait = cls == kWaitClass;
    return cls == kErrorClass || cls == kWaitClass ? b : 0;
}

// The game's «Please wait...» window in its list, shown or held hidden by the card; 0 none
uint32_t pleaseWait() {
    const uint32_t head = read32(kWindowsList);
    size_t seen = 0;
    for (uint32_t node = head ? read32(head) : 0; node && node != head && seen < kMostWindows; node = read32(node)) {
        ++seen;
        const uint32_t w = read32(node + 8);
        if (w && read32(w) == kPleaseWaitClass) return w;
    }
    return 0;
}

std::string utf8(const State& s, const std::string& game) { return utf8Of(widened(game, s.codePage)); }

ac::Texts texts(const State& s) {
    ac::Texts t;
    t.ok = say(settings::Text::AskOk);
    t.yes = say(settings::Text::MsgYes);
    t.no = say(settings::Text::MsgNo);
    t.cancel = say(settings::Text::MsgCancel);
    t.acceptKey = s.padNow ? uapad::buttonLabel(0) : "Enter";
    t.okKey = s.padNow ? uapad::buttonLabel(0) : "Enter";
    t.declineKey = s.padNow ? uapad::buttonLabel(1) : "Esc";
    return t;
}

void render(State& s) {
    ++s.sheetGen;
}

// The message's rows at the card's width, word by word as GDI measures them (its own line breaks kept)
int rowsOf(const State& s, const std::string& utf8Text) {
    const float width = ac::messageWidth(s.scale), px = 13 * s.scale;
    int rows = 0;
    size_t at = 0;
    while (at <= utf8Text.size()) {
        size_t end = utf8Text.find('\n', at);
        if (end == std::string::npos) end = utf8Text.size();
        const std::string line = utf8Text.substr(at, end - at);
        float x = 0;
        int lineRows = 1;
        size_t from = 0;
        while (from < line.size()) {
            size_t sp = line.find(' ', from);
            sp = sp == std::string::npos ? line.size() : sp + 1;
            const float ww = float(wordsWidth(utf8Wide(line.substr(from, sp - from)), px, false));
            if (x > 0 && x + ww > width) {
                ++lineRows;
                x = 0;
            }
            x += ww;
            from = sp;
        }
        rows += lineRows;
        at = end + 1;
        if (end == utf8Text.size()) break;
    }
    return std::max(1, rows);
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // 3.0.18-beta.74: the live box's own
    if (s.live.box(lx, ly, lw, lh)) x0 = float(lx), y0 = float(ly), b.w = float(lw), b.h = float(lh);
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(Blocker::Message, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Message, nullptr, nullptr, false);
    s.shown = false;
}

void pressId(State& s, int id, const char* why) {
    bool wait = false;
    const uint32_t b = gameBox(wait);
    if (!b || wait || !id) return;
    const uint32_t send = read32(read32(b) + kSendMsgSlot);
    if (s.told++ < 30) LOG_INFO("messages: the box answered 0x%X (%s)", unsigned(id), why);
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(b, 0, kButtonMsg, id, 0, 0);
    s.key.clear();
}

void press(State& s, ac::Button b) { pressId(s, ac::gameButton(b), "its button"); }

}  // namespace

void install() {
    static bool done = false;
    if (done) return;
    done = true;
    g_error = codedetour::detour(kErrorMsg, kErrorCode, sizeof kErrorCode, reinterpret_cast<const void*>(&errorMsg));
    g_wait = codedetour::detour(kWaitMsg, kWaitCode, sizeof kWaitCode, reinterpret_cast<const void*>(&waitMsg));
    LOG_INFO("messages: the game's message box's words %s, its wait's %s",
             g_error ? "caught" : "not caught (another build)", g_wait ? "caught" : "not caught (another build)");
}

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("messages: [interface] messages = %d", on ? 1 : 0);
    }
    bool wait = false;
    uint32_t box = g_error || g_wait ? gameBox(wait) : 0;
    // 3.0.18-beta.4 (the tester: «все ще трапляються старі «Please wait...», на аккаунтах на які раніше не заходив»):
    // a box of the manager's and the list's «Please wait...» at once - the card takes the box, the wait stood in its
    // old look. Held hidden too, given back when the card is off.
    const uint32_t waiting = box ? pleaseWait() : 0;
    if (waiting && on && read32(waiting + kShown) && write32(waiting + kShown, 0)) {
        if (s.waitHidden != waiting && s.told++ < 30)
            LOG_INFO("messages: the game's «Please wait» behind another box - hidden as well");
        s.waitHidden = waiting;
    } else if (s.waitHidden && (!on || s.waitHidden != waiting)) {
        if (!on && s.waitHidden == pleaseWait()) write32(s.waitHidden + kShown, 1);
        s.waitHidden = 0;
    }
    if (!box && (box = pleaseWait()) != 0) {  // 3.0.11-beta.196: no box of the manager's - its «Please wait...»
        wait = true;
        s.text = copied(reinterpret_cast<MsgStringFn>(kMsgString)(kPleaseWaitLine));
        s.ownWords = false;
        s.kind = ac::Kind::Wait;
        if (box != s.box && s.told++ < 30) LOG_INFO("messages: the game's «Please wait» window (UI window 6) - «%s»",
                                                    s.text.c_str());
    }
    if (box) {  // the game's box hidden while ours shows; back when ours is off
        if (on && read32(box + kShown) && write32(box + kShown, 0)) s.hid = true;
        else if (!on && s.hid && write32(box + kShown, 1)) s.hid = false;
    } else {
        s.hid = false;
    }
    if (!on || !box || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || s.text.empty()) {
        hide(s);
        s.box = 0;
        return;
    }
    if (!s.shown || box != s.box) s.show = ac::Show{};
    s.box = box;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.padNow = uapad::runtime::look().shown;
    s.card = ac::Card{};
    s.card.kind = wait ? ac::Kind::Wait : s.kind;
    s.card.title = say(wait ? settings::Text::MsgWaitTitle : settings::Text::MsgTitle);
    // 3.0.11-beta.218 (core/game_words): the game's own Korean ones in the player's language, its colour marks out
    const std::string known = s.ownWords ? std::string() : gamewords::known(s.text, language());
    s.card.message = s.ownWords ? s.text : !known.empty() ? known : utf8(s, gamewords::withoutColours(s.text));
    s.card.messageLines = rowsOf(s, s.card.message);
    s.layout = ac::layout(s.card, targetWidth, targetHeight, s.scale, texts(s));
    s.show.hover = -1;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        s.show.hover = ac::buttonAt(s.layout, float(cx) * float(k) - float(s.layout.x),
                                    float(cy) * float(k) - float(s.layout.y));
    const std::string key = std::to_string(int(s.card.kind)) + ';' + s.card.message + ';' +
                            std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' +
                            std::to_string(s.show.hover) + (s.padNow ? "P" : "") + '#' +
                            std::to_string(uistyle::generation());
    if (const std::string drawn = key + (uaui::readyAnywhere() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.37
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.74: the live box's model and place (at the middle across, a third down, by its own size)
static void liveFrame(State& s) {
    s.live.make("message box", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<ButtonView>()) {
            v.RegisterMember("label", &ButtonView::label);
            v.RegisterMember("key", &ButtonView::key);
            v.RegisterMember("main", &ButtonView::main);
        }
        m.RegisterArray<std::vector<ButtonView>>();
        m.Bind("buttons", &s.buttonViews);
        m.Bind("title", &s.title);
        m.Bind("message", &s.message);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("press_btn", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            const std::vector<ac::Button> bs = ac::buttons(st.card.kind);
            if (!a.empty() && a[0].Get<int>() >= 0 && a[0].Get<int>() < int(bs.size()))
                press(st, bs[size_t(a[0].Get<int>())]);
        });
    });
    if (!s.live.made()) return;
    const ac::Texts t = texts(s);
    std::vector<ButtonView> views;
    for (const ac::Button b : ac::buttons(s.card.kind)) {
        ButtonView v;
        v.label = b == ac::Button::Accept    ? t.accept
                  : b == ac::Button::Decline ? t.decline
                  : b == ac::Button::Yes     ? t.yes
                  : b == ac::Button::No      ? t.no
                  : b == ac::Button::Cancel  ? t.cancel
                                             : t.ok;
        v.main = b != ac::Button::Decline && b != ac::Button::No && b != ac::Button::Cancel;
        v.key = v.main ? (b == ac::Button::Ok ? t.okKey : t.acceptKey) : t.declineKey;
        views.push_back(v);
    }
    const bool same = views.size() == s.buttonViews.size() &&
                      std::equal(views.begin(), views.end(), s.buttonViews.begin(),
                                 [](const ButtonView& a, const ButtonView& b) {
                                     return a.label == b.label && a.key == b.key && a.main == b.main;
                                 });
    if (!same) s.buttonViews = std::move(views), s.live.doc.dirty("buttons");
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    if (s.card.title != s.title || s.card.message != s.message || ground != s.ground) {
        s.title = s.card.title, s.message = s.card.message, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    int x = s.layout.x, y = s.layout.y, bx = 0, by = 0, bw = 0, bh = 0;
    if (s.live.box(bx, by, bw, bh)) x = (s.targetW - bw) / 2, y = std::max(0, s.targetH / 3 - bh / 2);
    s.live.frame(s.shown, x, y, int(winstack::Win::Message), ".btn");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    // 3.0.18-beta.78 (R-8WH7KM: «Please wait…» and the login's errors not seen - the box drawn by uaUI in the world
    // alone, its picture outside it gone with beta.75): anywhere, the login's screens too
    s.live.anywhere = true;
    s.sheet.anywhere = true;
    liveFrame(s);  // the old edition (the sheet) gone: the live window alone
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.live.shown) return;  // 3.0.18-beta.74
    s.live.draw(s.shadow, record, renderer, s.scale, s.k);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    if (s.live.shown) {  // 3.0.18-beta.74: RmlUi's
        if (!s.live.mouse(game, m, wp, lp)) return false;
        result = 0;
        return true;
    }
    return false;  // the old edition gone: the live window alone
}

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (!s.shown || s.card.kind == ac::Kind::Wait || m != WM_KEYDOWN || (wp != VK_RETURN && wp != VK_ESCAPE))
        return false;
    bool wait = false;
    const uint32_t b = gameBox(wait);
    if (!b || wait) return false;
    pressId(s, int(read32(b + (wp == VK_RETURN ? kEnterAnswer : kEscAnswer))), wp == VK_RETURN ? "Enter" : "Esc");
    return true;
}

bool shown() { return state().shown; }

bool asking() {  // 3.0.11-beta.203: a box that asks or tells (not the wait's card)
    const State& s = state();
    return s.shown && s.card.kind != ac::Kind::Wait;
}

bool close() {
    State& s = state();
    if (!s.shown || s.card.kind == ac::Kind::Wait) return false;
    bool wait = false;
    const uint32_t b = gameBox(wait);
    if (!b || wait) return false;
    pressId(s, int(read32(b + kEscAnswer)), "Esc");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    bool wait = false;
    if (!s.on) return;
    uint32_t b = gameBox(wait);
    if (!b) b = pleaseWait();  // 3.0.11-beta.196
    if (b && read32(b + kShown) && write32(b + kShown, 0)) s.hid = true;
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const std::vector<ac::Button> bs = ac::buttons(s.card.kind);
    const size_t i = c == PadCommand::First ? 0 : 1;
    if (i < bs.size()) press(s, bs[i]);
    else if (c == PadCommand::Second) close();
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    if (s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.74
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::msgboxwin
