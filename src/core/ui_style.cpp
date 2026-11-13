#include "core/ui_style.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace uagfx::uistyle {
namespace {

std::atomic<int> g_panel{kPanelDefault}, g_window{kWindowDefault}, g_generation{0};
std::atomic<float> g_size{1.0f}, g_text{1.0f};  // 3.0.11-beta.67: the reading windows' text
std::atomic<int> g_pictures{-1};                 // 3.0.11-beta.74

}  // namespace

void set(int panelPercent, int windowPercent) {
    const int p = std::clamp(panelPercent, kPanelLeast, 100), w = std::clamp(windowPercent, kWindowLeast, 100);
    if (p == g_panel && w == g_window) return;
    g_panel = p;
    g_window = w;
    ++g_generation;
}

float panel() { return float(g_panel) / 100.0f; }
float window() { return float(g_window) / 100.0f; }
float chat() { return std::max(0.8f, panel()); }
int generation() { return g_generation; }

void setPictures(int source) {
    if (g_pictures.exchange(source) != source) ++g_generation;
}

void bump() { ++g_generation; }

float autoSize(int gameW, int gameH, double windowScale) {
    if (gameW <= 0 || gameH <= 0) return 1.0f;
    const float share = std::min(float(gameW) / float(kFullW), float(gameH) / float(kFullH));
    const float legible = windowScale > 0 ? float(11.0 / (14.0 * windowScale)) : 1.0f;  // 14 words at 11 or more
    return std::clamp(std::max(share, legible), 0.6f, 1.0f);
}

void setSize(float size) {
    size = std::clamp(size, 0.5f, 1.5f);
    if (std::fabs(size - g_size.load()) < 1e-4f) return;
    g_size = size;
    ++g_generation;
}

float size() { return g_size; }

float partScale(double k) { return std::clamp(float(k) * size(), 0.5f, 6.0f); }

void setText(float text) {
    text = std::clamp(text, 0.8f, 1.5f);
    if (std::fabs(text - g_text.load()) < 1e-4f) return;
    g_text = text;
    ++g_generation;
}

float text() { return g_text; }

float readingScale(double k) { return std::clamp(float(k) * size() * text(), 0.5f, 6.0f); }

namespace {
std::atomic<float> g_items{1.0f};  // 3.0.15-beta.9
}  // namespace

void setItems(float items) {
    items = std::clamp(items, 0.6f, 1.0f);
    if (std::fabs(items - g_items.load()) < 1e-4f) return;
    g_items = items;
    ++g_generation;
}

float itemScale(double k) { return std::clamp(readingScale(k) * g_items.load(), 0.5f, 6.0f); }

panel::Picture shadow(int w, int h, float radius, float spread, float alpha, int down) {
    panel::Picture p;
    const int m = int(std::ceil(std::max(0.0f, spread))), dn = std::max(0, down);
    if (w <= 0 || h <= 0 || m <= 0) return p;
    p.width = w + 2 * m;
    p.height = h + 2 * m + dn;
    p.px.assign(size_t(p.width) * size_t(p.height), 0u);
    const float hx = float(w) / 2, hy = float(h) / 2, cx = float(m) + hx, cy = float(m) + hy;
    const float r = std::clamp(radius, 0.0f, std::min(hx, hy));
    const auto outOf = [&](float x, float y, float top) {  // the distance out of the rounded box (signed)
        const float qx = std::fabs(x - cx) - (hx - r), qy = std::fabs(y - (cy + top)) - (hy - r);
        return panel::length(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
    };
    // 3.0.11-beta.55 (R-9PP79T): the rows beside the window's straight sides have their shadow only left and right of
    // it (46 ms for a window of 1520 x 1200 pixel by pixel, the first time it opened)
    const int sideFrom = m + int(std::ceil(r)), sideTo = m + h - int(std::ceil(r));
    for (int y = 0; y < p.height; ++y)
        for (int x = 0; x < p.width; ++x) {
            if (y >= sideFrom && y < sideTo && x == m) x = m + w;  // the window itself: no shadow under it
            const float px = float(x) + 0.5f, py = float(y) + 0.5f;
            if (outOf(px, py, 0) <= 0) continue;
            const float out = outOf(px, py, float(dn));  // from the box dropped
            if (out >= spread) continue;
            const float t = out <= 0 ? 1.0f : 1 - out / spread;
            const uint32_t a = uint32_t(panel::roundOf(std::clamp(alpha * t * t, 0.0f, 1.0f) * 255));
            p.px[size_t(y) * size_t(p.width) + size_t(x)] = a << 24;  // black, premultiplied
        }
    return p;
}

bool scrollbarPart(const std::string& tag) {
    // RmlUi's own elements of a scrollbar (ElementScroll): the bars, their tracks, thumbs and arrows, the corner
    static const char* const kParts[] = {"scrollbarvertical", "scrollbarhorizontal", "scrollbarcorner", "slidertrack",
                                         "sliderbar", "sliderarrowdec", "sliderarrowinc"};
    for (const char* part : kParts)
        if (tag == part) return true;
    return false;
}

}  // namespace uagfx::uistyle
