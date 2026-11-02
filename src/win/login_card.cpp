#include "win/login_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

#include "core/login_card.hpp"
#include "core/login_chrome.hpp"
#include "core/reference_live.hpp"
#include "core/server_status.hpp"
#include "core/login_hints.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/login_refusal.hpp"
#include "win/paths.hpp"
#include "win/reference_load.hpp"
#include "win/server_probe.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

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
using ShellExecuteFn = HINSTANCE(WINAPI*)(HWND, const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*, int);

struct State {
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

lc::Texts texts() {
    lc::Texts t;
    t.title = "uaRO";
    t.subtitle = say(settings::Text::LoginSubtitle);
    t.account = say(settings::Text::LoginAccount);
    t.password = say(settings::Text::LoginPassword);
    t.remember = say(settings::Text::LoginRemember);
    t.enter = say(settings::Text::LoginEnter);
    t.exit = say(settings::Text::LoginExit);
    t.register_ = say(settings::Text::LoginRegister);
    t.createAccount = say(settings::Text::LoginCreateAccount);
    t.resetPassword = say(settings::Text::LoginResetPassword);
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
    const uicost::Paint cost("the login", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    lc::paint(s.picture, s.layout, s.fields, s.show);
    drawWords(s.picture, gdiWords(lc::words(s.layout, s.fields, s.show, texts())));
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
void sendAgain(State& s, uint32_t window) {
    reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, 0);
    setText(read32(window + s.places.account), s.tryAccount);
    setText(read32(window + s.places.password), s.tryPassword);
    LOG_INFO("login: the server did not answer - the login sent again");
    reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)))(window, 0, kButtonMsg,
                                                                                 lc::gameButton(lc::Part::Enter), 0, 0);
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
                            c.news;
    if (key == s.chromeKey) return;
    s.chromeKey = key;
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
bool openUrl(const wchar_t* url) {
    const HMODULE shell = LoadLibraryA("shell32.dll");
    const auto open = reinterpret_cast<ShellExecuteFn>(
        reinterpret_cast<void*>(shell ? GetProcAddress(shell, "ShellExecuteW") : nullptr));
    if (open) open(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return open != nullptr;
}

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

void press(State& s, lc::Part p) {
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
        const bool opened = openUrl(kRegister);
        LOG_INFO("login: the registration page %s (uaro.kiev.ua/account/register)", opened ? "opened" : "not opened");
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

}  // namespace

void frame(HWND game, bool on, bool status, bool news, unsigned codePage, double k, int targetWidth,
           int targetHeight) {
    State& s = state();
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
    }
    s.hadWindow = window != 0;
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
    s.layout = lc::layout(targetWidth, targetHeight, s.scale,
                          s.error.empty() ? s.fields.hintField : s.fields.errorField);
    s.show.hover = 0;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.show.hover = int(lc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                       float(cy) * float(k) - float(s.layout.y)));
    const lc::Fields& f = s.fields;
    const std::string key = f.account + ';' + std::to_string(f.passwordLength) + ';' + std::to_string(int(f.focus)) +
                            (f.caret ? "|" : "") + (f.remember ? "r" : "") + ';' + std::to_string(s.layout.x) + ',' +
                            std::to_string(s.layout.y) + ';' + std::to_string(s.show.hover) + '#' +
                            std::to_string(uistyle::generation()) + '!' + s.error + '?' + s.fields.hint;
    if (key != s.key) {
        s.key = key;
        render(s);
    }
    chromeFrame(s, targetWidth, targetHeight);
    s.shown = true;
    block(s, game);
    blockBar(s, game);
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
    if (s.chrome.w > 0 && s.chromePicture.width > 0)  // 3.0.11-beta.173: the strip at the screen's foot
        drawPart(record, renderer, s.chromePicture, s.chromeTexture, s.chromeTexW, s.chromeTexH, s.chromeUpload,
                 s.chrome.x, s.chrome.y, s.k);
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
    if (s.chrome.w > 0) {  // 3.0.11-beta.174: the strip's switches
        const loginchrome::Part p = loginchrome::hitTest(s.chrome, tx - float(s.chrome.x), ty - float(s.chrome.y));
        const bool lang = p >= loginchrome::Part::Lang0 && p <= loginchrome::Part::Lang2;
        const bool news = p == loginchrome::Part::News || p == loginchrome::Part::AllNews;
        if (lang || news || (s.chromeShow.views && p != loginchrome::Part::None)) {
            result = 0;
            const bool click = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK;
            if (lang && click) chooseLanguage(int(p) - int(loginchrome::Part::Lang0));
            if (news && click) openNews(s, p == loginchrome::Part::AllNews);
            return true;
        }
    }
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        const lc::Part p = lc::hitTest(s.layout, lx, ly);
        if (p != lc::Part::None) press(s, p);
    }
    return true;
}

bool shown() { return state().shown; }

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

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::loginwin
