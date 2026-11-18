#include "core/item_pick.hpp"

#include <algorithm>

namespace uagfx::itempick {

bool Picked::has(int index) const { return std::find(picked_.begin(), picked_.end(), index) != picked_.end(); }

void Picked::toggle(int index) {
    const auto it = std::find(picked_.begin(), picked_.end(), index);
    if (it != picked_.end()) picked_.erase(it);
    else picked_.push_back(index);
    last_ = index;
}

void Picked::range(const std::vector<int>& shown, int index) {
    const auto a = std::find(shown.begin(), shown.end(), last_), b = std::find(shown.begin(), shown.end(), index);
    if (b == shown.end()) return;
    if (a == shown.end()) {
        if (!has(index)) picked_.push_back(index);
        last_ = index;
        return;
    }
    const auto from = std::min(a, b), to = std::max(a, b);
    for (auto it = from; it <= to; ++it)
        if (!has(*it)) picked_.push_back(*it);
    last_ = index;
}

void Picked::all(const std::vector<int>& shown) {
    for (const int i : shown)
        if (!has(i)) picked_.push_back(i);
}

void Picked::keepOnly(const std::vector<int>& there) {
    picked_.erase(std::remove_if(picked_.begin(), picked_.end(),
                                 [&](int i) { return std::find(there.begin(), there.end(), i) == there.end(); }),
                  picked_.end());
}

void Picked::clear() {
    picked_.clear();
    last_ = -1;
}

bool step(Pace& p, uint64_t now, size_t left, const std::function<bool()>& answer, const std::function<void()>& move) {
    if (left == 0 && now >= p.lastUntil) {  // nothing queued, the last one's «how many» not awaited
        p.lastUntil = 0;
        return false;
    }
    if (answer()) {  // a «how many» up: all of it - the next move a step later
        p.next = now + kStepMs;
        if (left > 0) return true;
        p.lastUntil = 0;  // the last one's: the batch is over
        return false;
    }
    if (left == 0 || now < p.next) return true;
    move();
    p.next = now + kStepMs;
    if (left == 1) p.lastUntil = now + kLastWaitMs;  // that was the last one
    return true;
}

}  // namespace uagfx::itempick
