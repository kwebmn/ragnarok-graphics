#pragma once
#include <cstdint>
#include <optional>
#include <vector>

namespace uagfx::winstack {

// 3.0.11-beta.108 (the concept «Концепция окон и инвентаря», stage 1; the owner 31.10: «окна одно поверх другого не
// появляются», answers 4-8 «ок»): the driver's windows in one stack - drawn bottom up, the mouse, the keys and the
// controller the top's first (they were each in an order of its own, the drawing's and the clicks' apart); a window
// opened or clicked in comes on top. No Windows here (win/window_stack).
enum class Win : uint8_t { Items, Stats, Skills, Equip, Shop, Npc, Reference };
constexpr int kWins = 7;

class Stack {
public:
    // Each frame, each window: shown now - opened, it comes on top; closed, out
    void see(Win w, bool shown);
    // Clicked in or dragged from: on top, when shown
    void raise(Win w);
    std::optional<Win> top() const;
    const std::vector<Win>& bottomUp() const { return order_; }
    std::vector<Win> topDown() const;
    // The gold edge (question 6): the top one of two or more shown
    bool marked(Win w) const;

private:
    std::vector<Win> order_;  // the shown ones, the bottom first
};

}  // namespace uagfx::winstack
