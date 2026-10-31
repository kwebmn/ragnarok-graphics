#include "core/window_stack.hpp"

#include <algorithm>
#include <cstdint>

namespace uagfx::winstack {

void Stack::see(Win w, bool shown) {
    const auto it = std::find(order_.begin(), order_.end(), w);
    if (shown && it == order_.end())
        order_.push_back(w);
    else if (!shown && it != order_.end())
        order_.erase(it);
}

void Stack::raise(Win w) {
    const auto it = std::find(order_.begin(), order_.end(), w);
    if (it == order_.end()) return;
    order_.erase(it);
    order_.push_back(w);
}

std::optional<Win> Stack::top() const {
    if (order_.empty()) return std::nullopt;
    return order_.back();
}

std::vector<Win> Stack::topDown() const { return std::vector<Win>(order_.rbegin(), order_.rend()); }

bool Stack::marked(Win w) const { return order_.size() >= 2 && order_.back() == w; }

namespace {

int64_t shared(const Box& a, const Box& b) {
    const int64_t w = std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x);
    const int64_t h = std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y);
    return w > 0 && h > 0 ? w * h : 0;
}

bool crowded(const Box& b, const std::vector<Box>& open) {
    for (const Box& o : open)
        if (shared(b, o) * 100 > int64_t(b.w) * int64_t(b.h) * 15) return true;
    return false;
}

Box inside(Box b, int screenW, int screenH) {
    b.x = std::clamp(b.x, 0, std::max(0, screenW - b.w));
    b.y = std::clamp(b.y, 0, std::max(0, screenH - b.h));
    return b;
}

}  // namespace

Box place(Box wanted, const std::vector<Box>& open, int screenW, int screenH, int gap, int step) {
    wanted = inside(wanted, screenW, screenH);
    if (!crowded(wanted, open)) return wanted;
    const Box* most = nullptr;
    for (const Box& o : open)
        if (!most || shared(wanted, o) > shared(wanted, *most)) most = &o;
    const Box o = *most;
    const int y = std::clamp(o.y, 0, std::max(0, screenH - wanted.h));
    const Box right{o.x + o.w + gap, y, wanted.w, wanted.h};
    if (right.x + right.w <= screenW && !crowded(right, open)) return right;
    const Box left{o.x - gap - wanted.w, y, wanted.w, wanted.h};
    if (left.x >= 0 && !crowded(left, open)) return left;
    return inside(Box{o.x + step, o.y + step, wanted.w, wanted.h}, screenW, screenH);
}

Side dockSide(Box bag, Box gear, int screenW) {
    if (bag.x - gear.w >= 0 || bag.x + bag.w + gear.w > screenW) return Side::Left;
    return Side::Right;
}

Side opposite(Side s) { return s == Side::Left ? Side::Right : Side::Left; }

Box docked(Box leader, Box follower, Side side) {
    follower.x = side == Side::Left ? leader.x - follower.w : leader.x + leader.w;
    follower.y = leader.y;
    return follower;
}

}  // namespace uagfx::winstack
