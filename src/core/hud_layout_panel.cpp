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

std::vector<Words> barWords(const Bar& b, const BarTexts& t) {
    const float s = b.s, left = px(16, s), room = b.reset.x - left - px(10, s);
    return {Words{panel::Box{left, px(7, s), room, px(20, s)}, t.title, kAccent2, 14 * s, true, Align::Left},
            Words{panel::Box{left, px(27, s), room, px(18, s)}, t.hint, kMuted, 11.5f * s, false, Align::Left},
            Words{b.reset, t.reset, kText, 12.5f * s, false, Align::Center},
            Words{b.done, t.done, kOnGold, 12.5f * s, true, Align::Center}};
}

std::vector<Words> frameWords(int w, int h, float scale, const std::string& name, int percent) {
    const float s = std::max(0.5f, scale);
    std::string text = name;
    if (percent != 100) text += " \xC2\xB7 " + std::to_string(percent) + " %";
    return {Words{panel::Box{px(8, s), px(4, s), std::max(0.0f, float(w) - px(16, s)), std::min(px(20, s), float(h))},
                  text, kAccent2, 12.5f * s, true, Align::Left}};
}

}  // namespace uagfx::hudlayout
