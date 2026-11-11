#pragma once
#include <cstddef>
#include <vector>

namespace uagfx::itempick {

// 3.0.18-beta.155 (the owner: things picked with Ctrl and moved together, and so everywhere):
// the things picked in a window of things, by their places (the game's item index) - Ctrl+click toggles one,
// Shift+click all from the last clicked to this one in the order shown, Ctrl+A all shown
class Picked {
public:
    void toggle(int index);
    void range(const std::vector<int>& shown, int index);  // the last clicked .. index (both in `shown`)
    void all(const std::vector<int>& shown);
    void keepOnly(const std::vector<int>& there);  // those gone from the window let go
    void clear();
    bool has(int index) const;
    size_t count() const { return picked_.size(); }
    const std::vector<int>& list() const { return picked_; }

private:
    std::vector<int> picked_;
    int last_ = -1;
};

}  // namespace uagfx::itempick
