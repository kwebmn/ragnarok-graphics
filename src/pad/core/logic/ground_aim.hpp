#pragma once
#include <cstdint>
#include <vector>

#include "pad/core/bytes.hpp"

namespace uapad {

// 3.0.11-beta.145 (the owner's concept «маг, прист и хант на геймпаде», answers 1 and 2 - «кнопки на правой руке, не
// очень удобно правым стиком выбирать»): a ground skill's button held - the aim, the character standing. Walls, traps
// and Fire Pillar by the side: the stick that leans the most (either - the thumb that is free) chooses one of the
// server's 8 sides from the character and how far (1 to 4 cells by the lean, a trap's up to 3); the areas and zones as
// a cursor (up to 12 cells a second at a full lean) that sticks to a monster or a friend within a cell as the stick
// lets go; the D-pad moves it a cell; never past the skill's reach
class GroundAim {
public:
    enum class Style : uint8_t { Side, Cursor };
    static Style styleOf(uint16_t skill);

    // From `from` (the press's own cell) for `skill` at `lv` on the hotkey `slot`, held on `button`
    void start(uint16_t skill, uint16_t lv, int slot, uint16_t button, Cell me, Cell from, int range);
    // One tick: the stick's lean on the map (east, north - its length the lean, 0 to 1), the D-pad's step on the map
    // pressed now, the cells it sticks to, the time since the last tick
    void step(double east, double north, Cell nudge, Cell me, const std::vector<Cell>& snaps, uint32_t dtMs);
    void stop() { active_ = false; }

    bool active() const { return active_; }
    Cell cell() const { return at_; }
    uint16_t skill() const { return skill_; }
    uint16_t level() const { return lv_; }
    int slot() const { return slot_; }
    uint16_t button() const { return button_; }
    bool inReach(Cell me) const { return cellDistance(me, at_) <= range_; }

private:
    void keepInReach(Cell me);

    bool active_ = false, moved_ = false;
    uint16_t skill_ = 0, lv_ = 0, button_ = 0;
    int slot_ = -1, range_ = 9, most_ = 4;
    Style style_ = Style::Side;
    Cell at_;
    double fx_ = 0, fy_ = 0;  // the cursor's place between cells
};

}  // namespace uapad
