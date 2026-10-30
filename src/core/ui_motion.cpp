#include "core/ui_motion.hpp"

#include <algorithm>
#include <atomic>

namespace uagfx::motion {
namespace {

std::atomic<bool> g_on{true};

float part(uint64_t now, uint64_t since, uint64_t ms) {
    if (!ms || now <= since) return 0;
    return std::min(1.0f, float(now - since) / float(ms));
}

}  // namespace

void setOn(bool on) { g_on = on; }
bool on() { return g_on; }

float easeOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1 - t;
    return 1 - u * u * u;
}

float fadeIn(uint64_t now, uint64_t since, uint64_t ms) {
    if (!g_on || !since) return 1;
    return easeOut(part(now, since, ms));
}

float fadeOut(uint64_t now, uint64_t since, uint64_t ms) {
    if (!g_on || !since) return 0;
    return 1 - easeOut(part(now, since, ms));
}

float flare(uint64_t now, uint64_t since, uint64_t ms) {
    if (!g_on || !since || now < since || now - since >= ms) return 0;
    const float t = part(now, since, ms);
    return t < 0.2f ? t / 0.2f : 1 - easeOut((t - 0.2f) / 0.8f);
}

}  // namespace uagfx::motion
