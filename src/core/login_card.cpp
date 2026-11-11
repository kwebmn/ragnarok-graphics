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
constexpr float kNoteLine = 17;         // 3.0.11-beta.176: the first launch's note, two lines

}  // namespace

Layout layout(int targetW, int targetH, float scale, loginerr::Field errorAt) {
    Options o;
    o.errorAt = errorAt;
    return layout(targetW, targetH, scale, o);
}

Layout layout(int targetW, int targetH, float scale, const Options& o) {
    const loginerr::Field errorAt = o.errorAt;
    const bool note = o.note, back = o.back;
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
    y += kBoxH * s + 4 * s;
    L.addShelf = panel::Box{pad, y, fw, kBoxH * s};  // 3.0.11-beta.175
    L.addTick = panel::Box{pad, y + 3 * s, 18 * s, 18 * s};
    y += kBoxH * s + kGap * s;
    const float bw = (fw - kGap * s) / 2;
    L.enter = panel::Box{pad, y, bw, kButtonH * s};
    L.exit = panel::Box{pad + bw + kGap * s, y, bw, kButtonH * s};
    y += kButtonH * s + kGap * s;
    L.link = panel::Box{pad, y, fw / 2, kLinkH * s};  // 3.0.11-beta.177: «Створити акаунт» | «Забули пароль?»
    L.forgot = panel::Box{pad + fw / 2, y, fw / 2, kLinkH * s};
    y += kLinkH * s;
    if (note) {  // 3.0.11-beta.176
        L.note = panel::Box{pad, y + 6 * s, fw, 2 * kNoteLine * s};
        y += 6 * s + 2 * kNoteLine * s;
    }
    if (o.site) {  // 3.0.11-beta.180: the site account's line
        L.site = panel::Box{pad, y + 8 * s, fw, kLinkH * s};
        L.siteOut = panel::Box{pad + fw - 70 * s, y + 8 * s, 70 * s, kLinkH * s};
        y += 8 * s + kLinkH * s;
    }
    y += pad;
    if (back) L.back = panel::Box{pad - 6 * s, pad - 6 * s, 96 * s, 24 * s};  // 3.0.11-beta.176: the title row's left
    if (o.drop) L.drop = panel::Box{L.account.x + L.account.w - 36 * s, L.account.y, 36 * s, L.account.h};  // .177
    L.eye = panel::Box{L.password.x + L.password.w - 38 * s, L.password.y, 38 * s, L.password.h};
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    // 3.0.11-beta.172: placed by its height without a line under a field - the top stays, it grows downward
    const float grown = (L.error.h > 0 ? 2 * kErrorH * s : 0) + (note ? 6 * s + 2 * kNoteLine * s : 0) +
                        (o.site ? 8 * s + kLinkH * s : 0);
    L.y = std::max(0, targetH / 2 - int(std::ceil(y - grown)) / 2);
    return L;
}

Part hitTest(const Layout& L, float x, float y) {
    if (L.back.w > 0 && L.back.contains(x, y)) return Part::Shelf;
    if (L.errorAction.h > 0 && L.errorAction.contains(x, y)) return Part::ErrorAction;
    if (L.drop.w > 0 && L.drop.contains(x, y)) return Part::Drop;  // 3.0.11-beta.177
    if (L.eye.w > 0 && L.eye.contains(x, y)) return Part::Eye;
    if (L.account.contains(x, y)) return Part::Account;
    if (L.password.contains(x, y)) return Part::Password;
    if (L.remember.contains(x, y)) return Part::Remember;
    if (L.addShelf.contains(x, y)) return Part::AddShelf;
    if (L.enter.contains(x, y)) return Part::Enter;
    if (L.exit.contains(x, y)) return Part::Exit;
    if (L.site.h > 0 && L.siteOut.contains(x, y)) return Part::SiteOut;  // 3.0.11-beta.180 (the chip's only)
    if (L.site.h > 0 && L.site.contains(x, y)) return Part::Site;
    if (L.link.contains(x, y)) return Part::Register;
    if (L.forgot.contains(x, y)) return Part::Forgot;
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

DropLayout dropLayout(const Layout& L, size_t logins) {
    return dropAt(L.account.x, L.account.y + L.account.h + 4 * L.s, L.account.w, logins, L.s);
}

DropLayout dropAt(float x, float y0, float w, size_t logins, float scale) {
    DropLayout D;
    const float s = scale;
    constexpr float kRowH = 46, kOtherH = 36, kPadIn = 6;
    D.x = x;
    D.y = y0;
    D.w = w;
    float y = kPadIn * s;
    for (size_t i = 0; i < logins; ++i) {
        D.rows.push_back(panel::Box{D.x + kPadIn * s, D.y + y, D.w - 2 * kPadIn * s, kRowH * s});
        y += kRowH * s;
    }
    D.rows.push_back(panel::Box{D.x + kPadIn * s, D.y + y, D.w - 2 * kPadIn * s, kOtherH * s});
    y += kOtherH * s + kPadIn * s;
    D.h = y;
    return D;
}

int dropHit(const DropLayout& D, float x, float y) {
    for (size_t i = 0; i < D.rows.size(); ++i)
        if (D.rows[i].contains(x, y)) return int(i);
    return -1;
}

std::vector<Words> dropWords(const DropLayout& D, const std::vector<DropRow>& rows, int hover, const std::string& other,
                             float scale) {
    std::vector<Words> out;
    const float s = scale;
    for (size_t i = 0; i < rows.size() && i < D.rows.size(); ++i) {
        const panel::Box r{D.rows[i].x - D.x + 10 * s, D.rows[i].y - D.y, D.rows[i].w - 20 * s, D.rows[i].h};
        const bool two = !rows[i].line.empty();
        out.push_back(Words{panel::Box{r.x, r.y + (two ? 4 * s : 0), r.w, two ? 20 * s : r.h}, rows[i].login,
                            hover == int(i) ? kGold2 : kText, 14.5f * s, true, Align::Left});
        if (two)
            out.push_back(Words{panel::Box{r.x, r.y + 24 * s, r.w, 18 * s}, rows[i].line, kMuted, 12 * s, false,
                                Align::Left});
    }
    if (!D.rows.empty()) {
        const panel::Box& o = D.rows.back();
        out.push_back(Words{panel::Box{o.x - D.x + 10 * s, o.y - D.y, o.w - 20 * s, o.h}, other,
                            hover == int(D.rows.size()) - 1 ? kGold2 : kLink, 13.5f * s, false, Align::Left});
    }
    return out;
}

void paintTriangle(panel::Picture& out, float cx, float cy, float w, uint32_t rgb, float a) {
    const float h = w / 2;  // rows a pixel high, narrower each one down
    const int rows = std::max(1, int(std::lround(h)));
    for (int i = 0; i < rows; ++i) {
        const float rw = w * (1.0f - float(i) / float(rows));
        panel::roundRect(out, panel::Box{cx - rw / 2, cy - h / 2 + float(i), rw, 1.0f}, 0, rgb, a);
    }
}

void paintEye(panel::Picture& out, const panel::Box& b, uint32_t rgb, float a, float scale) {
    panel::roundRect(out, b, b.h / 2, rgb, 0.0f, rgb, a, 1.5f * scale);  // the lids
    panel::circle(out, b.x + b.w / 2, b.y + b.h / 2, b.h * 0.2f, rgb, a);  // the pupil
}

}  // namespace uagfx::logincard
