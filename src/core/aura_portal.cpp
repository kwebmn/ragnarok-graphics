#include "core/aura_portal.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::auraportal {

namespace {
bool boxOf(const uint8_t* v, uint32_t count, uint32_t stride, Box& out) {
    if (!v || count == 0 || stride < 8) return false;
    out = Box{1e9f, 1e9f, -1e9f, -1e9f};
    for (uint32_t i = 0; i < count; ++i) {
        float xy[2];
        std::memcpy(xy, v + size_t(i) * stride, sizeof xy);
        out.x0 = std::min(out.x0, xy[0]);
        out.y0 = std::min(out.y0, xy[1]);
        out.x1 = std::max(out.x1, xy[0]);
        out.y1 = std::max(out.y1, xy[1]);
    }
    return out.x1 > out.x0 && out.y1 > out.y0;
}
}  // namespace

void Portals::frame() {
    last_.swap(now_);
    now_.clear();
}

void Portals::portal(const uint8_t* vertices, uint32_t count, uint32_t stride) {
    Box b{};
    if (!boxOf(vertices, count, stride, b)) return;
    for (Box& o : now_)  // one portal's many draws: one box
        if (b.x0 <= o.x1 && o.x0 <= b.x1 && b.y0 <= o.y1 && o.y0 <= b.y1) {
            o = Box{std::min(o.x0, b.x0), std::min(o.y0, b.y0), std::max(o.x1, b.x1), std::max(o.y1, b.y1)};
            return;
        }
    if (now_.size() < 32) now_.push_back(b);
}

bool Portals::covers(const uint8_t* vertices, uint32_t count, uint32_t stride) const {
    Box b{};
    if (last_.empty() || !boxOf(vertices, count, stride, b)) return false;
    const float mx = (b.x0 + b.x1) / 2, my = (b.y0 + b.y1) / 2;
    for (const Box& o : last_)
        if (mx >= o.x0 && mx <= o.x1 && my >= o.y0 && my <= o.y1) return true;
    return false;
}

}  // namespace uagfx::auraportal
