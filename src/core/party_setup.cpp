#include "core/party_setup.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::partysetup {
namespace {

// At scale 1: the card 420 wide, 18 in; its title 32; a row's name 20 over its two choices 34; the buttons 38
constexpr float kW = 420, kPad = 18, kTitleH = 32, kLabelH = 20, kChoiceH = 34, kButtonH = 38, kGap = 10;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kText = 0xEEF2F8, kMuted = 0x9AA3B5;

}  // namespace

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, cw = (w - 2 * pad - kGap * s) / 2;
    L.title = panel::Box{pad, pad, w - 2 * pad, kTitleH * s};
    float y = pad + kTitleH * s + 4 * s;
    for (int r = 0; r < 3; ++r) {
        L.labels[r] = panel::Box{pad, y, w - 2 * pad, kLabelH * s};
        y += kLabelH * s + 2 * s;
        L.choices[r][0] = panel::Box{pad, y, cw, kChoiceH * s};
        L.choices[r][1] = panel::Box{pad + cw + kGap * s, y, cw, kChoiceH * s};
        y += kChoiceH * s + 10 * s;
    }
    L.note = panel::Box{pad, y, w - 2 * pad, 18 * s};
    y += 22 * s;
    const float bw = (w - 2 * pad - kGap * s) / 2;
    L.ok = panel::Box{pad, y, bw, kButtonH * s};
    L.cancel = panel::Box{pad + bw + kGap * s, y, bw, kButtonH * s};
    y += kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 2; ++c)
            if (L.choices[r][c].contains(x, y)) return Hit{Part::Choice, r, c};
    if (L.ok.contains(x, y)) return Hit{Part::Ok, -1, -1};
    if (L.cancel.contains(x, y)) return Hit{Part::Cancel, -1, -1};
    return Hit{};
}

void paint(panel::Picture& out, const Layout& L, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    for (int r = 0; r < 3; ++r) {
        if (!sh.present[r]) continue;
        if (sh.focus == r)
            panel::roundRect(out,
                             panel::Box{L.choices[r][0].x - 4 * s, L.choices[r][0].y - 3 * s,
                                        L.choices[r][1].x + L.choices[r][1].w - L.choices[r][0].x + 8 * s,
                                        L.choices[r][0].h + 6 * s},
                             10 * s, kGold, 0.0f, kGold, 0.7f, 1.5f * s);
        for (int c = 0; c < 2; ++c) {
            const bool can = changeable(sh, r);
            const bool on = sh.values[r] == c, hot = sh.hoverRow == r && sh.hoverChoice == c && can;
            panel::roundRect(out, L.choices[r][c], 8 * s, on ? kGold : kWhite,
                             on ? (can ? 0.26f : 0.14f) : hot ? 0.10f : 0.04f, on ? kGold : kWhite,
                             on ? (can ? 0.85f : 0.45f) : 0.14f, 1.0f * s);
        }
    }
    panel::roundRect(out, L.ok, 8 * s, kGold, sh.hoverPart == int(Part::Ok) ? 0.34f : 0.22f, kGold, 0.8f, 1.0f * s);
    panel::roundRect(out, L.cancel, 8 * s, kWhite, sh.hoverPart == int(Part::Cancel) ? 0.12f : 0.06f, kWhite, 0.16f,
                     1.0f * s);
}

bool changeable(const Show& sh, int row) { return sh.canChange && (row == 0 || !sh.thingsFixed); }

std::vector<Words> words(const Layout& L, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 15 * s, true, Align::Left});
    for (int r = 0; r < 3; ++r) {
        if (!sh.present[r]) continue;
        out.push_back(Words{L.labels[r], t.rows[r], kMuted, 12 * s, false, Align::Left});
        for (int c = 0; c < 2; ++c)
            out.push_back(Words{L.choices[r][c], t.choices[r][c], changeable(sh, r) ? kText : kMuted, 13 * s,
                                sh.values[r] == c, Align::Center});
    }
    if (!sh.canChange)
        out.push_back(Words{L.note, t.leaderOnly, kMuted, 11.5f * s, false, Align::Center});
    else if (sh.thingsFixed && !t.thingsFixed.empty())  // 3.0.11-beta.200
        out.push_back(Words{L.note, t.thingsFixed, kMuted, 11.5f * s, false, Align::Center});
    out.push_back(Words{L.ok, t.ok, kText, 13.5f * s, true, Align::Center});
    out.push_back(Words{L.cancel, t.cancel, kText, 13.5f * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::partysetup
