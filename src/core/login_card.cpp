#include "core/login_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::logincard {
namespace {

// At scale 1: the card 380 wide, 24 in; the title 34, a label 18 over its field 38; the box 24; the buttons 42; the
// link 20
constexpr float kW = 380, kPad = 24, kTitleH = 34, kSubH = 20, kLabelH = 18, kFieldH = 38, kBoxH = 24, kButtonH = 42;
constexpr float kLinkH = 20, kGap = 12;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000, kLink = 0x8CB8FF, kRed = 0xFF5A4E;
constexpr float kErrorH = 18;  // 3.0.11-beta.172: a refusal's line and its action's under a field
constexpr uint32_t kAmber = 0xF2C46B;  // 3.0.11-beta.172: a hint's words

}  // namespace

Layout layout(int targetW, int targetH, float scale, loginerr::Field errorAt) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, fw = w - 2 * pad;
    float y = pad;
    L.title = panel::Box{pad, y, fw, kTitleH * s};
    y += kTitleH * s;
    L.subtitle = panel::Box{pad, y, fw, kSubH * s};
    y += kSubH * s + kGap * s;
    L.accountLabel = panel::Box{pad, y, fw, kLabelH * s};
    y += kLabelH * s;
    L.account = panel::Box{pad, y, fw, kFieldH * s};
    y += kFieldH * s + 8 * s;
    const auto errorLines = [&] {  // the refusal under this field: its words, then the action's link
        L.error = panel::Box{pad, y - 4 * s, fw, kErrorH * s};
        L.errorAction = panel::Box{pad, y - 4 * s + kErrorH * s, fw, kErrorH * s};
        y += 2 * kErrorH * s;
    };
    if (errorAt == loginerr::Field::Account) errorLines();
    L.passwordLabel = panel::Box{pad, y, fw, kLabelH * s};
    y += kLabelH * s;
    L.password = panel::Box{pad, y, fw, kFieldH * s};
    y += kFieldH * s + 10 * s;
    if (errorAt == loginerr::Field::Password) {
        y -= 2 * s;
        errorLines();
        y += 2 * s;
    }
    L.remember = panel::Box{pad, y, fw, kBoxH * s};
    L.tick = panel::Box{pad, y + 3 * s, 18 * s, 18 * s};
    y += kBoxH * s + kGap * s;
    const float bw = (fw - kGap * s) / 2;
    L.enter = panel::Box{pad, y, bw, kButtonH * s};
    L.exit = panel::Box{pad + bw + kGap * s, y, bw, kButtonH * s};
    y += kButtonH * s + kGap * s;
    L.link = panel::Box{pad, y, fw, kLinkH * s};
    y += kLinkH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    // 3.0.11-beta.172: placed by its height without a line under a field - the top stays, it grows downward
    const float lines = L.error.h > 0 ? 2 * kErrorH * s : 0;
    L.y = std::max(0, targetH / 2 - int(std::ceil(y - lines)) / 2);
    return L;
}

Part hitTest(const Layout& L, float x, float y) {
    if (L.errorAction.h > 0 && L.errorAction.contains(x, y)) return Part::ErrorAction;
    if (L.account.contains(x, y)) return Part::Account;
    if (L.password.contains(x, y)) return Part::Password;
    if (L.remember.contains(x, y)) return Part::Remember;
    if (L.enter.contains(x, y)) return Part::Enter;
    if (L.exit.contains(x, y)) return Part::Exit;
    if (L.link.contains(x, y)) return Part::Register;
    return Part::None;
}

FieldPlaces fieldPlaces(uint32_t first, uint32_t firstMask, uint32_t second, uint32_t secondMask) {
    FieldPlaces f;
    if ((firstMask != 0) == (secondMask != 0)) return f;
    f.account = firstMask ? second : first;
    f.password = firstMask ? first : second;
    return f;
}

int gameButton(Part p) {
    switch (p) {
        case Part::Enter: return 0x78;
        case Part::Exit: return 0x9B;
        case Part::Remember: return 0x93;
        default: return 0;
    }
}

void paint(panel::Picture& out, const Layout& L, const Fields& f, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 14 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    const auto field = [&](const panel::Box& b, bool focused, bool hot, bool refused) {
        const float edge = refused ? 0.9f : focused ? 0.85f : hot ? 0.30f : 0.16f;
        panel::roundRect(out, b, 8 * s, kBlack, 0.35f, refused ? kRed : focused ? kGold : kWhite, edge,
                         (focused || refused ? 1.5f : 1.0f) * s);
    };
    const bool bad = !f.error.empty();
    field(L.account, f.focus == Focus::Account, sh.hover == int(Part::Account),
          bad && f.errorField == loginerr::Field::Account);
    field(L.password, f.focus == Focus::Password, sh.hover == int(Part::Password),
          bad && f.errorField == loginerr::Field::Password);
    panel::roundRect(out, L.tick, 4 * s, f.remember ? kGold : kWhite, f.remember ? 0.85f : 0.06f,
                     f.remember ? kGold : kWhite, f.remember ? 1.0f : 0.35f, 1.0f * s);
    const bool hotEnter = sh.hover == int(Part::Enter), hotExit = sh.hover == int(Part::Exit);
    panel::roundRect(out, L.enter, 9 * s, kGold, hotEnter ? 0.36f : 0.24f, kGold, 0.85f, 1.0f * s);
    panel::roundRect(out, L.exit, 9 * s, kWhite, hotExit ? 0.12f : 0.06f, kWhite, 0.16f, 1.0f * s);
}

std::vector<Words> words(const Layout& L, const Fields& f, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 22 * s, true, Align::Center});
    out.push_back(Words{L.subtitle, t.subtitle, kMuted, 12.5f * s, false, Align::Center});
    out.push_back(Words{L.accountLabel, t.account, kMuted, 12 * s, true, Align::Left});
    out.push_back(Words{L.passwordLabel, t.password, kMuted, 12 * s, true, Align::Left});
    const auto inside = [&](const panel::Box& b) { return panel::Box{b.x + 12 * s, b.y, b.w - 24 * s, b.h}; };
    std::string dots;
    for (int i = 0; i < std::clamp(f.passwordLength, 0, 24); ++i) dots += "\xE2\x80\xA2";
    out.push_back(Words{inside(L.account), f.account + (f.focus == Focus::Account && f.caret ? "|" : ""), kText,
                        14 * s, false, Align::Left});
    out.push_back(Words{inside(L.password), dots + (f.focus == Focus::Password && f.caret ? "|" : ""), kText, 14 * s,
                        false, Align::Left});
    if (f.remember)
        out.push_back(Words{L.tick, "\xE2\x9C\x93", kBlack, 13 * s, true, Align::Center});  // ✓
    out.push_back(Words{panel::Box{L.remember.x + 28 * s, L.remember.y, L.remember.w - 28 * s, L.remember.h},
                        t.remember, kText, 12.5f * s, false, Align::Left});
    out.push_back(Words{L.enter, t.enter, kText, 14 * s, true, Align::Center});
    out.push_back(Words{L.exit, t.exit, kText, 14 * s, false, Align::Center});
    out.push_back(Words{L.link, t.register_, sh.hover == int(Part::Register) ? kGold2 : kLink, 12 * s, false,
                        Align::Center});
    if (f.error.empty() && !f.hint.empty() && L.error.h > 0)
        out.push_back(Words{L.error, f.hint, kAmber, 12 * s, false, Align::Left});
    if (!f.error.empty() && L.error.h > 0) {
        out.push_back(Words{L.error, f.error, kRed, 12 * s, false, Align::Left});
        const std::string& act = f.errorAction == loginerr::Action::CreateAccount   ? t.createAccount
                                 : f.errorAction == loginerr::Action::ResetPassword ? t.resetPassword
                                                                                    : std::string();
        if (!act.empty())
            out.push_back(Words{L.errorAction, act, sh.hover == int(Part::ErrorAction) ? kGold2 : kLink, 12 * s,
                                false, Align::Left});
    }
    return out;
}

}  // namespace uagfx::logincard
