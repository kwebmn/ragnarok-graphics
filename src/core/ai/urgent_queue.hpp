#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>

namespace uagfx::ai {

// 3.0.2-beta.10: the pack's pictures' decoding pool's work - the most urgent first (the lowest rank: a sprite frame
// or an icon 0, a texture in view 1, one known ahead 2), the oldest of a rank first. One thread at a time.
template <class T>
class UrgentQueue {
public:
    void push(int rank, T item) { items_.push_back({rank, next_++, std::move(item)}); }
    bool pop(T& out) {
        if (items_.empty()) return false;
        size_t best = 0;  // a few dozen at most: a look at each
        for (size_t i = 1; i < items_.size(); ++i)
            if (items_[i].rank < items_[best].rank ||
                (items_[i].rank == items_[best].rank && items_[i].order < items_[best].order))
                best = i;
        out = std::move(items_[best].item);
        items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(best));
        return true;
    }
    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

private:
    struct Entry {
        int rank;
        uint64_t order;
        T item;
    };
    std::deque<Entry> items_;
    uint64_t next_ = 0;
};

}  // namespace uagfx::ai
