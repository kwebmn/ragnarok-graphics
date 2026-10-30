#include "core/hud_layout_panel.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::hudlayout {
namespace {

constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kAccent = 0xC9A55B, kAccent2 = 0xE6CB8A;
constexpr uint32_t kText = 0xECEFF5, kMuted = 0x9AA3B5, kOnGold = 0x1A1408;
constexpr float kBarW = 720, kBarH = 52, kButtonW = 128, kButtonH = 32, kPad = 10;

float px(float v, float s) { return std::round(v * s); }

}  // namespace

Bar barLayout(int targetW, float scale) {
    Bar b;
    b.s = std::max(0.5f, scale);
    b.w = int(px(kBarW, b.s));
    b.h = int(px(kBarH, b.s));
    b.x = (targetW - b.w) / 2;
    b.y = int(px(12, b.s));
    const float bw = px(kButtonW, b.s), bh = px(kButtonH, b.s), top = (float(b.h) - bh) / 2;
    b.done = panel::Box{float(b.w) - px(kPad, b.s) - bw, top, bw, bh};
    b.reset = panel::Box{b.done.x - px(8, b.s) - bw, top, bw, bh};
    return b;
}

BarPart barHit(const Bar& b, float x, float y) {
    if (x < 0 || y < 0 || x >= float(b.w) || y >= float(b.h)) return BarPart::None;
    if (b.done.contains(x, y)) return BarPart::Done;
    if (b.reset.contains(x, y)) return BarPart::Reset;
    return BarPart::Body;
}

void paintBar(panel::Picture& out, const Bar& b, BarPart hover) {
    out.width = b.w;
    out.height = b.h;
    out.px.assign(size_t(b.w) * size_t(b.h), 0u);
    const float s = b.s, edge = std::max(1.0f, s);
    panel::roundRect(out, panel::Box{0, 0, float(b.w), float(b.h)}, 12 * s, kCard, 0.94f, kAccent, 0.55f, edge);
    panel::roundRect(out, b.reset, 8 * s, kWhite, hover == BarPart::Reset ? 0.16f : 0.08f, kWhite, 0.22f, edge);
    panel::roundRect(out, b.done, 8 * s, hover == BarPart::Done ? kAccent2 : kAccent, 1.0f);
}

std::vector<Words> barWords(const Bar& b, const BarTexts& t) {
    const float s = b.s, left = px(16, s), room = b.reset.x - left - px(10, s);
    return {Words{panel::Box{left, px(7, s), room, px(20, s)}, t.title, kAccent2, 14 * s, true, Align::Left},
            Words{panel::Box{left, px(27, s), room, px(18, s)}, t.hint, kMuted, 11.5f * s, false, Align::Left},
            Words{b.reset, t.reset, kText, 12.5f * s, false, Align::Center},
            Words{b.done, t.done, kOnGold, 12.5f * s, true, Align::Center}};
}

void paintFrame(panel::Picture& out, int w, int h, float scale, Look look, float tagWidth) {
    out.width = std::max(0, w);
    out.height = std::max(0, h);
    out.px.assign(size_t(out.width) * size_t(out.height), 0u);
    if (w <= 0 || h <= 0) return;
    const float s = std::max(0.5f, scale), edge = std::max(2.0f, 2 * s);
    const float fill = look == Look::Dragged ? 0.22f : look == Look::Hover ? 0.16f : 0.08f;
    const bool idle = look == Look::Idle;
    panel::roundRect(out, panel::Box{0, 0, float(w), float(h)}, 8 * s, kAccent, fill, idle ? kAccent : kAccent2,
                     idle ? 0.75f : 1.0f, edge);
    if (tagWidth > 0)
        panel::roundRect(out, panel::Box{px(4, s), px(3, s), std::min(float(w) - px(8, s), tagWidth + px(10, s)),
                                         std::min(px(22, s), float(h) - px(6, s))},
                         6 * s, kCard, 0.92f);
}

void paintGrip(panel::Picture& out, int side, float scale, bool hover) {
    out.width = out.height = std::max(0, side);
    out.px.assign(size_t(out.width) * size_t(out.height), 0u);
    if (side <= 0) return;
    const float s = std::max(0.5f, scale), f = float(side);
    panel::roundRect(out, panel::Box{0, 0, f, f}, f / 4, kCard, hover ? 0.95f : 0.8f, kAccent2, hover ? 1.0f : 0.7f,
                     std::max(1.0f, s));
    const float r = std::max(0.8f, 1.1f * s);
    for (int row = 0; row < 3; ++row)  // ⋰ - the three corners' dots of a resize handle
        for (int col = 2 - row; col < 3; ++col)
            panel::circle(out, f * (0.3f + 0.2f * float(col)), f * (0.3f + 0.2f * float(row)), r, kAccent2,
                          hover ? 1.0f : 0.8f);
}

std::vector<Words> frameWords(int w, int h, float scale, const std::string& name, int percent) {
    const float s = std::max(0.5f, scale);
    std::string text = name;
    if (percent != 100) text += " \xC2\xB7 " + std::to_string(percent) + " %";
    return {Words{panel::Box{px(8, s), px(4, s), std::max(0.0f, float(w) - px(16, s)), std::min(px(20, s), float(h))},
                  text, kAccent2, 12.5f * s, true, Align::Left}};
}

}  // namespace uagfx::hudlayout
