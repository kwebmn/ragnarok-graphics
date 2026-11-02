#include "core/site_signin.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::sitesignin {
namespace {

// At scale 1 the classic card's measures: 380 wide, 24 in; a label 18 over its field 38; the ways' segments 32 high
constexpr float kW = 380, kPad = 24, kTitleH = 30, kSubH = 20, kWaysH = 32, kLabelH = 18, kFieldH = 38, kGap = 12;
constexpr float kButtonH = 42, kErrorH = 18, kBackH = 20, kCodeH = 52, kWaitH = 20;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000, kLink = 0x8CB8FF, kRed = 0xFF5A4E, kChosen = 0xF5E6BE;

std::string fill(const std::string& pattern, const std::string& v) {
    const size_t at = pattern.find("{1}");
    return at == std::string::npos ? pattern : pattern.substr(0, at) + v + pattern.substr(at + 3);
}

}  // namespace

Layout layout(int targetW, int targetH, float scale, Way way, bool twofa) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, fw = w - 2 * pad;
    float y = pad;
    L.back = panel::Box{pad - 6 * s, pad - 6 * s, 96 * s, kBackH * s + 4 * s};
    y += kBackH * s;
    L.title = panel::Box{pad, y, fw, kTitleH * s};
    y += kTitleH * s;
    L.subtitle = panel::Box{pad, y, fw, kSubH * s};
    y += kSubH * s + kGap * s;
    if (!isForm(way)) {
        for (int i = 0; i < 3; ++i) L.ways[i] = panel::Box{pad + i * fw / 3, y, fw / 3, kWaysH * s};
        y += kWaysH * s + kGap * s + 4 * s;
    }
    const auto field = [&](panel::Box& label, panel::Box& box) {
        label = panel::Box{pad, y, fw, kLabelH * s};
        y += kLabelH * s;
        box = panel::Box{pad, y, fw, kFieldH * s};
        y += kFieldH * s + 10 * s;
    };
    if (way == Way::Password) {
        field(L.emailLabel, L.email);
        field(L.passwordLabel, L.password);
        if (twofa) field(L.codeLabel, L.code);
    } else if (way == Way::Key) {
        field(L.keyLabel, L.key);
    } else if (way == Way::NewLogin || way == Way::Link) {  // 3.0.11-beta.181
        field(L.loginLabel, L.login);
        field(L.gamePassLabel, L.gamePass);
        if (way == Way::NewLogin) {
            L.sex[0] = panel::Box{pad, y, fw / 2, kWaysH * s};
            L.sex[1] = panel::Box{pad + fw / 2, y, fw / 2, kWaysH * s};
            y += kWaysH * s + 10 * s;
        }
    } else if (way == Way::GamePassword) {
        field(L.gamePassLabel, L.gamePass);
        field(L.newPassLabel, L.newPass);
    } else {
        L.codeBig = panel::Box{pad, y, fw, kCodeH * s};
        y += kCodeH * s;
        L.wait = panel::Box{pad, y, fw, kWaitH * s};
        y += kWaitH * s + 10 * s;
    }
    L.error = panel::Box{pad, y, fw, kErrorH * s};
    y += kErrorH * s + 6 * s;
    L.button = panel::Box{pad, y, fw, kButtonH * s};
    y += kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

bool isForm(Way w) { return w == Way::NewLogin || w == Way::Link || w == Way::GamePassword; }

Part hitTest(const Layout& L, float x, float y) {
    if (L.back.contains(x, y)) return Part::Back;
    if (L.login.h > 0 && L.login.contains(x, y)) return Part::Login;  // 3.0.11-beta.181: the forms
    if (L.gamePass.h > 0 && L.gamePass.contains(x, y)) return Part::GamePass;
    if (L.newPass.h > 0 && L.newPass.contains(x, y)) return Part::NewPass;
    if (L.sex[0].w > 0 && L.sex[0].contains(x, y)) return Part::Sex0;
    if (L.sex[1].w > 0 && L.sex[1].contains(x, y)) return Part::Sex1;
    for (int i = 0; i < 3; ++i)
        if (L.ways[i].contains(x, y)) return Part(int(Part::Way0) + i);
    if (L.email.h > 0 && L.email.contains(x, y)) return Part::Email;
    if (L.password.h > 0 && L.password.contains(x, y)) return Part::Password;
    if (L.code.h > 0 && L.code.contains(x, y)) return Part::Code;
    if (L.key.h > 0 && L.key.contains(x, y)) return Part::Key;
    if (L.button.contains(x, y)) return Part::Button;
    return Part::None;
}

void paint(panel::Picture& out, const Layout& L, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 14 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    if (L.ways[0].w > 0) {
        const panel::Box all{L.ways[0].x, L.ways[0].y, L.ways[2].x + L.ways[2].w - L.ways[0].x, L.ways[0].h};
        panel::roundRect(out, all, all.h / 2, kBlack, 0.30f, kWhite, 0.12f, 1.0f * s);
        panel::roundRect(out, L.ways[int(sh.way)], L.ways[0].h / 2, kGold, 0.26f);
    }
    if (L.sex[0].w > 0) {  // 3.0.11-beta.181: the new login's sex
        const panel::Box both{L.sex[0].x, L.sex[0].y, L.sex[1].x + L.sex[1].w - L.sex[0].x, L.sex[0].h};
        panel::roundRect(out, both, both.h / 2, kBlack, 0.30f, kWhite, 0.12f, 1.0f * s);
        panel::roundRect(out, L.sex[sh.sex == 'F' ? 1 : 0], L.sex[0].h / 2, kGold, 0.26f);
    }
    const auto field = [&](const panel::Box& b, Field f, Part p) {
        if (b.h <= 0) return;
        const bool focused = sh.focus == f, hot = sh.hover == int(p);
        panel::roundRect(out, b, 8 * s, kBlack, 0.35f, focused ? kGold : kWhite, focused ? 0.85f : hot ? 0.30f : 0.16f,
                         (focused ? 1.5f : 1.0f) * s);
    };
    field(L.email, Field::Email, Part::Email);
    field(L.password, Field::Password, Part::Password);
    field(L.code, Field::Code, Part::Code);
    field(L.key, Field::Key, Part::Key);
    field(L.login, Field::Login, Part::Login);
    field(L.gamePass, Field::GamePass, Part::GamePass);
    field(L.newPass, Field::NewPass, Part::NewPass);
    if (L.codeBig.h > 0 && !sh.userCode.empty())
        panel::roundRect(out, L.codeBig, 10 * s, kBlack, 0.35f, kGold, 0.55f, 1.0f * s);
    const bool hot = sh.hover == int(Part::Button);
    panel::roundRect(out, L.button, 9 * s, kGold, sh.busy ? 0.10f : hot ? 0.36f : 0.24f, kGold, sh.busy ? 0.4f : 0.85f,
                     1.0f * s);
}

std::vector<Words> words(const Layout& L, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{panel::Box{L.back.x + 6 * s, L.back.y, L.back.w, L.back.h}, t.back,
                        sh.hover == int(Part::Back) ? kGold2 : kLink, 12.5f * s, false, Align::Left});
    const bool form = isForm(sh.way);
    out.push_back(Words{L.title, form ? t.forms[int(sh.way) - int(Way::NewLogin)] : t.title, kText, 19 * s, true,
                        Align::Center});
    if (!form) out.push_back(Words{L.subtitle, t.subtitle, kMuted, 12.5f * s, false, Align::Center});
    for (int i = 0; i < 3 && L.ways[0].w > 0; ++i)
        out.push_back(Words{L.ways[i], t.ways[i], i == int(sh.way) ? kChosen : kMuted, 12 * s, i == int(sh.way),
                            Align::Center});
    const auto inside = [&](const panel::Box& b) { return panel::Box{b.x + 12 * s, b.y, b.w - 24 * s, b.h}; };
    const auto dots = [](int n) {
        std::string d;
        for (int i = 0; i < std::clamp(n, 0, 32); ++i) d += "\xE2\x80\xA2";
        return d;
    };
    const auto caret = [&](Field f) { return sh.focus == f && sh.caret ? std::string("|") : std::string(); };
    const auto field = [&](const panel::Box& label, const std::string& name, const panel::Box& box,
                           const std::string& text, Field f) {
        if (box.h <= 0) return;
        out.push_back(Words{label, name, kMuted, 12 * s, true, Align::Left});
        out.push_back(Words{inside(box), text + caret(f), kText, 14 * s, false, Align::Left});
    };
    field(L.emailLabel, t.email, L.email, sh.email, Field::Email);
    field(L.passwordLabel, t.password, L.password, dots(sh.passwordLength), Field::Password);
    field(L.codeLabel, t.code2fa, L.code, sh.code2fa, Field::Code);
    field(L.keyLabel, t.key, L.key, dots(sh.keyLength), Field::Key);
    field(L.loginLabel, t.login, L.login, sh.login, Field::Login);  // 3.0.11-beta.181: the forms
    field(L.gamePassLabel, sh.way == Way::GamePassword ? t.currentPass : t.gamePass, L.gamePass,
          dots(sh.gamePassLength), Field::GamePass);
    field(L.newPassLabel, t.newPass, L.newPass, dots(sh.newPassLength), Field::NewPass);
    for (int i = 0; i < 2 && L.sex[0].w > 0; ++i) {
        const bool on = (sh.sex == 'F') == (i == 1);
        out.push_back(Words{L.sex[i], t.sexes[i], on ? kChosen : kMuted, 12 * s, on, Align::Center});
    }
    if (L.codeBig.h > 0 && !sh.userCode.empty()) {
        out.push_back(Words{L.codeBig, sh.userCode, kChosen, 26 * s, true, Align::Center});
        out.push_back(Words{L.wait, fill(t.waiting, std::to_string(sh.secondsLeft)), kMuted, 12.5f * s, false,
                            Align::Center});
    }
    if (!sh.error.empty()) out.push_back(Words{L.error, sh.error, kRed, 12 * s, false, Align::Left});
    const std::string& button = sh.way == Way::Browser        ? t.startBrowser
                                : sh.way == Way::NewLogin     ? t.create
                                : sh.way == Way::Link         ? t.link
                                : sh.way == Way::GamePassword ? t.change
                                                              : t.signIn;
    out.push_back(Words{L.button, button, sh.busy ? kMuted : kText, 14 * s, true, Align::Center});
    return out;
}

}  // namespace uagfx::sitesignin
