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

// 3.0.11-beta.109 (question 4 «ок»): where a window opening at `wanted` goes - there, inside the screen, when no open
// window shares more than 15 % of it; else beside the one sharing most: right of it, else left (`gap` apart, tops in
// line), while the screen holds it and no other open one is in the way; else `step` down and right of it
struct Box {
    int x = 0, y = 0, w = 0, h = 0;
};
Box place(Box wanted, const std::vector<Box>& open, int screenW, int screenH, int gap, int step);

// 3.0.11-beta.111 (question 1: «два окна вплотную друг к другу как одно окно»): «Спорядження» and «Речі» open together
// stand side by side, tops in line, and move as one - the gear left of the bag where the screen has room, else right
enum class Side : uint8_t { Left, Right };
Side dockSide(Box bag, Box gear, int screenW);
Side opposite(Side s);
Box docked(Box leader, Box follower, Side side);  // the follower `side` of the leader, tops in line

}  // namespace uagfx::winstack
