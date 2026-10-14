#include "core/map_glows.hpp"

#include <cmath>

namespace uagfx::map_glows {
namespace {

Glow make(Shape shape, float r, float g, float b, float size, float light, float reach) {
    Glow out;
    out.shape = shape;
    out.colour[0] = r;
    out.colour[1] = g;
    out.colour[2] = b;
    out.size = size;
    out.light = light;
    out.reach = reach;
    return out;
}

}  // namespace

bool glowOf(int id, Glow& glow) {
    switch (id) {
    case 693: glow = make(Shape::Orb, 1.0f, 0.82f, 0.5f, 0.6f, 0.22f, 22.0f); return true;   // glow1: warm gold
    case 694: glow = make(Shape::Orb, 0.62f, 0.8f, 1.0f, 0.7f, 0.22f, 24.0f); return true;   // glow2: pale blue
    case 695: glow = make(Shape::Orb, 1.0f, 0.94f, 0.86f, 0.8f, 0.25f, 26.0f); return true;  // glow4: white
    case 701: glow = make(Shape::Orb, 1.0f, 0.58f, 0.3f, 0.5f, 0.2f, 20.0f); return true;    // glow11: orange
    case 702: glow = make(Shape::Orb, 0.5f, 1.0f, 0.86f, 0.7f, 0.22f, 24.0f); return true;   // glow12: teal
    case 703: glow = make(Shape::Pool, 0.78f, 1.0f, 0.82f, 1.2f, 0.35f, 26.0f); return true;  // circlelight
    case 710: glow = make(Shape::Orb, 0.45f, 1.0f, 0.5f, 0.7f, 0.25f, 24.0f); return true;   // map_greenlight
    case 712: glow = make(Shape::Orb, 0.55f, 1.0f, 0.62f, 0.8f, 0.25f, 26.0f); return true;  // map_greenlight2
    case 689: glow = make(Shape::Dust, 1.0f, 0.93f, 0.8f, 20.0f, 0.0f, 0.0f); return true;   // dust
    default: return false;
    }
}

float pulse(double time, float seed) {
    const double a = time * (0.45 + 0.3 * double(seed)) + double(seed) * 11.0;
    return float(0.8 + 0.2 * (0.7 * std::sin(a) + 0.3 * std::sin(a * 2.17 + 1.3)));
}

float bob(double time, float seed) {
    const double a = time * (0.3 + 0.2 * double(seed)) + double(seed) * 5.0;
    return float(0.8 * std::sin(a) + 0.3 * std::sin(a * 1.9 + 0.7));
}

}  // namespace uagfx::map_glows
