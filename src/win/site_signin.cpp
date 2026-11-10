#include "win/site_signin.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "core/accounts.hpp"
#include "core/register_form.hpp"
#include "core/settings/strings.hpp"
#include "core/site_api.hpp"
#include "core/site_signin.hpp"
#include "core/ui_style.hpp"
#include "win/accounts_store.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/open_page.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/site_accounts.hpp"
#include "win/site_http.hpp"
#include "win/site_key.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::sitesigninwin {
namespace {

namespace ss = sitesignin;
constexpr ULONGLONG kProbeAgainMs = 10 * 60 * 1000;
constexpr size_t kFieldMost = 120;  // a field's bytes at most

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.40: the card drawn by uaUI (before the world)
    int sheetGen = 0;
    bool sheetShown = false;
    bool open = false;
    ss::Way way = ss::Way::Password;
    ss::Field focus = ss::Field::Email;
    std::string email, password, code2fa, key;  // never logged; the secret ones zeroed when closed
    std::string login, gamePass, newPass;       // 3.0.11-beta.181: the forms'
    char sex = 'M';
    int formAccount = 0;
    // 3.0.11-beta.181: the registration - the rules ticked, the login asked while typed, the result once
    bool agree = false;
    int loginFree = -1;
    std::string freeAsked, freeSeen;  // the login asked about, the login as last typed
    ULONGLONG loginChangedAt = 0;
    bool registered = false;
    std::string regLogin, regPassword;
    bool twofa = false, busy = false, signedIn = false;
    std::string error;
    // the browser's code
    std::string deviceCode, userCode;
    ULONGLONG expiresAt = 0, nextPollAt = 0;
    int interval = 5;
    // the site's client API there?
    int ready = -1;  // -1 not known, 0 no, 1 yes
    bool probing = false;
    ULONGLONG probedAt = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    ss::Layout layout;
    ss::Show show;
    std::string picKey;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

void wipe(std::string& t) {
    if (!t.empty()) SecureZeroMemory(t.data(), t.size());
    t.clear();
}

ss::Texts texts() {
    ss::Texts t;
    t.title = say(settings::Text::SiteTitle);
    t.subtitle = say(settings::Text::SiteSubtitle);
    t.ways[0] = say(settings::Text::SiteWayPassword);
    t.ways[1] = say(settings::Text::SiteWayBrowser);
    t.ways[2] = say(settings::Text::SiteWayKey);
    t.email = "E-mail";
    t.password = say(settings::Text::SiteWayPassword);
    t.code2fa = say(settings::Text::SiteCode2fa);
    t.key = say(settings::Text::SiteKeyLabel);
    t.signIn = say(settings::Text::LoginEnter);
    t.startBrowser = say(settings::Text::SiteStartBrowser);
    t.waiting = say(settings::Text::SiteWaiting);
    t.back = say(settings::Text::SiteBack);
    t.forms[0] = say(settings::Text::SiteFormNewLogin);  // 3.0.11-beta.181
    t.forms[1] = say(settings::Text::SiteFormLink);
    t.forms[2] = say(settings::Text::SiteFormGamePassword);
    t.login = say(settings::Text::LoginAccount);
    t.gamePass = say(settings::Text::SiteGamePass);
    t.currentPass = say(settings::Text::SiteCurrentPass);
    t.newPass = say(settings::Text::SiteNewPass);
    t.sexes[0] = say(settings::Text::SiteSexM);
    t.sexes[1] = say(settings::Text::SiteSexF);
    t.create = say(settings::Text::SiteCreate);
    t.link = say(settings::Text::SiteLink);
    t.change = say(settings::Text::SiteChange);
    t.regTitle = say(settings::Text::RegTitle);  // 3.0.11-beta.181: the registration
    t.steps = say(settings::Text::RegSteps);
    t.password2 = say(settings::Text::RegPassword2);
    t.agree = say(settings::Text::RegAgree);
    t.rules = say(settings::Text::RegRules);
    t.regCreate = say(settings::Text::RegCreate);
    t.haveAccount = say(settings::Text::RegHaveAccount);
    t.free = say(settings::Text::RegFree);
    t.taken = say(settings::Text::RegTaken);
    return t;
}

std::vector<Words> gdi(const std::vector<ss::Words>& in) {
    std::vector<Words> out;
    for (const ss::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ss::Align::Center  ? DT_CENTER
                                 : w.align == ss::Align::Right ? DT_RIGHT
                                                               : DT_LEFT),
                            false});
    return out;
}

std::string* fieldText(State& s, ss::Field f) {
    switch (f) {
        case ss::Field::Email: return &s.email;
        case ss::Field::Password: return &s.password;
        case ss::Field::Code: return &s.code2fa;
        case ss::Field::Key: return &s.key;
        case ss::Field::Login: return &s.login;
        case ss::Field::GamePass: return &s.gamePass;
        case ss::Field::NewPass: return &s.newPass;
        default: return nullptr;
    }
}

// The way's fields in their order (Tab goes round them)
std::vector<ss::Field> fieldsOf(const State& s) {
    if (s.way == ss::Way::Password)
        return s.twofa ? std::vector<ss::Field>{ss::Field::Email, ss::Field::Password, ss::Field::Code}
                       : std::vector<ss::Field>{ss::Field::Email, ss::Field::Password};
    if (s.way == ss::Way::Key) return {ss::Field::Key};
    if (s.way == ss::Way::NewLogin || s.way == ss::Way::Link) return {ss::Field::Login, ss::Field::GamePass};
    if (s.way == ss::Way::GamePassword) return {ss::Field::GamePass, ss::Field::NewPass};
    if (s.way == ss::Way::Register)
        return {ss::Field::Login, ss::Field::GamePass, ss::Field::NewPass, ss::Field::Email};
    return {};
}

void signedIn(State& s, const std::string& token, const std::string& nick, const char* how) {
    const bool kept = sitekey::save(token, nick);
    LOG_INFO("site: signed in (%s)%s", how, kept ? "" : " - the token not kept (this game only)");
    s.signedIn = true;
    close();
}

std::string failed(int status) {
    return status == 0 ? say(settings::Text::SiteErrNoAnswer)
                       : settings::fmt(say(settings::Text::SiteErrFailed), {std::to_string(status)});
}

// The site's refusal: a known field's in our words (the player's language), else the site's (final review)
std::string refusal(const siteapi::TokenAnswer& t, int status, ss::Way way) {
    if (t.fieldErrors.empty()) return failed(status);
    const auto& first = *t.fieldErrors.begin();
    const auto own = ss::errorText(way, first.first);
    return own ? say(*own) : first.second;
}

void tokenAnswer(State& s, int status, const std::string& body, const char* how) {
    s.busy = false;
    const siteapi::TokenAnswer t = siteapi::parseToken(status, body);
    if (t.ok) return signedIn(s, t.token, t.nick, how);
    if (t.twofa) {
        s.twofa = true;
        s.focus = ss::Field::Code;
        s.error = say(settings::Text::SiteErrTwofa);
        return;
    }
    s.error = refusal(t, status, s.way);
}

void poll(State& s) {
    s.busy = true;
    sitehttp::send(siteapi::devicePoll(s.deviceCode), [](int status, const std::string& body) {
        State& st = state();
        st.busy = false;
        if (!st.open || st.deviceCode.empty()) return;
        siteapi::TokenAnswer t;
        switch (siteapi::parsePoll(status, body, t)) {
            case siteapi::Poll::Approved: return signedIn(st, t.token, t.nick, "a code in the browser");
            case siteapi::Poll::Expired:
                st.deviceCode.clear();
                st.userCode.clear();
                st.error = say(settings::Text::SiteErrExpired);
                return;
            default: st.nextPollAt = GetTickCount64() + ULONGLONG(st.interval) * 1000; return;
        }
    });
}

// 3.0.11-beta.181: a form done - the login and its password on this computer's shelf (one click then), the site's
// logins asked again
void formDone(State& s, const std::string& login, const std::string& password, const char* what) {
    accounts::Shelf& shelf = accountsstore::shelf();
    bool there = false;
    for (accounts::Account& a : shelf.list)
        if (accounts::loginHash(a.login) == accounts::loginHash(login)) {
            a.password = password;
            a.refused = false;
            there = true;
        }
    if (!there && s.way != ss::Way::GamePassword) accounts::upsert(shelf, login, password, 0);
    const bool saved = accountsstore::save();
    LOG_INFO("site: %s - the login %016llx%s", what, (unsigned long long)accounts::loginHash(login),
             saved ? " kept on the shelf" : "");
    siteaccounts::refresh(true);
    close();
}

void formAnswer(int status, const std::string& body, const char* what) {
    State& s = state();
    s.busy = false;
    if (status == 200 || status == 201) {
        const std::string login = s.login, password = s.way == ss::Way::GamePassword ? s.newPass : s.gamePass;
        return formDone(s, login, password, what);
    }
    const siteapi::TokenAnswer t = siteapi::parseToken(status, body);  // its errors
    s.error = refusal(t, status, s.way);
}

// 3.0.11-beta.181: the site's language of its texts (the verification e-mail)
const char* siteLang() {
    const settings::Lang l = language();
    return l == settings::Lang::Uk ? "ua" : l == settings::Lang::Ru ? "ru" : "en";
}

void pressRegister(State& s) {
    regform::Form f;
    f.login = s.login;
    f.password = s.gamePass;
    f.password2 = s.newPass;
    f.email = s.email;
    f.sex = s.sex == 'M' ? 0 : s.sex == 'F' ? 1 : -1;
    f.agree = s.agree;
    const auto refused = regform::check(f);
    wipe(f.password);
    wipe(f.password2);
    if (!refused.empty()) {
        s.error = say(refused.front().second);
        const regform::Field at = refused.front().first;
        s.focus = at == regform::Field::Login       ? ss::Field::Login
                  : at == regform::Field::Password  ? ss::Field::GamePass
                  : at == regform::Field::Password2 ? ss::Field::NewPass
                  : at == regform::Field::Email     ? ss::Field::Email
                                                    : s.focus;
        return;
    }
    s.busy = true;
    sitehttp::send(siteapi::registerAccount(s.login, s.gamePass, s.email, s.sex, sitekey::deviceName(), siteLang()),
                   [](int status, const std::string& body) {
                       State& st = state();
                       st.busy = false;
                       const siteapi::TokenAnswer t = siteapi::parseToken(status, body);
                       if (!t.ok) {
                           st.error = refusal(t, status, ss::Way::Register);
                           return;
                       }
                       sitekey::save(t.token, t.nick);
                       accounts::Shelf& shelf = accountsstore::shelf();
                       accounts::upsert(shelf, st.login, st.gamePass, 0);
                       accountsstore::save();
                       st.regLogin = st.login;
                       st.regPassword = st.gamePass;
                       st.registered = true;
                       LOG_INFO("site: registered - the login %016llx, its first character next",
                                (unsigned long long)accounts::loginHash(st.login));
                       close();
                   });
}

// The registration's login asked 600 ms after it was last typed (a valid one, not asked yet)
void freeFrame(State& s, ULONGLONG now) {
    if (s.login != s.freeSeen) {
        s.freeSeen = s.login;
        s.loginChangedAt = now;
        s.loginFree = -1;
    }
    if (s.login == s.freeAsked || now - s.loginChangedAt < 600 || s.login.size() < 4 || s.login.size() > 23) return;
    s.freeAsked = s.login;
    const std::string asked = s.login;
    sitehttp::send(siteapi::loginFree(asked), [asked](int status, const std::string& body) {
        State& st = state();
        if (st.login == asked) st.loginFree = siteapi::parseFree(status, body);
    });
}

bool pressForm(State& s) {
    if (!ss::isForm(s.way)) return false;
    if (s.way == ss::Way::Register) {
        pressRegister(s);
        return true;
    }
    const bool change = s.way == ss::Way::GamePassword;
    if ((!change && (s.login.size() < 4 || s.gamePass.size() < 4)) || (change && (s.gamePass.empty() ||
                                                                                   s.newPass.size() < 4))) {
        s.error = say(settings::Text::SiteErrForm);
        return true;
    }
    s.busy = true;
    if (s.way == ss::Way::NewLogin)
        sitehttp::send(siteapi::createLogin(s.login, s.gamePass, s.sex),
                       [](int status, const std::string& body) { formAnswer(status, body, "a new game login"); });
    else if (s.way == ss::Way::Link)
        sitehttp::send(siteapi::linkLogin(s.login, s.gamePass),
                       [](int status, const std::string& body) { formAnswer(status, body, "a game login linked"); });
    else
        sitehttp::send(siteapi::changePassword(s.formAccount, s.gamePass, s.newPass),
                       [](int status, const std::string& body) {
                           formAnswer(status, body, "a game password changed");
                       });
    return true;
}

void press(State& s) {
    if (s.busy) return;
    s.error.clear();
    if (pressForm(s)) return;
    if (s.way == ss::Way::Password) {
        if (s.email.empty() || s.password.empty()) {
            s.error = say(settings::Text::SiteErrEmpty);
            return;
        }
        s.busy = true;
        sitehttp::send(siteapi::authPassword(s.email, s.password, sitekey::deviceName(), s.code2fa),
                       [](int status, const std::string& body) { tokenAnswer(state(), status, body, "the password"); });
    } else if (s.way == ss::Way::Browser) {
        s.busy = true;
        sitehttp::send(siteapi::deviceStart(sitekey::deviceName()), [](int status, const std::string& body) {
            State& st = state();
            st.busy = false;
            const siteapi::DeviceStart d = siteapi::parseDeviceStart(status, body);
            if (!d.ok) {
                st.error = failed(status);
                return;
            }
            st.deviceCode = d.deviceCode;
            st.userCode = d.userCode;
            st.interval = std::max(2, d.interval);
            st.expiresAt = GetTickCount64() + ULONGLONG(d.expiresIn) * 1000;
            st.nextPollAt = GetTickCount64() + ULONGLONG(st.interval) * 1000;
            const bool opened = d.verifyUrl.rfind("https://uaro.kiev.ua/", 0) == 0 && openPage(utf8Wide(d.verifyUrl));
            LOG_INFO("site: a code for the browser%s", opened ? " - its page opened" : "");
        });
    } else {
        if (s.key.empty()) {
            s.error = say(settings::Text::SiteErrKey);
            return;
        }
        s.busy = true;
        const std::string key = s.key;
        sitehttp::send(
            siteapi::accounts(),
            [key](int status, const std::string& body) {
                State& st = state();
                st.busy = false;
                if (status != 200) {
                    st.error = status == 401 ? say(settings::Text::SiteErrKey) : failed(status);
                    return;
                }
                const std::vector<siteapi::SiteLogin> logins = siteapi::parseAccounts(status, body);
                signedIn(st, key, logins.empty() ? std::string() : logins.front().login, "the cabinet's key");
            },
            key);
    }
}

// Ctrl+V: the clipboard's words into the focused field (one line, trimmed)
void paste(State& s) {
    std::string* f = fieldText(s, s.focus);
    if (!f || !OpenClipboard(nullptr)) return;
    std::wstring text;
    if (const HANDLE h = GetClipboardData(CF_UNICODETEXT))
        if (const auto* p = static_cast<const wchar_t*>(GlobalLock(h))) {
            text = p;
            GlobalUnlock(h);
        }
    CloseClipboard();
    std::string add = utf8Of(text);
    add.erase(std::remove_if(add.begin(), add.end(), [](char c) { return c == '\r' || c == '\n' || c == '\t'; }),
              add.end());
    const size_t a = add.find_first_not_of(' '), b = add.find_last_not_of(' ');
    add = a == std::string::npos ? std::string() : add.substr(a, b - a + 1);
    if (f->size() + add.size() <= kFieldMost) *f += add;
    wipe(add);
}

void render(State& s) {
    const ss::Layout& L = s.layout;
    if (uaui::readyAnywhere()) {  // 3.0.18-beta.40: uaUI's card said again, no picture
        ++s.sheetGen;
        return;
    }
    if (!panel::blank(s.picture, L.w, L.h)) return;
    ss::paint(s.picture, L, s.show);
    drawWords(s.picture, gdi(ss::words(L, s.show, texts())));
    s.upload = true;
}

}  // namespace

void open() {
    State& s = state();
    if (s.open) return;
    s.open = true;
    s.way = ss::Way::Password;
    s.focus = ss::Field::Email;
    s.error.clear();
    s.picKey.clear();
    LOG_INFO("site: the sign-in card opened");
}

void openForm(int way, int accountId, const std::string& login) {
    State& s = state();
    open();
    s.way = ss::Way(std::clamp(way, int(ss::Way::NewLogin), int(ss::Way::GamePassword)));
    s.formAccount = accountId;
    s.login = login;
    s.sex = 'M';
    s.focus = s.way == ss::Way::GamePassword || !login.empty() ? ss::Field::GamePass : ss::Field::Login;
}

void openRegister() {
    State& s = state();
    open();
    s.way = ss::Way::Register;
    s.focus = ss::Field::Login;
    s.sex = 0;  // chosen by the player
    s.agree = false;
    s.loginFree = -1;
    s.freeAsked.clear();
}

bool takeRegistered(std::string& login, std::string& password) {
    State& s = state();
    if (!s.registered) return false;
    s.registered = false;
    login = s.regLogin;
    password = s.regPassword;
    s.regLogin.clear();
    wipe(s.regPassword);
    return true;
}

bool isOpen() { return state().open; }

void close() {
    State& s = state();
    s.open = false;
    wipe(s.password);
    wipe(s.key);
    wipe(s.gamePass);
    wipe(s.newPass);
    s.login.clear();
    s.email.clear();
    s.agree = false;
    s.code2fa.clear();
    s.twofa = false;
    s.deviceCode.clear();
    s.userCode.clear();
    s.busy = false;
}

void probe() {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (s.ready == 1 || s.probing || (s.probedAt && now - s.probedAt < kProbeAgainMs)) return;
    s.probing = true;
    s.probedAt = now;
    sitehttp::send(siteapi::loginFree(""), [](int status, const std::string&) {
        State& st = state();
        st.probing = false;
        st.ready = status == 422 ? 1 : status == 0 ? -1 : 0;  // 422: the API there (an empty login refused)
        if (st.ready == 1) LOG_INFO("site: the client's API is there - the site account offered on the login screen");
    });
}

bool siteReady() { return state().ready == 1; }

static void frameInner(double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (!s.open) return;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    if (!s.deviceCode.empty()) {
        if (now >= s.expiresAt) {
            s.deviceCode.clear();
            s.userCode.clear();
            s.error = say(settings::Text::SiteErrExpired);
        } else if (!s.busy && now >= s.nextPollAt) {
            poll(s);
        }
    }
    s.layout = ss::layout(targetWidth, targetHeight, s.scale, s.way, s.twofa);
    ss::Show& sh = s.show;
    sh.way = s.way;
    sh.focus = s.focus;
    sh.email = s.email;
    sh.code2fa = s.code2fa;
    sh.passwordLength = int(s.password.size());
    sh.keyLength = int(s.key.size());
    sh.caret = (now / 500) % 2 == 0;
    sh.userCode = s.userCode;
    sh.secondsLeft = s.deviceCode.empty() ? 0 : int((s.expiresAt - std::min(s.expiresAt, now)) / 1000);
    sh.error = s.error;
    sh.busy = s.busy || !s.deviceCode.empty();
    sh.login = s.login;  // 3.0.11-beta.181: the forms
    sh.gamePassLength = int(s.gamePass.size());
    sh.newPassLength = int(s.newPass.size());
    sh.sex = s.sex;
    sh.agree = s.agree;  // 3.0.11-beta.181: the registration
    if (s.way == ss::Way::Register) freeFrame(s, now);
    sh.loginFree = s.loginFree;
    sh.hover = 0;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        sh.hover = int(ss::hitTest(s.layout, float(cx) * s.k - float(s.layout.x), float(cy) * s.k - float(s.layout.y)));
    const std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' +
                            std::to_string(int(sh.way)) + std::to_string(int(sh.focus)) + (sh.caret ? "|" : "") +
                            sh.email + '\x01' + std::to_string(sh.passwordLength) + ',' + std::to_string(sh.keyLength) +
                            '\x01' + sh.code2fa + '\x01' + sh.userCode + std::to_string(sh.secondsLeft) + sh.error +
                            (sh.busy ? "b" : "") + std::to_string(sh.hover) + (s.twofa ? "t" : "") + sh.login +
                            '\x01' + std::to_string(sh.gamePassLength) + ',' + std::to_string(sh.newPassLength) +
                            sh.sex + (sh.agree ? "A" : "") + std::to_string(sh.loginFree) + '#' +
                            std::to_string(uistyle::generation()) + (uaui::readyAnywhere() ? "u" : "");
    if (key != s.picKey) {
        s.picKey = key;
        render(s);
    }
}

// 3.0.18-beta.40 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): this window in RmlUi's
// markup, line by line as core sitesignin paints it (its boxes, icons, circles); the words where it places them
static void fillSheet(State& s, uisheet::Content& ui) {
    using namespace uagfx::sitesignin;
    const auto& L = s.layout;
    const auto& sh = s.show;
    [[maybe_unused]] constexpr uint32_t kCard = 0x10141C;
    [[maybe_unused]] constexpr uint32_t kWhite = 0xFFFFFF;
    [[maybe_unused]] constexpr uint32_t kGold = 0xC9A55B;
    [[maybe_unused]] constexpr uint32_t kGold2 = 0xE6CB8A;
    [[maybe_unused]] constexpr uint32_t kText = 0xEEF2F8;
    [[maybe_unused]] constexpr uint32_t kMuted = 0x9AA3B5;
    [[maybe_unused]] constexpr uint32_t kBlack = 0x000000;
    [[maybe_unused]] constexpr uint32_t kLink = 0x8CB8FF;
    [[maybe_unused]] constexpr uint32_t kRed = 0xFF5A4E;
    [[maybe_unused]] constexpr uint32_t kChosen = 0xF5E6BE;
    [[maybe_unused]] constexpr float kW = 380;
    [[maybe_unused]] constexpr float kPad = 24;
    [[maybe_unused]] constexpr float kTitleH = 30;
    [[maybe_unused]] constexpr float kSubH = 20;
    [[maybe_unused]] constexpr float kWaysH = 32;
    [[maybe_unused]] constexpr float kLabelH = 18;
    [[maybe_unused]] constexpr float kFieldH = 38;
    [[maybe_unused]] constexpr float kGap = 12;
    [[maybe_unused]] constexpr float kButtonH = 42;
    [[maybe_unused]] constexpr float kErrorH = 18;
    [[maybe_unused]] constexpr float kBackH = 20;
    [[maybe_unused]] constexpr float kCodeH = 52;
    [[maybe_unused]] constexpr float kWaitH = 20;
    ui.w = float(L.w), ui.h = float(L.h);
    const auto put = [&](const panel::Picture& p, const panel::Box& box, float k = 1.0f) {
        if (p.width > 0)
            ui.icons.push_back(uisheet::Icon{std::round(box.x + (box.w - float(p.width)) / 2),
                                            std::round(box.y + (box.h - float(p.height)) / 2), float(p.width),
                                            float(p.height), k, s.sheet.icon(&p)});
    };
    [[maybe_unused]] const auto circle = [&](float cx, float cy, float r, uint32_t rgb, float a) {
        ui.rect(ui.under, panel::Box{cx - r, cy - r, 2 * r, 2 * r}, r, rgb, a);
    };
    [[maybe_unused]] const auto icon = [&](uiicons::Icon which, float x, float y, float side, uint32_t rgb, float a,
                                           float stroke) {
        const int px = std::max(4, int(std::ceil(side)));
        const std::string key = "u" + std::to_string(int(which)) + ':' + std::to_string(px) + ':' +
                                std::to_string(rgb) + ':' + std::to_string(int(a * 100));
        const std::string src = s.sheet.made(key, px, px, [&](panel::Picture& p) {
            uiicons::draw(p, which, 0, 0, float(px), rgb, a, stroke);
        });
        ui.icons.push_back(uisheet::Icon{x, y, float(px), float(px), 1, src});
    };
    (void)put;

    [&] {
        const float sc = L.s;
        ui.rect(ui.under, L.glass, 14 * sc, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * sc);
        if (L.ways[0].w > 0) {
            const panel::Box all{L.ways[0].x, L.ways[0].y, L.ways[2].x + L.ways[2].w - L.ways[0].x, L.ways[0].h};
            ui.rect(ui.under, all, all.h / 2, kBlack, 0.30f, kWhite, 0.12f, 1.0f * sc);
            ui.rect(ui.under, L.ways[int(sh.way)], L.ways[0].h / 2, kGold, 0.26f);
        }
        if (L.sex[0].w > 0) {  // 3.0.11-beta.181: the new login's sex
            const panel::Box both{L.sex[0].x, L.sex[0].y, L.sex[1].x + L.sex[1].w - L.sex[0].x, L.sex[0].h};
            ui.rect(ui.under, both, both.h / 2, kBlack, 0.30f, kWhite, 0.12f, 1.0f * sc);
            if (sh.sex == 'M' || sh.sex == 'F')
                ui.rect(ui.under, L.sex[sh.sex == 'F' ? 1 : 0], L.sex[0].h / 2, kGold, 0.26f);
        }
        const auto field = [&](const panel::Box& b, Field f, Part p) {
            if (b.h <= 0) return;
            const bool focused = sh.focus == f, hot = sh.hover == int(p);
            ui.rect(ui.under, b, 8 * sc, kBlack, 0.35f, focused ? kGold : kWhite, focused ? 0.85f : hot ? 0.30f : 0.16f,
                    (focused ? 1.5f : 1.0f) * sc);
        };
        field(L.email, Field::Email, Part::Email);
        field(L.password, Field::Password, Part::Password);
        field(L.code, Field::Code, Part::Code);
        field(L.key, Field::Key, Part::Key);
        field(L.login, Field::Login, Part::Login);
        field(L.gamePass, Field::GamePass, Part::GamePass);
        field(L.newPass, Field::NewPass, Part::NewPass);
        if (L.codeBig.h > 0 && !sh.userCode.empty())
            ui.rect(ui.under, L.codeBig, 10 * sc, kBlack, 0.35f, kGold, 0.55f, 1.0f * sc);
        if (L.agree.h > 0)  // 3.0.11-beta.181: the rules' tick
            ui.rect(ui.under, L.agreeTick, 4 * sc, sh.agree ? kGold : kWhite, sh.agree ? 0.85f : 0.06f,
                    sh.agree ? kGold : kWhite, sh.agree ? 1.0f : 0.35f, 1.0f * sc);
        const bool hot = sh.hover == int(Part::Button);
        ui.rect(ui.under, L.button, 9 * sc, kGold, sh.busy ? 0.10f : hot ? 0.36f : 0.24f, kGold, sh.busy ? 0.4f : 0.85f,
                1.0f * sc);
    }();
    ui.say(uagfx::sitesignin::words(L, s.show, texts()));
}

void frame(double k, int targetWidth, int targetHeight) {
    frameInner(k, targetWidth, targetHeight);
    State& s = state();
    s.sheet.anywhere = true;
    s.sheetShown = s.sheet.frame("site sign-in", uaui::readyAnywhere() && s.open && s.k > 0 && s.sheetGen > 0, true,
                                 s.layout.x, s.layout.y, s.sheetGen, [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.open || s.sheetShown || s.k <= 0 || s.picture.width <= 0) return;
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
}

bool mouse(UINT m, float tx, float ty) {
    State& s = state();
    if (!s.open) return false;
    const float x = tx - float(s.layout.x), y = ty - float(s.layout.y);
    if (!s.layout.glass.contains(x, y)) return false;
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    s.picKey.clear();
    switch (ss::hitTest(s.layout, x, y)) {
        case ss::Part::Way0:
        case ss::Part::Way1:
        case ss::Part::Way2: {
            const ss::Way w = ss::Way(int(ss::hitTest(s.layout, x, y)) - int(ss::Part::Way0));
            if (w != s.way && !s.busy) {
                s.way = w;
                s.error.clear();
                s.deviceCode.clear();
                s.userCode.clear();
                const std::vector<ss::Field> f = fieldsOf(s);
                s.focus = f.empty() ? ss::Field::None : f.front();
            }
            return true;
        }
        case ss::Part::Email: s.focus = ss::Field::Email; return true;
        case ss::Part::Password: s.focus = ss::Field::Password; return true;
        case ss::Part::Code: s.focus = ss::Field::Code; return true;
        case ss::Part::Key: s.focus = ss::Field::Key; return true;
        case ss::Part::Login: s.focus = ss::Field::Login; return true;  // 3.0.11-beta.181: the forms
        case ss::Part::GamePass: s.focus = ss::Field::GamePass; return true;
        case ss::Part::NewPass: s.focus = ss::Field::NewPass; return true;
        case ss::Part::Sex0: s.sex = 'M'; return true;
        case ss::Part::Sex1: s.sex = 'F'; return true;
        case ss::Part::Agree: s.agree = !s.agree; return true;  // 3.0.11-beta.181: the registration
        case ss::Part::Rules: {
            const settings::Lang l = language();
            openPage(l == settings::Lang::Uk   ? L"https://uaro.kiev.ua/uk/rules"
                     : l == settings::Lang::Ru ? L"https://uaro.kiev.ua/ru/rules"
                                               : L"https://uaro.kiev.ua/en/rules");
            return true;
        }
        case ss::Part::HaveAccount:  // the sign-in first, then «+ Новий ігровий логін»
            s.way = ss::Way::Password;
            s.focus = ss::Field::Email;
            s.error.clear();
            return true;
        case ss::Part::Button: press(s); return true;
        case ss::Part::Back: close(); return true;
        case ss::Part::None: return true;
    }
    return true;
}

bool key(UINT m, WPARAM wp) {
    State& s = state();
    if (!s.open) return false;
    s.picKey.clear();
    if (m == WM_KEYDOWN) {
        if (wp == VK_ESCAPE) {
            close();
        } else if (wp == VK_RETURN) {
            press(s);
        } else if (wp == VK_TAB) {
            const std::vector<ss::Field> f = fieldsOf(s);
            if (!f.empty()) {
                const auto at = std::find(f.begin(), f.end(), s.focus);
                s.focus = at == f.end() || at + 1 == f.end() ? f.front() : *(at + 1);
            }
        } else if (wp == VK_BACK) {
            if (std::string* t = fieldText(s, s.focus)) {
                while (!t->empty() && (uint8_t(t->back()) & 0xC0) == 0x80) t->pop_back();
                if (!t->empty()) t->pop_back();
            }
        }
        return true;
    }
    if (m == WM_CHAR) {
        if (wp == 0x16) {  // Ctrl+V
            paste(s);
        } else if (wp >= 0x20 && wp != 0x7F) {
            if (std::string* t = fieldText(s, s.focus))
                if (t->size() < kFieldMost) *t += utf8Of(std::wstring(1, wchar_t(wp)));
        }
        return true;
    }
    return m == WM_KEYUP || m == WM_SYSKEYDOWN || m == WM_SYSCHAR;
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.open || s.layout.w <= 0) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

bool takeSignedIn() {
    State& s = state();
    const bool was = s.signedIn;
    s.signedIn = false;
    return was;
}

}  // namespace uagfx::win::sitesigninwin
