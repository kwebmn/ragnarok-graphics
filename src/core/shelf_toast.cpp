#include "core/shelf_toast.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::shelftoast {
namespace {

// At scale 1: 36 high, 24 under the screen's top, 18 in; « · » between the words and the link
constexpr float kH = 36, kTop = 24, kPad = 18, kPx = 13.5f, kDotGap = 10;
constexpr uint32_t kGround = 0x10141C, kGold = 0xC9A55B, kWords = 0xEEF2F8, kLink = 0x8CB8FF, kLinkHot = 0xB5D0FF;
constexpr uint32_t kMuted = 0x9AA3B5;

}  // namespace

Layout layout(int targetW, int targetH, float scale, const std::string& text, const std::string& undo) {
    (void)targetH;
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float t = panel::wordsWidth(text, kPx * s, false), u = panel::wordsWidth(undo, kPx * s, true);
    const float dot = panel::wordsWidth("·", kPx * s, false);
    const float w = kPad * s + t + 2 * kDotGap * s + dot + u + kPad * s;
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(kH * s));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = int(std::round(kTop * s));
    L.pill = panel::Box{0, 0, float(L.w), float(L.h)};
    L.text = panel::Box{kPad * s, 0, t + 1, float(L.h)};
    L.undo = panel::Box{kPad * s + t + 2 * kDotGap * s + dot, 0, u + 2, float(L.h)};
    return L;
}

bool overUndo(const Layout& L, float x, float y) {
    const panel::Box b{L.undo.x - 6 * L.s, L.undo.y, L.undo.w + 12 * L.s, L.undo.h};  // a little wider to hit
    return b.contains(x, y);
}

void paint(panel::Picture& out, const Layout& L, bool hot) {
    panel::roundRect(out, L.pill, L.pill.h / 2, kGround, 0.94f, kGold, hot ? 0.75f : 0.55f, 1.0f * L.s);
}

std::vector<Words> words(const Layout& L, const std::string& text, const std::string& undo, bool hot) {
    const float px = kPx * L.s;
    const panel::Box dot{L.text.x + L.text.w, 0, L.undo.x - (L.text.x + L.text.w), float(L.h)};
    return {Words{L.text, text, kWords, px, false}, Words{dot, "·", kMuted, px, false},
            Words{L.undo, undo, hot ? kLinkHot : kLink, px, true}};
}

}  // namespace uagfx::shelftoast
