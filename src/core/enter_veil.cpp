#include "core/enter_veil.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::enterveil {
namespace {

constexpr float kW = 380, kH = 132, kPad = 12;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xE6CB8A, kMuted = 0x9AA3B5;

}  // namespace

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.w = int(std::ceil((kW + 2 * kPad) * s));
    L.h = int(std::ceil((kH + 2 * kPad) * s));
    L.x = (targetW - L.w) / 2;
    L.y = (targetH - L.h) / 2;
    const float p = kPad * s;
    L.card = panel::Box{p, p, kW * s, kH * s};
    L.title = panel::Box{p, p + 22 * s, kW * s, 26 * s};
    L.name = panel::Box{p, p + 54 * s, kW * s, 22 * s};
    L.line = panel::Box{p, p + 76 * s, kW * s, 18 * s};
    L.dots = panel::Box{p + (kW / 2 - 22) * s, p + 104 * s, 44 * s, 10 * s};
    return L;
}

std::vector<Words> words(const Layout& L, const Texts& t) {
    const float s = L.s;
    std::vector<Words> out{Words{L.title, t.title, kGold, 18 * s, true, Align::Center}};
    if (!t.name.empty()) out.push_back(Words{L.name, t.name, 0xEEF2F8, 15 * s, true, Align::Center});
    if (!t.line.empty()) out.push_back(Words{L.line, t.line, kMuted, 12.5f * s, false, Align::Center});
    return out;
}

End ending(bool mapStarted, bool boxAsks, bool loginBack, uint64_t since, uint64_t now) {
    if (mapStarted) return End::World;
    if (boxAsks) return End::Box;
    if (loginBack) return End::Login;
    if (now - since > kMostMs) return End::Late;
    return End::No;
}

}  // namespace uagfx::enterveil
