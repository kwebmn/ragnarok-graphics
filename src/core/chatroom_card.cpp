#include "core/chatroom_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::chatroomcard {
namespace {

// At scale 1: the card 380 wide, 18 in; its heading 30; a label 18 over its field 34; chips 30 high, 6 apart;
// buttons 38
constexpr float kW = 380, kPad = 18, kHeadH = 30, kLabelH = 18, kFieldH = 34, kChipH = 30, kChipGap = 6;
constexpr float kButtonH = 38, kGap = 10;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000;

}  // namespace

std::string masked(size_t n) {
    std::string out;
    for (size_t i = 0; i < n; ++i) out += "\xE2\x80\xA2";  // •
    return out;
}

Layout layout(size_t limits, int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s, inner = w - 2 * pad;
    L.heading = panel::Box{pad, pad, inner - 30 * s, kHeadH * s};
    L.close = panel::Box{w - pad - 26 * s, pad + 2 * s, 26 * s, 26 * s};
    float y = pad + kHeadH * s + 4 * s;
    L.titleLabel = panel::Box{pad, y, inner, kLabelH * s};
    y += kLabelH * s;
    L.title = panel::Box{pad, y, inner, kFieldH * s};
    y += kFieldH * s + kGap * s;
    L.limitLabel = panel::Box{pad, y, inner, kLabelH * s};
    y += kLabelH * s;
    const size_t n = std::max<size_t>(limits, 1);
    const float cw = (inner - float(n - 1) * kChipGap * s) / float(n);
    for (size_t i = 0; i < limits; ++i)
        L.limits.push_back(panel::Box{pad + float(i) * (cw + kChipGap * s), y, cw, kChipH * s});
    y += kChipH * s + kGap * s;
    const float half = (inner - kChipGap * s) / 2;
    L.open = panel::Box{pad, y, half, kChipH * s};
    L.closed = panel::Box{pad + half + kChipGap * s, y, half, kChipH * s};
    y += kChipH * s + kGap * s;
    L.passwordLabel = panel::Box{pad, y, inner, kLabelH * s};
    y += kLabelH * s;
    L.password = panel::Box{pad, y, inner, kFieldH * s};
    y += kFieldH * s + 14 * s;
    const float bw = (inner - kGap * s) / 2;
    L.create = panel::Box{pad, y, bw, kButtonH * s};
    L.cancel = panel::Box{pad + bw + kGap * s, y, bw, kButtonH * s};
    y += kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::clamp(int(float(targetH) * 0.30f) - L.h / 2, 0, std::max(0, targetH - L.h));
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    if (L.title.contains(x, y)) return Hit{Part::Title, -1};
    if (L.password.contains(x, y)) return Hit{Part::Password, -1};
    for (size_t i = 0; i < L.limits.size(); ++i)
        if (L.limits[i].contains(x, y)) return Hit{Part::Limit, int(i)};
    if (L.open.contains(x, y)) return Hit{Part::Open, -1};
    if (L.closed.contains(x, y)) return Hit{Part::Closed, -1};
    if (L.create.contains(x, y)) return Hit{Part::Create, -1};
    if (L.cancel.contains(x, y)) return Hit{Part::Cancel, -1};
    return Hit{};
}

std::vector<Words> words(const Layout& L, const Card& c, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.heading, t.heading, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xE2\x9C\x95", sh.hover.part == Part::Close ? kText : kMuted, 13 * s, false,
                        Align::Center});  // ✕
    out.push_back(Words{L.titleLabel, t.titleLabel, kMuted, 11.5f * s, false, Align::Left});
    const std::string caret = c.caret ? "|" : "";
    out.push_back(Words{panel::Box{L.title.x + 10 * s, L.title.y, L.title.w - 20 * s, L.title.h},
                        c.title + (c.focus == Field::Title ? caret : ""), kText, 13.5f * s, false, Align::Left});
    out.push_back(Words{L.limitLabel, t.limitLabel, kMuted, 11.5f * s, false, Align::Left});
    for (size_t i = 0; i < L.limits.size() && i < c.limits.size(); ++i) {
        const bool on = c.limits[i] == c.limit;
        out.push_back(Words{L.limits[i], std::to_string(c.limits[i]), on ? kGold2 : kText, 12.5f * s, on,
                            Align::Center});
    }
    out.push_back(Words{L.open, t.open, !c.closed ? kGold2 : kText, 12.5f * s, !c.closed, Align::Center});
    out.push_back(Words{L.closed, t.closed, c.closed ? kGold2 : kText, 12.5f * s, c.closed, Align::Center});
    out.push_back(Words{L.passwordLabel, t.passwordLabel, c.closed ? kMuted : 0x5E6575, 11.5f * s, false,
                        Align::Left});
    out.push_back(Words{panel::Box{L.password.x + 10 * s, L.password.y, L.password.w - 20 * s, L.password.h},
                        masked(c.passwordLength) + (c.focus == Field::Password ? caret : ""), kText, 13.5f * s,
                        false, Align::Left});
    out.push_back(Words{L.create, t.create, kText, 13 * s, true, Align::Center});
    out.push_back(Words{L.cancel, t.cancel, kText, 13 * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::chatroomcard
