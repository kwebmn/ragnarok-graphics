#include "core/login_chrome.hpp"

#include <cmath>

namespace uagfx::loginchrome {
namespace {

// At scale 1: the strip 80 high; the pill 32 high, 20 over the edge, 16 in, its dot 9 and 10 to the words
constexpr float kStripH = 80, kPillH = 32, kEdge = 20, kPad = 16, kDot = 9, kDotGap = 10, kPx = 14;
constexpr uint32_t kPill = 0x0A0C12, kWhite = 0xFFFFFF, kWords = 0xC9CFDB;

}  // namespace

Layout layout(int targetW, int targetH, float scale, const std::string& statusText) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.w = targetW;
    L.h = int(std::ceil(kStripH * s));
    L.x = 0;
    L.y = targetH - L.h;
    if (!statusText.empty()) {
        const float text = panel::wordsWidth(statusText, kPx * s, false);
        const float w = kPad * s + kDot * s + kDotGap * s + text + kPad * s;
        const float top = float(L.h) - kEdge * s - kPillH * s;
        L.status = panel::Box{std::round((float(targetW) - w) / 2), top, std::ceil(w), kPillH * s};
        L.statusDot = panel::Box{L.status.x + kPad * s, top + (kPillH - kDot) * s / 2, kDot * s, kDot * s};
        L.statusText = panel::Box{L.statusDot.x + (kDot + kDotGap) * s, top, text + 2 * s, kPillH * s};
    }
    return L;
}

void paint(panel::Picture& out, const Layout& L, const Show& sh) {
    const float s = L.s;
    if (L.status.w <= 0) return;
    panel::roundRect(out, L.status, kPillH * s / 2, kPill, 0.72f, kWhite, 0.10f, 1.0f * s);
    panel::roundRect(out, L.statusDot, kDot * s / 2, sh.statusRgb, 1.0f, sh.statusRgb, 1.0f, 0.0f);
}

std::vector<Words> words(const Layout& L, const Show& sh) {
    std::vector<Words> out;
    if (L.status.w > 0 && !sh.status.empty())
        out.push_back(Words{L.statusText, sh.status, kWords, kPx * L.s, false, Align::Left});
    return out;
}

}  // namespace uagfx::loginchrome
