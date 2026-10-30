#include "core/ui_style.hpp"

#include <algorithm>
#include <atomic>

namespace uagfx::uistyle {
namespace {

std::atomic<int> g_panel{kPanelDefault}, g_window{kWindowDefault}, g_generation{0};

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

}  // namespace uagfx::uistyle
