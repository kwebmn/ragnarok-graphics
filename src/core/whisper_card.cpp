#include "core/whisper_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::whispercard {
namespace {

// At scale 1: the card 420 wide, 14 in; its title 28; the lines 17 a row (8 rows); the input row 32; cascaded 28 apart
constexpr float kW = 420, kPad = 14, kTitleH = 28, kRowH = 17, kInputH = 32, kGap = 8, kStep = 28;
constexpr int kRows = 8;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlack = 0x000000, kWhisper = 0xF3D27A;

std::string withName(const std::string& form, const std::string& name) {
    std::string out = form;
    const size_t at = out.find("{1}");
    if (at != std::string::npos) out.replace(at, 3, name);
    return out;
}

}  // namespace

Layout layout(int n, int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, pad = kPad * s;
    L.title = panel::Box{pad, pad, w - 2 * pad - 30 * s, kTitleH * s};
    L.close = panel::Box{w - pad - 26 * s, pad + 1 * s, 26 * s, 26 * s};
    const float top = pad + kTitleH * s + 4 * s;
    L.lines = panel::Box{pad, top, w - 2 * pad, kRowH * kRows * s + 8 * s};
    L.lineRows = kRows;
    const float iy = L.lines.y + L.lines.h + kGap * s;
    L.send = panel::Box{w - pad - 104 * s, iy, 104 * s, kInputH * s};
    L.input = panel::Box{pad, iy, L.send.x - kGap * s - pad, kInputH * s};
    const float h = iy + kInputH * s + pad;
    L.glass = panel::Box{0, 0, w, h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(h));
    const int step = int(kStep * s) * std::max(0, n % 8);
    L.x = std::clamp(int(24 * s) + step, 0, std::max(0, targetW - L.w));
    L.y = std::clamp(int(float(targetH) * 0.38f) + step, 0, std::max(0, targetH - L.h));
    return L;
}

Part hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Part::Close;
    if (L.send.contains(x, y)) return Part::Send;
    if (L.input.contains(x, y)) return Part::Input;
    if (L.glass.contains(x, y)) return Part::Body;
    return Part::None;
}

void paint(panel::Picture& out, const Layout& L, const Card& c, Part hover) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.close, 6 * s, kWhite, hover == Part::Close ? 0.14f : 0.0f, kWhite,
                     hover == Part::Close ? 0.3f : 0.0f, 1.0f * s);
    panel::roundRect(out, L.lines, 8 * s, kBlack, 0.25f, kWhite, 0.06f, 1.0f * s);
    panel::roundRect(out, L.input, 8 * s, kBlack, 0.35f, c.focus ? kGold : kWhite, c.focus ? 0.8f : 0.18f, 1.0f * s);
    panel::roundRect(out, L.send, 8 * s, kGold, hover == Part::Send ? 0.34f : 0.22f, kGold, 0.8f, 1.0f * s);
}

std::vector<Words> words(const Layout& L, const Card& c, Part hover, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, withName(t.with, c.name), kWhisper, 14 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xE2\x9C\x95", hover == Part::Close ? kText : kMuted, 13 * s, false,
                        Align::Center});  // ✕
    const size_t from = c.lines.size() > size_t(L.lineRows) ? c.lines.size() - size_t(L.lineRows) : 0;
    float y = L.lines.y + L.lines.h - float(c.lines.size() - from) * kRowH * s - 4 * s;
    for (size_t i = from; i < c.lines.size(); ++i) {
        out.push_back(Words{panel::Box{L.lines.x + 8 * s, y, L.lines.w - 16 * s, kRowH * s}, c.lines[i].text,
                            c.lines[i].rgb ? c.lines[i].rgb : kText, 12 * s, false, Align::Left});
        y += kRowH * s;
    }
    out.push_back(Words{panel::Box{L.input.x + 10 * s, L.input.y, L.input.w - 20 * s, L.input.h},
                        c.input + (c.focus && c.caret ? "|" : ""), kText, 13 * s, false, Align::Left});
    out.push_back(Words{L.send, t.send, kText, 12.5f * s, true, Align::Center});
    return out;
}

}  // namespace uagfx::whispercard
