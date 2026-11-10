#include "win/login_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

#include "core/enter_veil.hpp"
#include "core/accounts.hpp"
#include "core/login_card.hpp"
#include "core/site_signin.hpp"
#include "core/login_chrome.hpp"
#include "core/reference_live.hpp"
#include "core/server_status.hpp"
#include "core/shelf_toast.hpp"
#include "core/login_hints.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/accounts_store.hpp"
#include "win/char_select.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/msg_box.hpp"
#include "win/game_logins.hpp"
#include "win/login_continue.hpp"
#include "win/login_refusal.hpp"
#include "win/login_shelf.hpp"
#include "win/open_page.hpp"
#include "win/site_accounts.hpp"
#include "win/site_key.hpp"
#include "win/site_signin.hpp"
#include "win/paths.hpp"
#include "win/reference_load.hpp"
#include "win/server_probe.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::loginwin {
namespace {

namespace lc = uagfx::logincard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the login window - where the manager keeps it, its class's vtable, its SendMsg (message 6 with a button's
// id); its two fields (an edit control each: its words FUN_00443300, their count FUN_00443330 - thiscall; the letter it
// draws in their place + 0x50 - the password's '*'), «Запам'ятати»; the manager's control with the keys (+ 0x6C) and
// its SetFocus. 3.0.11-beta.170 (R-4TNJP8): the account + 0x60, the password + 0x64 - told by the mask, not assumed
constexpr uint32_t kWindowMgr = 0x727ED8, kLoginAt = kWindowMgr + 0x88, kLoginClass = 0x697A70, kLoginSend = 0x457280;
constexpr uint32_t kFirstField = 0x60, kSecondField = 0x64, kMaskAt = 0x50;
constexpr uint32_t kRemember = 0x68, kRect = 0x14, kSendMsgSlot = 0x94;
constexpr uint32_t kGetText = 0x443300, kTextLength = 0x443330, kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510;
constexpr int kButtonMsg = 6;
// 3.0.11-beta.173: the game's chosen login server (FUN_004F3A10 from sclientinfo's connection): its address and port
// words; an edit's SetText (its vtable's slot)
constexpr uint32_t kServerAddress = 0x6BD370, kServerPort = 0x6BD374, kSetTextSlot = 0x98;
constexpr ULONGLONG kRetryForMs = 10 * 60 * 1000;  // a login not answered tried again for 10 minutes at most
constexpr int kAway = -4000;
constexpr ULONGLONG kBlinkMs = 530;
const wchar_t kRegister[] = L"https://uaro.kiev.ua/account/register";
const wchar_t kForgot[] = L"https://uaro.kiev.ua/account/forgot";  // 3.0.11-beta.172: a refused password's action
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using LengthFn = int(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);
using SetTextFn = void(__attribute__((thiscall)) *)(uint32_t edit, const char* text);

// 3.0.11-beta.203: the veil while the login enters the game by itself (core/enter_veil)
struct Veil {
    bool on = false;
    ULONGLONG since = 0;
    enterveil::Texts texts;
    enterveil::Layout layout;
    float k = 0;
    int w = 0, h = 0, phase = -1;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
};

struct State {
    // 3.0.18-beta.69 (the big six, B): the veil and the shelf's note drawn by uaUI over its windows
    uisheet::Sheet veilSheet, toastSheet;
    bool veilShown = false, toastShown = false;
    int veilGen = 0, toastGen = 0;
    uisheet::Sheet sheet;  // 3.0.18-beta.33: the card drawn by uaUI
    uisheet::Sheet chromeSheet;  // 3.0.18-beta.44: the strip at the screen's foot
    int chromeGen = 0;
    bool chromeShown = false;
    int sheetGen = 0;
    bool sheetShown = false;
    Veil veil;  // 3.0.11-beta.203
    WindowShadow shadow;
    bool on = false, shown = false, moved = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    int oldX = 0;
    uint32_t window = 0;
    lc::FieldPlaces places;
    bool toldPlaces = false, toldUnknown = false;
    lc::Fields fields;
    lc::Show show;
    lc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    // 3.0.11-beta.172: the server's refusal shown under its field until the player changes that field
    std::string lastTyped;  // the login last seen in the game's field, not empty (a refused one: its window gone)
    std::string error;
    loginerr::Field errorField = loginerr::Field::None;
    loginerr::Action errorAction = loginerr::Action::None;
    bool errorFresh = false;  // the window's fields as they were when the refusal came not seen yet
    std::string errorAccount;
    int errorPasswordLength = 0;
    // 3.0.11-beta.173: the card took the last login (the window seen with its fields known; the game closes it while
    // the server answers - the refusals and the failed connects come then)
    bool tookLast = false;
    // 3.0.11-beta.173: the server's status line and the login tried again after a failed connect
    bool status = true;
    std::string seenAccount, seenPassword;  // the game's fields' words as last seen (its code page) - never logged
    std::string tryAccount, tryPassword;    // the login sent (the window gone since)
    bool hadWindow = false, connectFailed = false;
    ULONGLONG failedAt = 0, firstFailAt = 0, probeAt = 0;
    loginchrome::Layout chrome;
    loginchrome::Show chromeShow;
    std::string chromeKey;
    panel::Picture chromePicture;
    uint32_t chromeTexture = 0;
    int chromeTexW = 0, chromeTexH = 0;
    bool chromeUpload = false;
    bool news = true;     // 3.0.11-beta.174: [interface] login_news
    std::string newsUrl;  // the item's page on the site (empty: its click does nothing)
    // 3.0.11-beta.175: the shelf - the login the card sent waits for the character select (accepted: on the shelf),
    // then the character it enters the world with goes on its row
    bool pendingShelf = false, selectWasUp = false, recordPending = false;
    std::string loggedIn;  // the accepted login (the game's words)
    // 3.0.11-beta.176: the login a shelf row sent with one click, and «Одразу в гру»'s character (UTF-8; "" the select)
    std::string oneClick, autoName;
    // 3.0.11-beta.176-177: what shows in the login window's place - the card, «Полиця», «Продовжити»
    enum class Mode : uint8_t { Card, Shelf, Continue, Site };  // 3.0.11-beta.180: the site sign-in card
    Mode mode = Mode::Card;
    loginview::View view = loginview::View::Shelf;  // the view now ([interface] login_view, or picked on the strip)
    std::optional<loginview::View> viewPicked;       // picked on the strip until the settings say the same
    bool eyeHeld = false;                            // 3.0.11-beta.177: the password's letters shown while held
    ULONGLONG exitAskedAt = 0;                       // 3.0.11-beta.179: the pad's B once - «Вийти» on the next
    bool makeCharAfter = false;  // 3.0.11-beta.181: registered - the first character made at the select
    // 3.0.11-beta.177: ▾'s saved logins under the account's field - their own picture
    bool dropOpen = false;
    lc::DropLayout drop;  // in the card's picture's pixels
    int dropHover = -1;
    std::vector<lc::DropRow> dropRows;
    std::string dropKey;
    panel::Picture dropPicture;
    uint32_t dropTexture = 0;
    int dropTexW = 0, dropTexH = 0;
    bool dropUpload = false;
    std::string toast, toastLogin;  // «kweb додано на полицю», the login «Скасувати» takes off
    ULONGLONG toastUntil = 0;
    float toastK = 0, toastScale = 1;
    int toastW = 0, toastH = 0;
    shelftoast::Layout toastLayout;
    bool toastHot = false, toastBlocked = false;
    std::string toastKey;
    panel::Picture toastPicture;
    uint32_t toastTexture = 0;
    int toastTexW = 0, toastTexH = 0;
    bool toastUpload = false;
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

uint32_t gameLogin() {
    const uint32_t w = read32(kLoginAt);
    return w && read32(w) == kLoginClass && read32(read32(w) + kSendMsgSlot) == kLoginSend ? w : 0;
}

// Which field is which: the one the game masks is the password's (none or both masked - not known)
lc::FieldPlaces placesOf(uint32_t window) {
    const uint32_t first = read32(window + kFirstField), second = read32(window + kSecondField);
    if (!first || !second) return lc::FieldPlaces{};
    return lc::fieldPlaces(kFirstField, read32(first + kMaskAt), kSecondField, read32(second + kMaskAt));
}

// Off the screen while ours shows (its fields keep the keys); back when ours is off
void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.moved = false;
        return;
    }
    const int32_t x = int32_t(read32(window + kRect));
    if (on && x != kAway) {
        s.oldX = x;
        write32(window + kRect, uint32_t(kAway));
        if (!s.moved)
            LOG_INFO("login: the game's login window off the screen for the new one (its fields keep the keys)");
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        s.moved = false;
    }
}

lc::Fields fieldsOf(const State& s, uint32_t window) {
    lc::Fields f;
    const uint32_t account = read32(window + s.places.account), password = read32(window + s.places.password);
    if (account) {
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(account);
        char buf[64] = {};
        if (t && readGame(uint32_t(reinterpret_cast<uintptr_t>(t)), buf, sizeof buf - 1)) {
            buf[sizeof buf - 1] = 0;
            f.account = utf8Of(widened(buf, s.codePage));
        }
    }
    if (password) f.passwordLength = reinterpret_cast<LengthFn>(uintptr_t(kTextLength))(password);
    const uint32_t focus = read32(kFocus);
    f.focus = focus && focus == account    ? lc::Focus::Account
              : focus && focus == password ? lc::Focus::Password
                                           : lc::Focus::None;
    f.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
    f.remember = read32(window + kRemember) != 0;
    return f;
}

// The keyboard's layout as the hint names it: its language's short name
std::string layoutName() {
    const unsigned lang = PRIMARYLANGID(LOWORD(HandleToUlong(GetKeyboardLayout(0))));
    return lang == LANG_UKRAINIAN ? "УКР" : lang == LANG_RUSSIAN ? "РУС" : lang == LANG_BELARUSIAN ? "БЕЛ" : "?";
}

// 3.0.11-beta.172: the hint under the field the keys go to - Caps Lock on, Cyrillic in the login (its layout)
void hintOf(lc::Fields& f) {
    const bool caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
    const loginhint::Hint h = f.focus == lc::Focus::Account    ? loginhint::forAccount(f.account, caps)
                              : f.focus == lc::Focus::Password ? loginhint::forPassword(caps)
                                                               : loginhint::Hint{};
    if (h.kind == loginhint::Kind::None) return;
    f.hintField = f.focus == lc::Focus::Account ? loginerr::Field::Account : loginerr::Field::Password;
    f.hint = h.kind == loginhint::Kind::CapsLock
                 ? say(settings::Text::LoginHintCaps)
                 : settings::fmt(say(settings::Text::LoginHintLayout), {layoutName(), h.guess});
}

lc::Texts texts(loginview::View view) {
    lc::Texts t;
    t.title = "uaRO";
    t.subtitle = say(settings::Text::LoginSubtitle);
    t.account = say(settings::Text::LoginAccount);
    t.password = say(settings::Text::LoginPassword);
    t.remember = say(settings::Text::LoginRemember);
    t.enter = say(settings::Text::LoginEnter);
    t.exit = say(settings::Text::LoginExit);
    t.register_ = say(settings::Text::ShelfCreate);  // 3.0.11-beta.177: «Створити акаунт» | «Забули пароль?»
    t.forgot = say(settings::Text::LoginForgot);
    t.createAccount = say(settings::Text::LoginCreateAccount);
    t.resetPassword = say(settings::Text::LoginResetPassword);
    t.addShelf = say(settings::Text::LoginAddShelf);  // 3.0.11-beta.175
    t.shelf = say(view == loginview::View::Continue ? settings::Text::ContinueBack
                                                     : settings::Text::ShelfBack);  // 3.0.11-beta.176
    t.siteOffer = say(settings::Text::SiteOffer);  // 3.0.11-beta.180
    t.siteOut = say(settings::Text::SiteOut);
    return t;
}

std::vector<Words> gdiWords(const std::vector<lc::Words>& in) {
    std::vector<Words> out;
    for (const lc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == lc::Align::Right    ? DT_RIGHT
                                 : w.align == lc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    if (uaui::readyAnywhere()) {  // 3.0.18-beta.33: uaUI's card says it all again, no picture
        ++s.sheetGen;
        return;
    }
    const uicost::Paint cost("the login", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    lc::paint(s.picture, s.layout, s.fields, s.show);
    drawWords(s.picture, gdiWords(lc::words(s.layout, s.fields, s.show, texts(s.view))));
    s.upload = true;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int sx = 0, sy = 0, sw = 0, sh = 0;
    const bool other = s.mode == State::Mode::Shelf      ? loginshelfwin::box(sx, sy, sw, sh)
                       : s.mode == State::Mode::Continue ? logincontinuewin::box(sx, sy, sw, sh)
                       : s.mode == State::Mode::Site     ? sitesigninwin::box(sx, sy, sw, sh)
                                                         : false;
    if (other) {  // 3.0.11-beta.176-177: the shelf's or «Продовжити»'s place
        b = panel::Box{0, 0, float(sw), float(sh)};
        x0 = float(sx);
        y0 = float(sy);
    }
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(Blocker::Login, game, &r, false);
}

// 3.0.11-beta.174: the strip kept from the game's mouse - from its news pill (else its switches) to the screen's foot
void blockBar(const State& s, HWND game) {
    RECT client{};
    if (s.chrome.w <= 0 || s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) {
        uapad::gamewindow::setBlocked(Blocker::LoginBar, nullptr, nullptr, false);
        return;
    }
    const float top = float(s.chrome.y) + (s.chrome.news.w > 0 ? s.chrome.news.y : s.chrome.langSeg[0].y);
    const RECT r{0, LONG(int64_t(top) * client.bottom / s.targetH), client.right, client.bottom};
    uapad::gamewindow::setBlocked(Blocker::LoginBar, game, &r, false);
}

void hide(State& s) {
    if (s.shown) {
        uapad::gamewindow::setBlocked(Blocker::Login, nullptr, nullptr, false);
        uapad::gamewindow::setBlocked(Blocker::LoginBar, nullptr, nullptr, false);
    }
    s.shown = false;
}

// An edit's words as the game keeps them (its code page)
std::string rawText(uint32_t edit) {
    if (!edit) return {};
    const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
    char buf[64] = {};
    if (!t || !readGame(uint32_t(reinterpret_cast<uintptr_t>(t)), buf, sizeof buf - 1)) return {};
    buf[sizeof buf - 1] = 0;
    return buf;
}

void setText(uint32_t edit, const std::string& raw) {
    const uint32_t fn = edit ? read32(read32(edit) + kSetTextSlot) : 0;
    if (fn) reinterpret_cast<SetTextFn>(uintptr_t(fn))(edit, raw.c_str());
}

// 3.0.11-beta.173: the login the server could not be reached for, sent again - its words put back into the game's
// fields (none focused meanwhile: the focused field's words would come from the keys' buffer) and its Enter
// The game's login window's fields set (its own SetText: the keys go on as typed), the focus off them
void putFields(const State& s, uint32_t window, const std::string& account, const std::string& password) {
    reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, 0);
    setText(read32(window + s.places.account), account);
    setText(read32(window + s.places.password), password);
}

void pressEnter(uint32_t window) {
    reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)))(window, 0, kButtonMsg,
                                                                                 lc::gameButton(lc::Part::Enter), 0, 0);
}

void sendAgain(State& s, uint32_t window) {
    putFields(s, window, s.tryAccount, s.tryPassword);
    LOG_INFO("login: the server did not answer - the login sent again");
    pressEnter(window);
}

// The game's chosen login server: its address and port (after the server select; before it the exe's own)
bool serverOf(std::string& host, int& port) {
    char buf[96] = {};
    const uint32_t a = read32(kServerAddress), p = read32(kServerPort);
    if (!a || !readGame(a, buf, sizeof buf - 1)) return false;
    host = buf;
    char pb[16] = {};
    if (!p || !readGame(p, pb, sizeof pb - 1)) return false;
    port = std::atoi(pb);
    return !host.empty() && port > 0;
}

// The language's segment of the strip: «УКР», «РУС», «ENG»
constexpr settings::Lang kSegmentLang[3] = {settings::Lang::Uk, settings::Lang::Ru, settings::Lang::En};

int segmentOf(settings::Lang l) {
    for (int i = 0; i < 3; ++i)
        if (kSegmentLang[i] == l) return i;
    return 2;
}

// 3.0.11-beta.173: the server's status line (empty with [interface] login_status = 0)
srvstatus::Line statusLine(State& s) {
    if (!s.status) return srvstatus::Line{};
    const ULONGLONG now = GetTickCount64();
    if (now - s.probeAt >= 1000) {
        s.probeAt = now;
        std::string host;
        int port = 0;
        if (serverOf(host, port)) serverprobe::want(host, port);
    }
    const serverprobe::Result r = serverprobe::result();
    const auto online = reference::online();
    const int players = online && reflive::fresh(*online, (long long)(std::time(nullptr))) ? online->online : -1;
    const int since = int((now - s.failedAt) / 1000);
    const int retryIn = s.connectFailed ? std::max(1, srvstatus::kProbeSec - since) : 0;
    return srvstatus::line(s.connectFailed ? srvstatus::State::Down : r.state, r.pingMs, players, retryIn,
                           language());
}

// 3.0.11-beta.173: the strip at the screen's foot - the server's status; 3.0.11-beta.174: the language's «УКР / РУС /
// ENG» at its right
void chromeFrame(State& s, int targetWidth, int targetHeight) {
    const srvstatus::Line line = statusLine(s);
    loginchrome::Content c;
    c.status = line.text;
    c.viewLabel = say(settings::Text::LoginViewLabel);  // 3.0.11-beta.177: «Вигляд»
    c.viewNames[0] = say(settings::Text::LoginViewClassic);
    c.viewNames[1] = say(settings::Text::LoginViewShelf);
    c.viewNames[2] = say(settings::Text::LoginViewContinue);
    s.chromeShow.views = true;
    s.chromeShow.view = int(s.view);
    s.chromeShow.viewLabel = c.viewLabel;
    for (int i = 0; i < 3; ++i) s.chromeShow.viewNames[i] = c.viewNames[i];
    s.newsUrl.clear();
    if (s.news)  // 3.0.11-beta.174: the site's event or news now, in the player's language
        if (const auto f = reference::now()) {
            const std::optional<refnow::Now>& item = f->items[int(language())];
            if (item && refnow::current(*item, (long long)(std::time(nullptr)))) {
                c.news = item->title;
                c.allNews = say(settings::Text::LoginAllNews);
                s.newsUrl = item->url;
            }
        }
    s.chromeShow.status = line.text;
    s.chromeShow.statusRgb = line.rgb;
    s.chromeShow.lang = segmentOf(language());
    s.chromeShow.news = c.news;
    s.chromeShow.allNews = c.allNews;
    s.chrome = loginchrome::layout(targetWidth, targetHeight, s.scale, c);
    s.chromeShow.hover = 0;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.chromeShow.hover = int(loginchrome::hitTest(s.chrome, float(cx) * s.k - float(s.chrome.x),
                                                      float(cy) * s.k - float(s.chrome.y)));
    const std::string key = line.text + '#' + std::to_string(line.rgb) + '#' + std::to_string(targetWidth) + 'x' +
                            std::to_string(targetHeight) + '#' + std::to_string(uistyle::generation()) + ';' +
                            std::to_string(s.chromeShow.lang) + ';' + std::to_string(s.chromeShow.hover) + '!' +
                            c.news + '%' + std::to_string(s.chromeShow.view) + c.viewNames[0];
    if (key + (uaui::readyAnywhere() ? "u" : "") == s.chromeKey) return;
    s.chromeKey = key + (uaui::readyAnywhere() ? "u" : "");
    ++s.chromeGen;  // 3.0.18-beta.44: uaUI's strip said again (no picture then)
    if (uaui::readyAnywhere()) return;
    if (!panel::blank(s.chromePicture, s.chrome.w, s.chrome.h)) return;
    loginchrome::paint(s.chromePicture, s.chrome, s.chromeShow);
    std::vector<Words> words;
    for (const loginchrome::Words& w : loginchrome::words(s.chrome, s.chromeShow))
        words.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                              UINT(w.align == loginchrome::Align::Center ? DT_CENTER : DT_LEFT), false});
    drawWords(s.chromePicture, words);
    s.chromeUpload = true;
}

// 3.0.11-beta.174: a language picked on the strip - kept in uaro-graphics.ini ([utility] language, as the settings
// app's choice) and our words in it at once; the game's own texts stay as they are
void chooseLanguage(int segment) {
    const settings::Lang l = kSegmentLang[std::clamp(segment, 0, 2)];
    if (l == language()) return;
    const bool written = editIniFile(ourFile(layout::File::Ini), {{"utility", "language", settings::langToken(l)}});
    setLanguage(l);
    LOG_INFO("login: the language %s picked%s", settings::langToken(l),
             written ? "" : " - uaro-graphics.ini not written: this game only");
}

// A page of the site in the player's browser (shell32 asked for only now: the driver's imports stay the four it has)
bool openUrl(const wchar_t* url) { return openPage(url); }

// 3.0.11-beta.174: the news line's click - the item's page, or the site's news in the player's language
void openNews(const State& s, bool all) {
    if (all) {
        const settings::Lang l = language();
        const wchar_t* url = l == settings::Lang::Uk   ? L"https://uaro.kiev.ua/uk/news"
                             : l == settings::Lang::Ru ? L"https://uaro.kiev.ua/ru/news"
                                                       : L"https://uaro.kiev.ua/en/news";
        LOG_INFO("login: the site's news opened%s", openUrl(url) ? "" : " - not possible here");
        return;
    }
    if (s.newsUrl.empty()) return;
    const bool opened = openUrl(utf8Wide(s.newsUrl).c_str());
    LOG_INFO("login: the news line's page opened%s", opened ? "" : " - not possible here");
}

// 3.0.11-beta.175: a password's letters gone from memory (never logged)
void wipe(std::string& t) {
    if (!t.empty()) SecureZeroMemory(t.data(), t.size());
    t.clear();
}

bool onShelf(const std::string& login) {
    const uint64_t h = accounts::loginHash(login);
    for (const accounts::Account& a : accountsstore::shelf().list)
        if (accounts::loginHash(a.login) == h) return true;
    return false;
}

// 3.0.11-beta.175: the login the card sent accepted (the character select came) - on the shelf when its tick is on
// (a login already there: its password kept fresh), «додано на полицю» for 6 s when new
void accepted(State& s) {
    s.pendingShelf = false;
    s.loggedIn = s.tryAccount;
    gamelogins::accepted(s.loggedIn);  // 3.0.11-beta.178: the other game windows' «вже в грі»
    accounts::Shelf& shelf = accountsstore::shelf();
    const bool there = onShelf(s.tryAccount);
    const unsigned long long hash = accounts::loginHash(s.tryAccount);
    if (shelf.addAfterLogin || there) {
        accounts::upsert(shelf, s.tryAccount, s.tryPassword, (long long)(std::time(nullptr)));
        const bool saved = accountsstore::save();
        LOG_INFO("login: the login %016llx %s the shelf%s", hash, there ? "renewed on" : "put on",
                 saved ? "" : " - not saved");
        if (saved && !there) {
            s.toast = settings::fmt(say(settings::Text::ShelfAdded), {utf8Of(widened(s.tryAccount, s.codePage))});
            s.toastLogin = s.tryAccount;
            s.toastUntil = GetTickCount64() + shelftoast::kShowMs;
        }
    }
    wipe(s.tryPassword);
    wipe(s.seenPassword);
}

// 3.0.11-beta.175: the character the accepted login entered the world with, on its shelf row (the map it came to)
void recordEntered(State& s, const std::string& map) {
    accounts::LastChar c = charselectwin::entered();
    if (c.slot < 0 || !onShelf(s.loggedIn)) return;
    c.map = map;
    accounts::recordChar(accountsstore::shelf(), s.loggedIn, c, (long long)(std::time(nullptr)));
    const bool saved = accountsstore::save();
    LOG_INFO("login: the shelf's %016llx entered with the character in slot %d on %s%s",
             (unsigned long long)accounts::loginHash(s.loggedIn), c.slot, map.c_str(), saved ? "" : " - not saved");
}

// 3.0.11-beta.175: «Скасувати» - the login just put on the shelf taken off again
void undoShelf(State& s) {
    const bool removed = accounts::remove(accountsstore::shelf(), s.toastLogin);
    if (removed) accountsstore::save();
    LOG_INFO("login: the login %016llx taken off the shelf again («Скасувати»)",
             (unsigned long long)accounts::loginHash(s.toastLogin));
    s.toastUntil = 0;
}

void press(State& s, lc::Part p) {
    if (p == lc::Part::Shelf) {  // 3.0.11-beta.176: «‹ Полиця» - the shelf again
        loginshelfwin::classic(false);
        s.key.clear();
        return;
    }
    if (p == lc::Part::AddShelf) {  // 3.0.11-beta.175: the shelf's own setting, kept with it
        accounts::Shelf& shelf = accountsstore::shelf();
        shelf.addAfterLogin = !shelf.addAfterLogin;
        const bool saved = accountsstore::save();
        LOG_INFO("login: «Додати на полицю після входу» %s%s", shelf.addAfterLogin ? "on" : "off",
                 saved ? "" : " - not saved");
        s.key.clear();
        return;
    }
    const uint32_t window = gameLogin();
    if (!window) return;
    if (p == lc::Part::ErrorAction) {  // 3.0.11-beta.172: what to do about a refusal
        const bool create = s.errorAction == loginerr::Action::CreateAccount;
        const bool opened = s.errorAction != loginerr::Action::None && openUrl(create ? kRegister : kForgot);
        LOG_INFO("login: the refusal's action - %s %s", create ? "the registration page" : "the password's reset",
                 opened ? "opened" : "not opened");
        return;
    }
    if (p == lc::Part::Account || p == lc::Part::Password) {  // the keys to that field
        const lc::FieldPlaces places = placesOf(window);
        if (!places.known()) return;
        const uint32_t edit = read32(window + (p == lc::Part::Account ? places.account : places.password));
        if (edit) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
        s.key.clear();
        return;
    }
    if (p == lc::Part::Register) {
        if (sitesigninwin::siteReady()) {  // 3.0.11-beta.181: the registration in the client
            sitesigninwin::openRegister();
            s.key.clear();
            return;
        }
        const bool opened = openUrl(kRegister);
        LOG_INFO("login: the registration page %s (uaro.kiev.ua/account/register)", opened ? "opened" : "not opened");
        return;
    }
    if (p == lc::Part::Site && !sitekey::token().empty()) p = lc::Part::None;  // the chip's words: nothing
    if (p == lc::Part::SiteOut && sitekey::token().empty()) p = lc::Part::Site;  // the offer's line, all of it
    if (p == lc::Part::Site) {  // 3.0.11-beta.180: the site sign-in card
        sitesigninwin::open();
        s.key.clear();
        return;
    }
    if (p == lc::Part::SiteOut) {
        sitekey::forget();
        LOG_INFO("site: signed out on the login screen");
        s.key.clear();
        return;
    }
    if (p == lc::Part::Forgot) {  // 3.0.11-beta.177
        const bool opened = openUrl(kForgot);
        LOG_INFO("login: the password's reset page %s", opened ? "opened" : "not opened");
        return;
    }
    if (p == lc::Part::Eye) {  // 3.0.11-beta.177: its letters while the button is held
        s.eyeHeld = true;
        s.key.clear();
        return;
    }
    if (p == lc::Part::Drop) {  // 3.0.11-beta.177: the saved logins
        s.dropOpen = !s.dropOpen;
        s.dropKey.clear();
        s.key.clear();
        return;
    }
    const int id = lc::gameButton(p);
    if (!id) return;
    const uint32_t send = read32(read32(window) + kSendMsgSlot);
    const int b = p == lc::Part::Remember ? (read32(window + kRemember) ? 0 : 1) : 0;  // the box: its new state
    if (p == lc::Part::Enter) s.error.clear();  // a new try: the last refusal gone
    if (s.told++ < 30)
        LOG_INFO("login: %s - the game's button 0x%X", p == lc::Part::Enter    ? "«Увійти»"
                                                       : p == lc::Part::Exit   ? "«Вийти»"
                                                                               : "«Запам'ятати логін»",
                 unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kButtonMsg, id, b, 0);
    s.key.clear();
}

// 3.0.11-beta.176: what the shelf asked - a card's login, the card for another one (empty fields) or for a card's
// login (its password typed anew), the site's registration, the game's «Вийти»
void act(State& s, const loginshelfwin::Asked& a) {
    using A = loginshelfwin::Act;
    accounts::Shelf& shelf = accountsstore::shelf();
    const uint32_t window = gameLogin();
    const shelfmerge::Row* r = loginshelfwin::row(a.index);  // 3.0.11-beta.181: this computer's or the site's
    accounts::Account account;
    if (r && r->local >= 0 && r->local < int(shelf.list.size())) {
        account = shelf.list[size_t(r->local)];
    } else if (r) {
        account.login = r->login;  // the site's alone: its password asked once
    }
    switch (a.act) {
        case A::Login:
        case A::CharLogin:
        case A::CharNew:
            if (r) loginWith(account, a.act == A::CharNew, a.act == A::CharLogin ? a.name : std::string());
            break;
        case A::GamePassword:
            if (r) sitesigninwin::openForm(int(sitesignin::Way::GamePassword), r->accountId, r->login);
            break;
        case A::NewLogin: sitesigninwin::openForm(int(sitesignin::Way::NewLogin), 0, std::string()); break;
        case A::LinkLogin: sitesigninwin::openForm(int(sitesignin::Way::Link), 0, std::string()); break;
        case A::Add:
            if (!shelf.addAfterLogin) {
                shelf.addAfterLogin = true;
                accountsstore::save();
            }
            [[fallthrough]];
        case A::Other:
            loginshelfwin::classic(true);
            if (window && s.places.known()) {
                putFields(s, window, std::string(), std::string());
                reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, read32(window + s.places.account));
            }
            LOG_INFO("login: the card for %s", a.act == A::Add ? "a login to add" : "another login");
            break;
        case A::Edit:
            loginshelfwin::classic(true);
            if (r && window && s.places.known()) {
                putFields(s, window, r->login, std::string());
                reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, read32(window + s.places.password));
            }
            break;
        case A::Create: press(s, lc::Part::Register); break;  // 3.0.11-beta.181: in the client when the site can
        case A::Exit: press(s, lc::Part::Exit); break;
        case A::None: break;
    }
    s.key.clear();
}

// 3.0.11-beta.177: what «Продовжити» asked - its account's login (with its character, or the select shown), the card
// for another login, the site's registration
void actContinue(State& s, const logincontinuewin::Asked& a) {
    using A = logincontinuewin::Act;
    accounts::Shelf& shelf = accountsstore::shelf();
    const bool row = a.index >= 0 && a.index < int(shelf.list.size());
    switch (a.act) {
        case A::Play:
        case A::OtherChar:
            if (row) loginWith(shelf.list[size_t(a.index)], a.act == A::OtherChar);
            break;
        case A::Other: act(s, loginshelfwin::Asked{loginshelfwin::Act::Other, -1, {}}); break;
        case A::Create: press(s, lc::Part::Register); break;  // 3.0.11-beta.181
        case A::None: break;
    }
    s.key.clear();
}

// 3.0.11-beta.177: a view picked on the strip - kept in uaro-graphics.ini ([interface] login_view), shown at once
void chooseView(State& s, loginview::View v) {
    if (v == s.view) return;
    const bool written =
        editIniFile(ourFile(layout::File::Ini), {{"interface", "login_view", loginview::token(v)}});
    s.viewPicked = v;
    s.view = v;
    loginshelfwin::classic(false);
    s.dropOpen = false;
    s.key.clear();
    LOG_INFO("login: the view %s picked%s", loginview::token(v),
             written ? "" : " - uaro-graphics.ini not written: this game only");
}

// 3.0.11-beta.177: ▾'s saved logins under the account's field (the card's picture's pixels), their own picture
void dropFrame(State& s) {
    const accounts::Shelf& shelf = accountsstore::shelf();
    s.dropRows.clear();
    for (const accounts::Account& a : shelf.list) {
        lc::DropRow r;
        r.login = utf8Of(widened(a.login, s.codePage));
        if (!a.last.name.empty())
            r.line = a.last.name + " · " + a.last.job + (a.last.level > 0 ? " " + std::to_string(a.last.level) : "");
        s.dropRows.push_back(r);
    }
    s.drop = lc::dropLayout(s.layout, s.dropRows.size());
    s.dropHover = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.dropHover = lc::dropHit(s.drop, float(cx) * s.k - float(s.layout.x), float(cy) * s.k - float(s.layout.y));
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' +
                      std::to_string(s.dropHover) + '#' + std::to_string(uistyle::generation());
    for (const lc::DropRow& r : s.dropRows) key += ';' + r.login + ',' + r.line;
    if (key == s.dropKey) return;
    s.dropKey = key;
    if (!panel::blank(s.dropPicture, int(s.drop.w + 0.5f), int(s.drop.h + 0.5f))) return;
    lc::paintDrop(s.dropPicture, s.drop, s.dropHover, s.scale);
    drawWords(s.dropPicture,
              gdiWords(lc::dropWords(s.drop, s.dropRows, s.dropHover, say(settings::Text::LoginOtherLogin), s.scale)));
    s.dropUpload = true;
}

// 3.0.11-beta.177: a saved login picked in ▾'s list - its login and password into the fields (not sent: «Увійти»
// is the player's); «+ Інший логін» - the fields empty
void dropPick(State& s, int row) {
    s.dropOpen = false;
    s.key.clear();
    const uint32_t window = gameLogin();
    const accounts::Shelf& shelf = accountsstore::shelf();
    if (!window || !s.places.known()) return;
    if (row >= 0 && row < int(shelf.list.size())) {
        const accounts::Account& a = shelf.list[size_t(row)];
        const bool password = !a.password.empty() && !a.refused;
        putFields(s, window, a.login, password ? a.password : std::string());
        reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, read32(window + s.places.password));
        LOG_INFO("login: ▾ - the shelf's %016llx put into the fields%s",
                 (unsigned long long)accounts::loginHash(a.login), password ? "" : " (its password to type)");
    } else {
        putFields(s, window, std::string(), std::string());
        reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, read32(window + s.places.account));
    }
}

// 3.0.11-beta.175: each frame, the login window there or not - the shelf's moments (the login accepted, the world
// entered) and its «додано» note's place, kept from the game's mouse while it shows
void shelfFrame(State& s, HWND game, double k, int targetWidth, int targetHeight) {
    if (s.veil.on) {  // 3.0.11-beta.203: the veil's end
        const ULONGLONG now = GetTickCount64();
        const enterveil::End end = enterveil::ending(!readGameMap().empty(), msgboxwin::asking(),
                                                     s.shown && now - s.veil.since > 1500, s.veil.since, now);
        if (end != enterveil::End::No) {
            s.veil.on = false;
            static const char* const kWhy[] = {"", "the map started", "a box asks", "the login card back", "30 s"};
            LOG_INFO("login: the veil off - %s after %llu ms", kWhy[int(end)],
                     static_cast<unsigned long long>(now - s.veil.since));
        }
        s.veil.k = float(k);
        s.veil.w = targetWidth;
        s.veil.h = targetHeight;
    }
    const bool selectUp = charselectwin::gameUp();
    if (selectUp && !s.selectWasUp) {
        if (s.pendingShelf) accepted(s);
        s.recordPending = !s.loggedIn.empty();
        if (s.makeCharAfter) {  // 3.0.11-beta.181: registered - «2 Персонаж»
            s.makeCharAfter = false;
            charselectwin::makeFirst();
            LOG_INFO("login: registered - the character make on the first empty place");
        }
        if (!s.autoName.empty() && s.oneClick == s.loggedIn) {  // 3.0.11-beta.176: «Одразу в гру»
            charselectwin::enterByName(s.autoName);
            LOG_INFO("login: «Одразу в гру» - the character select passed with the saved character");
        }
        s.oneClick.clear();
        s.autoName.clear();
    }
    s.selectWasUp = selectUp;
    if (s.recordPending && !selectUp)
        if (const std::string map = readGameMap(); !map.empty()) {
            s.recordPending = false;
            recordEntered(s, map);
        }
    s.toastK = float(k);
    s.toastScale = uistyle::readingScale(k);
    s.toastW = targetWidth;
    s.toastH = targetHeight;
    const bool showing = !s.toast.empty() && GetTickCount64() < s.toastUntil && k > 0 && targetWidth > 0;
    RECT client{};
    if (showing && game && GetClientRect(game, &client)) {
        s.toastLayout =
            shelftoast::layout(targetWidth, targetHeight, s.toastScale, s.toast, say(settings::Text::ShelfUndo));
        const shelftoast::Layout& L = s.toastLayout;
        const RECT r{LONG(int64_t(L.x) * client.right / targetWidth), LONG(int64_t(L.y) * client.bottom / targetHeight),
                     LONG(int64_t(L.x + L.w) * client.right / targetWidth + 1),
                     LONG(int64_t(L.y + L.h) * client.bottom / targetHeight + 1)};
        uapad::gamewindow::setBlocked(Blocker::ShelfToast, game, &r, false);
        s.toastBlocked = true;
    } else if (s.toastBlocked) {
        uapad::gamewindow::setBlocked(Blocker::ShelfToast, nullptr, nullptr, false);
        s.toastBlocked = false;
    }
    // 3.0.18-beta.69 (the big six, B): the veil - the screen shaded, its card with the dots lit by the phase - and
    // the shelf's note, on sheets of uaUI's over its windows
    s.veilSheet.anywhere = s.toastSheet.anywhere = true;
    s.veilSheet.top = s.toastSheet.top = true;
    const Veil& v = s.veil;
    const int phase = v.on ? int((GetTickCount64() - v.since) / 350 % 3) : 0;
    s.veilShown = s.veilSheet.frame("enter veil", v.on && k > 0 && targetWidth > 0, true, 0, 0,
                                    phase * 131 + targetWidth * 7 + targetHeight + (v.on ? 1 : 0),
                                    [&](uisheet::Content& c) {
                                        c.w = float(targetWidth), c.h = float(targetHeight);
                                        c.rect(c.under, panel::Box{0, 0, c.w, c.h}, 0, 0x000000, 0.94f);
                                        const enterveil::Layout L =
                                            enterveil::layout(targetWidth, targetHeight, uistyle::readingScale(k));
                                        const float sc = L.s, x = float(L.x), y = float(L.y);
                                        panel::Box card = L.card;
                                        card.x += x, card.y += y;
                                        c.rect(c.over, card, 14 * sc, 0x10141C, 0.97f, 0xFFFFFF, 0.14f,
                                               std::max(1.0f, sc));
                                        for (int i = 0; i < 3; ++i) {
                                            const float cx = x + L.dots.x + (7 + 15 * float(i)) * sc,
                                                        cy = y + L.dots.y + L.dots.h / 2, r = 3.2f * sc;
                                            c.rect(c.over, panel::Box{cx - r, cy - r, 2 * r, 2 * r}, r, 0xE6CB8A,
                                                   i <= phase ? 0.95f : 0.25f);
                                        }
                                        for (const enterveil::Words& w : enterveil::words(L, v.texts))
                                            c.words.push_back(uisheet::Word{
                                                x + w.box.x, y + w.box.y, w.box.w, w.box.h, w.px, w.box.h, w.text,
                                                uisheet::Content::rgba(w.rgb, 1),
                                                std::string(w.bold ? "word bold" : "word") +
                                                    (w.align == enterveil::Align::Left    ? ""
                                                     : w.align == enterveil::Align::Right ? " right"
                                                                                          : " centre")});
                                    });
    const ULONGLONG now = GetTickCount64();
    const bool toastOn = !s.toast.empty() && now < s.toastUntil && k > 0 && s.toastLayout.w > 0;
    if (toastOn) {
        int cx = 0, cy = 0;
        const shelftoast::Layout& L = s.toastLayout;
        s.toastHot = readGameCursor(cx, cy) &&
                     shelftoast::overUndo(L, float(cx) * float(k) - float(L.x), float(cy) * float(k) - float(L.y));
    }
    const float fade = std::clamp(float(s.toastUntil > now ? s.toastUntil - now : 0) / 400.0f, 0.0f, 1.0f);
    s.toastShown = s.toastSheet.frame(
        "shelf toast", toastOn, true, s.toastLayout.x, s.toastLayout.y,
        int(std::hash<std::string>()(s.toast)) ^ (s.toastHot ? 1 : 0) ^ int(fade * 20) * 977 ^ s.toastLayout.w,
        [&](uisheet::Content& c) {
            const shelftoast::Layout& L = s.toastLayout;
            c.w = float(L.w), c.h = float(L.h), c.op = fade;
            c.rect(c.under, L.pill, L.pill.h / 2, 0x10141C, 0.94f, 0xC9A55B, s.toastHot ? 0.75f : 0.55f, L.s);
            for (const shelftoast::Words& w : shelftoast::words(L, s.toast, say(settings::Text::ShelfUndo), s.toastHot))
                c.words.push_back(uisheet::Word{w.box.x, w.box.y, w.box.w, w.box.h, w.px, w.box.h, w.text,
                                                uisheet::Content::rgba(w.rgb, 1), w.bold ? "word bold" : "word"});
        });

}

}  // namespace

static void frameInner(HWND game, bool on, bool status, bool news, loginview::View view, unsigned codePage, double k,
           int targetWidth, int targetHeight) {
    State& s = state();
    if (s.viewPicked && *s.viewPicked == view) s.viewPicked.reset();  // the settings say the same now
    s.view = s.viewPicked.value_or(view);
    s.codePage = codePage;
    s.status = status;
    s.news = news;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("login: [interface] login = %d", on ? 1 : 0);
    }
    const uint32_t window = gameLogin();
    if (s.hadWindow && !window) {  // 3.0.11-beta.173: the window gone - the login sent (or the game left it)
        s.tryAccount = s.seenAccount;
        s.tryPassword = s.seenPassword;
        s.pendingShelf = s.tookLast && !s.tryAccount.empty();  // 3.0.11-beta.175: waits for the character select
        loginshelfwin::classic(false);  // 3.0.11-beta.176: the next login screen the shelf again
    }
    if (window && !s.hadWindow) {  // the window back: refused, not reached, or back from the game
        s.pendingShelf = false;
        s.makeCharAfter = false;
        if (!s.loggedIn.empty()) {  // 3.0.11-beta.178: this game plays no login now
            gamelogins::left();
            s.loggedIn.clear();
        }
    }
    s.hadWindow = window != 0;
    shelfFrame(s, game, k, targetWidth, targetHeight);
    s.places = window ? placesOf(window) : lc::FieldPlaces{};
    if (on && window && !s.places.known() && !s.toldUnknown) {
        s.toldUnknown = true;
        LOG_INFO("login: the game's fields not as known (no single one drawn as its mask) - its own window kept");
    }
    if (s.places.known() && !s.toldPlaces) {
        s.toldPlaces = true;
        LOG_INFO("login: the game's fields - the login + 0x%X, the password + 0x%X (drawn as «*»)",
                 unsigned(s.places.account), unsigned(s.places.password));
    }
    const bool take = on && s.places.known();
    if (!on) s.tookLast = false;
    if (take && window) s.tookLast = true;
    hold(s, window, take);
    if (!take || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    if (!s.shown || window != s.window) LOG_INFO("login: the game's login - the new one shows it");
    s.window = window;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.fields = fieldsOf(s, window);
    s.fields.addShelf = accountsstore::shelf().addAfterLogin;  // 3.0.11-beta.175
    s.seenAccount = rawText(read32(window + s.places.account));
    s.seenPassword = rawText(read32(window + s.places.password));
    if (s.connectFailed) {  // 3.0.11-beta.173: the server not reached - its login tried again every 8 s
        const ULONGLONG now = GetTickCount64();
        const bool retyped = !s.seenAccount.empty() && s.seenAccount != s.tryAccount;
        if (retyped || now - s.firstFailAt > kRetryForMs || s.tryAccount.empty() || s.tryPassword.empty()) {
            s.connectFailed = false;
            LOG_INFO("login: the login not tried again any more (%s)", retyped ? "typed anew" : "given up");
        } else if (srvstatus::retryLogin(true, true, int((now - s.failedAt) / 1000))) {
            s.connectFailed = false;
            sendAgain(s, window);
            return;
        }
    }
    if (const auto seen = loginrefusal::take()) {  // 3.0.11-beta.172: the server's refusal - under its field
        const loginerr::Refusal r = loginerr::refusal(seen->code);
        // 3.0.11-beta.176: a shelf row's password refused - not sent again by itself until a login goes through
        if (r.field == loginerr::Field::Password && !s.oneClick.empty() &&
            accounts::markRefused(accountsstore::shelf(), s.oneClick)) {
            accountsstore::save();
            LOG_INFO("login: the shelf's %016llx - its password refused, asked again",
                     (unsigned long long)accounts::loginHash(s.oneClick));
        }
        if (!s.oneClick.empty()) loginshelfwin::classic(true);  // 3.0.11-beta.176: the card, its password asked
        s.oneClick.clear();
        s.autoName.clear();
        if (r.inlineOnly) {
            s.error = loginerr::words(r, language(), s.lastTyped, utf8Of(widened(seen->until, s.codePage)));
            s.errorField = r.field;
            s.errorAction = r.action;
            s.errorFresh = true;
        }
    }
    if (!s.error.empty() && s.errorFresh) {  // the window as it came back: what the player changes clears it
        s.errorFresh = false;
        s.errorAccount = s.fields.account;
        s.errorPasswordLength = s.fields.passwordLength;
    } else if (!s.error.empty() && (s.fields.account != s.errorAccount ||
                                    s.fields.passwordLength != s.errorPasswordLength)) {
        s.error.clear();
    }
    if (!s.fields.account.empty()) s.lastTyped = s.fields.account;
    s.fields.error = s.error;
    s.fields.errorField = s.error.empty() ? loginerr::Field::None : s.errorField;
    s.fields.errorAction = s.errorAction;
    hintOf(s.fields);  // 3.0.11-beta.172: Caps Lock, the layout - under the field typed in
    // 3.0.11-beta.176-177: the card, «Полиця» or «Продовжити» - the shelf empty, or the card asked for one login:
    // the card whichever view
    const accounts::Shelf& shelf = accountsstore::shelf();
    const bool any = !shelf.list.empty(), forOne = loginshelfwin::classicChosen();
    sitesigninwin::probe();  // 3.0.11-beta.180: the site's client API there? (asked once; again while not)
    if (sitesigninwin::takeSignedIn()) siteaccounts::refresh(true);  // 3.0.11-beta.181: the shelf from the site
    {
        std::string login, password;
        if (sitesigninwin::takeRegistered(login, password) && window) {  // the new login sent, its character next
            accounts::Account a;
            a.login = login;
            a.password = password;
            s.makeCharAfter = true;
            loginWith(a, true);
            wipe(password);
            wipe(a.password);
            siteaccounts::refresh(true);
        }
    }
    const State::Mode mode = sitesigninwin::isOpen()                                    ? State::Mode::Site
                             : !any || forOne || s.view == loginview::View::Classic ? State::Mode::Card
                             : s.view == loginview::View::Shelf                    ? State::Mode::Shelf
                                                                                   : State::Mode::Continue;
    if (mode != s.mode) {
        s.mode = mode;
        s.key.clear();
        s.dropOpen = false;
        LOG_INFO("login: %s shown", mode == State::Mode::Shelf      ? "«Полиця»"
                                    : mode == State::Mode::Continue ? "«Продовжити»"
                                    : mode == State::Mode::Site     ? "the site sign-in card"
                                                                    : "the login card");
    }
    if (mode != State::Mode::Card) {
        if (mode == State::Mode::Shelf)
            loginshelfwin::frame(k, targetWidth, targetHeight, codePage);
        else if (mode == State::Mode::Continue)
            logincontinuewin::frame(k, targetWidth, targetHeight, codePage);
        else
            sitesigninwin::frame(k, targetWidth, targetHeight);
        chromeFrame(s, targetWidth, targetHeight);
        s.shown = true;
        block(s, game);
        blockBar(s, game);
        return;
    }
    if (s.eyeHeld && GetAsyncKeyState(VK_LBUTTON) >= 0) {  // 3.0.11-beta.177: let go - dots again
        s.eyeHeld = false;
        s.key.clear();
    }
    s.fields.passwordShown = s.eyeHeld ? utf8Of(widened(s.seenPassword, codePage)) : std::string();
    s.fields.dropOpen = s.dropOpen;
    s.fields.shelfBack = any && forOne && s.view != loginview::View::Classic;
    s.fields.note = !any && shelf.addAfterLogin ? say(settings::Text::ShelfFirstNote) : "";
    const std::string siteNick = sitekey::token().empty() ? std::string() : sitekey::nick();  // 3.0.11-beta.180
    s.fields.site = !sitekey::token().empty()   ? lc::SiteLine::Chip
                    : sitesigninwin::siteReady() ? lc::SiteLine::Offer
                                                 : lc::SiteLine::None;
    s.fields.siteChip = settings::fmt(say(settings::Text::SiteChip),
                                      {siteNick.empty() ? std::string("uaro.kiev.ua") : siteNick,
                                       std::to_string(siteaccounts::logins().size())});
    lc::Options o;
    o.errorAt = s.error.empty() ? s.fields.hintField : s.fields.errorField;
    o.note = !s.fields.note.empty();
    o.back = s.fields.shelfBack;
    o.drop = any;
    o.site = s.fields.site != lc::SiteLine::None;
    s.layout = lc::layout(targetWidth, targetHeight, s.scale, o);
    s.show.hover = 0;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.show.hover = int(lc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                       float(cy) * float(k) - float(s.layout.y)));
    const lc::Fields& f = s.fields;
    const std::string key = f.account + ';' + std::to_string(f.passwordLength) + ';' + std::to_string(int(f.focus)) +
                            (f.caret ? "|" : "") + (f.remember ? "r" : "") + (f.addShelf ? "s" : "") +
                            (f.shelfBack ? "b" : "") + f.note + (f.passwordShown.empty() ? "" : "e") +
                            (f.dropOpen ? "d" : "") + std::to_string(int(s.view)) + std::to_string(int(f.site)) +
                            f.siteChip + ';' +
                            std::to_string(s.layout.x) + ',' +
                            std::to_string(s.layout.y) + ';' + std::to_string(s.show.hover) + '#' +
                            std::to_string(uistyle::generation()) + '!' + s.error + '?' + s.fields.hint;
    if (const std::string drawn = key + (uaui::readyAnywhere() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.33
        s.key = drawn;
        render(s);
    }
    if (s.dropOpen) dropFrame(s);  // 3.0.11-beta.177
    chromeFrame(s, targetWidth, targetHeight);
    s.shown = true;
    block(s, game);
    blockBar(s, game);
}

// 3.0.18-beta.44 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the strip at the login's
// foot in RmlUi's markup, line by line as core loginchrome paints it - the server's status pill and its dot, the site's
// news pill (gold edged), the languages' and the views' segmented pills (the chosen gold); the words where core
// loginchrome places them
static void chromeSheet(State& s, uisheet::Content& c) {
    const loginchrome::Layout& L = s.chrome;
    const loginchrome::Show& sh = s.chromeShow;
    const float sc = L.s;
    constexpr float kPillH = 32, kDot = 9, kNewsH = 30;  // core loginchrome's
    constexpr uint32_t kPill = 0x0A0C12, kWhite = 0xFFFFFF, kGold = 0xC9A55B;
    c.w = float(L.w), c.h = float(L.h);
    if (L.status.w > 0) {
        c.rect(c.under, L.status, kPillH * sc / 2, kPill, 0.72f, kWhite, 0.10f, 1.0f * sc);
        c.rect(c.under, L.statusDot, kDot * sc / 2, sh.statusRgb, 1.0f);
    }
    if (L.news.w > 0)
        c.rect(c.under, L.news, kNewsH * sc / 2, kPill, 0.62f, kGold,
               sh.hover == int(loginchrome::Part::News) ? 0.55f : 0.30f, 1.0f * sc);
    const auto segments = [&](const panel::Box* seg, int chosen, int firstPart) {
        const panel::Box all{seg[0].x, seg[0].y, seg[2].x + seg[2].w - seg[0].x, seg[0].h};
        c.rect(c.under, all, kPillH * sc / 2, kPill, 0.72f, kWhite, 0.10f, 1.0f * sc);
        for (int i = 0; i < 3; ++i) {
            if (i == chosen) c.rect(c.under, seg[i], kPillH * sc / 2, kGold, 0.26f);
            else if (sh.hover == firstPart + i) c.rect(c.under, seg[i], kPillH * sc / 2, kWhite, 0.08f);
        }
    };
    segments(L.langSeg, sh.lang, int(loginchrome::Part::Lang0));
    if (sh.views) segments(L.viewSeg, sh.view, int(loginchrome::Part::View0));
    c.say(loginchrome::words(L, sh));
}

// 3.0.18-beta.33 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the login in RmlUi's
// markup, line by line as core logincard paints it - its glass, the account's and the password's fields (gold
// focused, red refused), the ticks, ▾ of the shelf and the eye (core logincard's shapes as pictures), «Увійти»,
// «Вийти»; the words where core logincard places them
static void fillSheet(State& s, uisheet::Content& c) {
    const lc::Layout& L = s.layout;
    const lc::Fields& f = s.fields;
    const lc::Show& sh = s.show;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kMuted = 0x9AA3B5;
    constexpr uint32_t kBlack = 0x000000, kRed = 0xFF5A4E;
    auto& u = c.under;
    c.w = float(L.w), c.h = float(L.h);
    c.rect(u, L.glass, 14 * sc, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * sc);
    const auto field = [&](const panel::Box& b, bool focused, bool hot, bool refused) {
        const float edge = refused ? 0.9f : focused ? 0.85f : hot ? 0.30f : 0.16f;
        c.rect(u, b, 8 * sc, kBlack, 0.35f, refused ? kRed : focused ? kGold : kWhite, edge,
               (focused || refused ? 1.5f : 1.0f) * sc);
    };
    const bool bad = !f.error.empty();
    field(L.account, f.focus == lc::Focus::Account, sh.hover == int(lc::Part::Account),
          bad && f.errorField == loginerr::Field::Account);
    field(L.password, f.focus == lc::Focus::Password, sh.hover == int(lc::Part::Password),
          bad && f.errorField == loginerr::Field::Password);
    const auto box = [&](const panel::Box& b, bool on) {
        c.rect(u, b, 4 * sc, on ? kGold : kWhite, on ? 0.85f : 0.06f, on ? kGold : kWhite, on ? 1.0f : 0.35f,
               1.0f * sc);
    };
    box(L.tick, f.remember);
    box(L.addTick, f.addShelf);
    if (L.drop.w > 0) {  // ▾: core logincard's triangle in a picture of its own
        const float a = sh.hover == int(lc::Part::Drop) || f.dropOpen ? 1.0f : 0.8f;
        const int side = std::max(4, int(std::ceil(12 * sc)));
        const std::string key = "drop" + std::to_string(side) + (a >= 1 ? "+" : "-");
        const std::string src = s.sheet.made(key, side, side, [&](panel::Picture& p) {
            lc::paintTriangle(p, float(side) / 2, float(side) / 2, 10 * sc, kGold2, a);
        });
        c.icons.push_back(uisheet::Icon{std::round(L.drop.x + L.drop.w / 2 - 2 * sc - float(side) / 2),
                                        std::round(L.drop.y + L.drop.h / 2 - float(side) / 2), float(side),
                                        float(side), 1, src});
    }
    {  // the eye
        const float a = !f.passwordShown.empty() ? 1.0f : sh.hover == int(lc::Part::Eye) ? 0.9f : 0.6f;
        const int w = std::max(4, int(std::ceil(20 * sc))), h = std::max(4, int(std::ceil(14 * sc)));
        const std::string key = "eye" + std::to_string(w) + ':' + std::to_string(int(a * 10));
        const std::string src = s.sheet.made(key, w, h, [&](panel::Picture& p) {
            lc::paintEye(p, panel::Box{0, 0, float(w), float(h)}, kMuted, a, sc);
        });
        c.icons.push_back(uisheet::Icon{std::round(L.eye.x + 8 * sc), std::round(L.eye.y + L.eye.h / 2 - 7 * sc),
                                        float(w), float(h), 1, src});
    }
    c.rect(u, L.enter, 9 * sc, kGold, sh.hover == int(lc::Part::Enter) ? 0.36f : 0.24f, kGold, 0.85f, 1.0f * sc);
    c.rect(u, L.exit, 9 * sc, kWhite, sh.hover == int(lc::Part::Exit) ? 0.12f : 0.06f, kWhite, 0.16f, 1.0f * sc);
    c.say(lc::words(L, f, sh, texts(s.view)));
    if (s.dropOpen && s.drop.w > 0) {  // 3.0.18-beta.45: ▾'s list over the card, as core logincard paints it
        const float x = s.drop.x, y = s.drop.y;
        c.rect(c.topBoxes, panel::Box{x, y, s.drop.w, s.drop.h}, 10 * sc, 0x161B26, 0.98f, 0xFFFFFF, 0.14f, 1.0f * sc);
        if (s.dropHover >= 0 && s.dropHover < int(s.drop.rows.size())) {
            const panel::Box& r = s.drop.rows[size_t(s.dropHover)];
            c.rect(c.topBoxes, panel::Box{x + r.x - s.drop.x, y + r.y - s.drop.y, r.w, r.h}, 7 * sc, kGold, 0.16f);
        }
        c.sayInto(c.topWords,
                  lc::dropWords(s.drop, s.dropRows, s.dropHover, say(settings::Text::LoginOtherLogin), s.scale), x, y);
    }
}

void frame(HWND game, bool on, bool status, bool news, loginview::View view, unsigned codePage, double k,
           int targetWidth, int targetHeight) {
    frameInner(game, on, status, news, view, codePage, k, targetWidth, targetHeight);
    State& s = state();
    s.chromeSheet.anywhere = true;  // 3.0.18-beta.44
    s.chromeShown = s.chromeSheet.frame("login strip", s.shown && s.chrome.w > 0 && s.chromeGen > 0, true, s.chrome.x,
                                        s.chrome.y, s.chromeGen, [&](uisheet::Content& c) { chromeSheet(s, c); });
    s.sheet.anywhere = true;  // before the world
    const int gen = s.sheetGen ^ (s.dropOpen ? (s.dropHover + 2) * 7919 : 0);  // ▾'s list and its row lit too
    s.sheetShown = s.sheet.frame("login", s.shown && s.mode == State::Mode::Card, true, s.layout.x, s.layout.y, gen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.shown || s.k <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    if (s.mode == State::Mode::Shelf) {  // 3.0.11-beta.176: «Полиця» in place of the card
        loginshelfwin::draw(record, renderer);
    } else if (s.mode == State::Mode::Continue) {  // 3.0.11-beta.177: «Продовжити»
        logincontinuewin::draw(record, renderer);
    } else if (s.mode == State::Mode::Site) {  // 3.0.11-beta.180: the site sign-in card
        sitesigninwin::draw(record, renderer);
    } else if (s.sheetShown || (s.picture.width > 0 && s.picture.height > 0)) {
        const panel::Box& g = s.layout.glass;
        s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                      14 * s.scale, s.scale, s.k);  // 3.0.18-beta.43: under uaUI's card too
        if (!s.sheetShown) {  // 3.0.18-beta.33: uaUI draws the card (with the windows' layer); ▾'s list stays here
            drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
        }
        if (!s.sheetShown && s.dropOpen && s.dropPicture.width > 0)  // 3.0.11-beta.177: ▾'s list over the card
            drawPart(record, renderer, s.dropPicture, s.dropTexture, s.dropTexW, s.dropTexH, s.dropUpload,
                     s.layout.x + int(s.drop.x), s.layout.y + int(s.drop.y), s.k);
    }
    if (!s.chromeShown && s.chrome.w > 0 && s.chromePicture.width > 0)  // 3.0.11-beta.173: the strip at the foot
        drawPart(record, renderer, s.chromePicture, s.chromeTexture, s.chromeTexW, s.chromeTexH, s.chromeUpload,
                 s.chrome.x, s.chrome.y, s.k);
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
    if (s.chrome.w > 0) {  // 3.0.11-beta.174: the strip's switches
        const loginchrome::Part p = loginchrome::hitTest(s.chrome, tx - float(s.chrome.x), ty - float(s.chrome.y));
        const bool lang = p >= loginchrome::Part::Lang0 && p <= loginchrome::Part::Lang2;
        const bool news = p == loginchrome::Part::News || p == loginchrome::Part::AllNews;
        if (lang || news || (s.chromeShow.views && p != loginchrome::Part::None)) {
            result = 0;
            const bool click = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK;
            if (lang && click) chooseLanguage(int(p) - int(loginchrome::Part::Lang0));
            if (news && click) openNews(s, p == loginchrome::Part::AllNews);
            if (click && p >= loginchrome::Part::View0 && p <= loginchrome::Part::View2)  // 3.0.11-beta.177
                chooseView(s, loginview::View(int(p) - int(loginchrome::Part::View0)));
            return true;
        }
    }
    if (s.mode == State::Mode::Shelf) {  // 3.0.11-beta.176
        bool took = false;
        const loginshelfwin::Asked a = loginshelfwin::mouse(m, wp, tx, ty, took);
        if (!took) return false;
        result = 0;
        act(s, a);
        return true;
    }
    if (s.mode == State::Mode::Site) {  // 3.0.11-beta.180
        if (!sitesigninwin::mouse(m, tx, ty)) return false;
        result = 0;
        return true;
    }
    if (s.mode == State::Mode::Continue) {  // 3.0.11-beta.177
        bool took = false;
        const logincontinuewin::Asked a = logincontinuewin::mouse(m, tx, ty, took);
        if (!took) return false;
        result = 0;
        actContinue(s, a);
        return true;
    }
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (s.dropOpen) {  // 3.0.11-beta.177: ▾'s list first; a press beside it closes it
        const bool over = lx >= s.drop.x && ly >= s.drop.y && lx < s.drop.x + s.drop.w && ly < s.drop.y + s.drop.h;
        if (over) {
            result = 0;
            const int row = lc::dropHit(s.drop, lx, ly);
            if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && row >= 0) dropPick(s, row);
            return true;
        }
        if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) {
            s.dropOpen = false;
            s.key.clear();
        }
    }
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        const lc::Part p = lc::hitTest(s.layout, lx, ly);
        if (p != lc::Part::None) press(s, p);
    }
    return true;
}

bool shown() { return state().shown; }

void drawVeil(FrameRecord& record, gpu::Renderer& renderer) {  // 3.0.11-beta.203
    Veil& v = state().veil;
    if (state().veilShown || !v.on || v.k <= 0 || v.w <= 0 || v.h <= 0) return;  // 3.0.18-beta.69: or by uaUI
    drawShade(record, 0, 0, v.w, v.h, v.k, 0.94f);
    const float scale = uistyle::readingScale(v.k);
    const int phase = int((GetTickCount64() - v.since) / 350 % 3);
    const enterveil::Layout L = enterveil::layout(v.w, v.h, scale);
    if (phase != v.phase || L.w != v.layout.w || L.h != v.layout.h) {
        v.phase = phase;
        v.layout = L;
        enterveil::paint(v.picture, L, phase);
        std::vector<Words> words;
        for (const enterveil::Words& w : enterveil::words(L, v.texts))
            words.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                                  UINT(w.align == enterveil::Align::Left    ? DT_LEFT
                                       : w.align == enterveil::Align::Right ? DT_RIGHT
                                                                            : DT_CENTER),
                                  false});
        drawWords(v.picture, words);
        v.upload = true;
    }
    drawPart(record, renderer, v.picture, v.texture, v.texW, v.texH, v.upload, L.x, L.y, v.k);
}

void drawToast(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (s.toastShown || s.toast.empty() || now >= s.toastUntil || s.toastK <= 0 || s.toastLayout.w <= 0) return;
    const shelftoast::Layout& L = s.toastLayout;
    int cx = 0, cy = 0;
    s.toastHot = readGameCursor(cx, cy) &&
                 shelftoast::overUndo(L, float(cx) * s.toastK - float(L.x), float(cy) * s.toastK - float(L.y));
    const std::string undo = say(settings::Text::ShelfUndo);
    const std::string key = s.toast + '#' + undo + (s.toastHot ? "h" : "") + std::to_string(L.w) + 'x' +
                            std::to_string(L.h) + '#' + std::to_string(uistyle::generation());
    if (key != s.toastKey) {
        s.toastKey = key;
        if (!panel::blank(s.toastPicture, L.w, L.h)) return;
        shelftoast::paint(s.toastPicture, L, s.toastHot);
        std::vector<Words> words;
        for (const shelftoast::Words& w : shelftoast::words(L, s.toast, undo, s.toastHot))
            words.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold, UINT(DT_LEFT), false});
        drawWords(s.toastPicture, words);
        s.toastUpload = true;
    }
    const float fade = std::clamp(float(s.toastUntil - now) / 400.0f, 0.0f, 1.0f);  // its last 0.4 s fading
    drawPart(record, renderer, s.toastPicture, s.toastTexture, s.toastTexW, s.toastTexH, s.toastUpload, L.x, L.y,
             s.toastK, fade);
}

bool toastMouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;
    if (s.toast.empty() || GetTickCount64() >= s.toastUntil || s.toastK <= 0 || s.toastLayout.w <= 0) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK;
    if (!button) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.toastK, s.toastW, s.toastH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.toastLayout.x), ly = ty - float(s.toastLayout.y);
    if (!s.toastLayout.pill.contains(lx, ly)) return false;
    result = 0;
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && shelftoast::overUndo(s.toastLayout, lx, ly)) undoShelf(s);
    return true;
}

const std::string& loggedIn() { return state().loggedIn; }

bool loginWith(const accounts::Account& a, bool stopAtSelect, const std::string& character) {
    State& s = state();
    const uint32_t window = gameLogin();
    if (!window || !s.places.known() || a.login.empty()) return false;
    const unsigned long long hash = accounts::loginHash(a.login);
    bool busy = false;  // 3.0.11-beta.178: played in another game window - its login there would throw it out
    for (const auto& b : gamelogins::others()) busy = busy || b.login == hash;
    const accounts::Plan p = accounts::planFor(a, stopAtSelect || GetKeyState(VK_SHIFT) < 0, busy);
    putFields(s, window, a.login, p.fillPassword ? a.password : std::string());
    s.seenAccount = a.login;  // the window may go before the next frame reads them
    s.seenPassword = p.fillPassword ? a.password : std::string();
    s.key.clear();
    if (!p.enterAtOnce) {  // the card shows it: its password to type, or «Увійти» pressed knowingly (another window)
        loginshelfwin::classic(true);
        reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, read32(window + s.places.password));
        if (!character.empty() && !busy) {  // 3.0.11-beta.181: entered with it once its password is typed
            s.oneClick = a.login;
            s.autoName = character;
        }
        LOG_INFO("login: the shelf's %016llx - %s", hash, busy ? "played in another game window: the card shows it"
                                                              : "its password asked");
        return true;
    }
    s.oneClick = a.login;
    s.autoName = stopAtSelect ? std::string() : !character.empty() ? character  // 3.0.11-beta.181: the one clicked
                                              : p.autoCharacter     ? a.last.name
                                                                    : std::string();
    LOG_INFO("login: the shelf's %016llx sent with one click%s", hash, p.autoCharacter ? " («Одразу в гру»)" : "");
    if (!s.autoName.empty()) {  // 3.0.11-beta.203: on to the map by itself - under the veil
        Veil& v = s.veil;
        v.on = true;
        v.since = GetTickCount64();
        v.texts.title = say(settings::Text::EnterVeilTitle);
        v.texts.name = s.autoName;  // UTF-8, as the shelf keeps it
        v.texts.line = s.autoName == a.last.name
                           ? a.last.job + (a.last.level > 0 ? " \xC2\xB7 " + std::to_string(a.last.level) : "")
                           : std::string();
        v.phase = -1;
        LOG_INFO("login: the veil «%s» over the way into the game", v.texts.title.c_str());
    }
    pressEnter(window);
    return true;
}

void veilOver(const std::string& name, const std::string& line) {
    State& s = state();
    if (!s.on) return;  // the login's new look off: the game's own way in
    Veil& v = s.veil;
    v.on = true;
    v.since = GetTickCount64();
    v.texts.title = say(settings::Text::EnterVeilTitle);
    v.texts.name = name;
    v.texts.line = line;
    v.phase = -1;
    LOG_INFO("login: the veil «%s» over the way into the game («Увійти» in the character select)",
             v.texts.title.c_str());
}

bool active() {
    const State& s = state();
    return s.on && s.tookLast;
}

void connectFailed() {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (!s.connectFailed && (s.firstFailAt == 0 || now - s.failedAt > kRetryForMs)) s.firstFailAt = now;
    s.connectFailed = true;
    s.failedAt = now;
    LOG_INFO("login: the server could not be reached - tried again in %d s", srvstatus::kProbeSec);
}

const std::string& account() { return state().lastTyped; }

void beforeGameDraw() {
    State& s = state();
    if (s.on && s.places.known()) hold(s, gameLogin(), true);
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (!s.shown) return false;
    bool took = false;
    if (s.mode == State::Mode::Shelf) {
        const loginshelfwin::Asked a = loginshelfwin::key(m, wp, took);
        if (a.act != loginshelfwin::Act::None) act(s, a);
    } else if (s.mode == State::Mode::Site) {  // 3.0.11-beta.180: its fields
        took = sitesigninwin::key(m, wp);
    } else if (s.mode == State::Mode::Continue) {  // 3.0.11-beta.177
        const logincontinuewin::Asked a = logincontinuewin::key(m, wp, took);
        if (a.act != logincontinuewin::Act::None) actContinue(s, a);
    } else if (s.dropOpen && m == WM_KEYDOWN && wp == VK_ESCAPE) {  // ▾'s list closed
        s.dropOpen = false;
        s.key.clear();
        took = true;
    }
    return took;
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const ULONGLONG now = GetTickCount64();
    const bool asked = s.exitAskedAt && now - s.exitAskedAt < 3000;
    if (asked) {  // «вийти з гри»: B again leaves, anything else stays
        s.exitAskedAt = 0;
        if (c == PadCommand::B) {
            LOG_INFO("login: the pad's B twice - «Вийти»");
            press(s, lc::Part::Exit);
        }
        return;
    }
    const bool forOne = loginshelfwin::classicChosen();
    if (s.mode == State::Mode::Shelf) {
        using P = loginshelfwin::Pad;
        const P p = c == PadCommand::Left ? P::Left : c == PadCommand::Right ? P::Right : c == PadCommand::Up ? P::Up
                    : c == PadCommand::Down ? P::Down : c == PadCommand::A ? P::Select : c == PadCommand::X ? P::Other
                    : c == PadCommand::Y ? P::Menu : P::Back;
        if (c == PadCommand::B && !loginshelfwin::menuOpen()) {
            s.exitAskedAt = now;
            return;
        }
        bool took = false;
        const loginshelfwin::Asked a = loginshelfwin::pad(p, took);
        if (a.act != loginshelfwin::Act::None) act(s, a);
        return;
    }
    if (s.mode == State::Mode::Continue) {
        const int index = logincontinuewin::shown();
        using A = logincontinuewin::Act;
        if (c == PadCommand::A) actContinue(s, logincontinuewin::Asked{A::Play, index});
        if (c == PadCommand::X) actContinue(s, logincontinuewin::Asked{A::OtherChar, index});
        if (c == PadCommand::Y) logincontinuewin::nextAccount();
        if (c == PadCommand::B) s.exitAskedAt = now;
        return;
    }
    if (c == PadCommand::A) press(s, lc::Part::Enter);  // the card: what is typed sent
    if (c == PadCommand::B) {
        if (forOne && !accountsstore::shelf().list.empty())
            press(s, lc::Part::Shelf);
        else
            s.exitAskedAt = now;
    }
}

uapad::LoginHint padHint() {
    const State& s = state();
    if (!s.shown) return uapad::LoginHint::None;
    if (s.exitAskedAt && GetTickCount64() - s.exitAskedAt < 3000) return uapad::LoginHint::ExitAsk;
    if (s.mode == State::Mode::Shelf)
        return loginshelfwin::menuOpen() ? uapad::LoginHint::Menu : uapad::LoginHint::Shelf;
    if (s.mode == State::Mode::Continue) return uapad::LoginHint::Continue;
    return loginshelfwin::classicChosen() && !accountsstore::shelf().list.empty() ? uapad::LoginHint::CardBack
                                                                                  : uapad::LoginHint::Card;
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    if (s.mode == State::Mode::Shelf) return loginshelfwin::box(x, y, w, h);         // 3.0.11-beta.176
    if (s.mode == State::Mode::Continue) return logincontinuewin::box(x, y, w, h);  // 3.0.11-beta.177
    if (s.mode == State::Mode::Site) return sitesigninwin::box(x, y, w, h);          // 3.0.11-beta.180
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::loginwin
