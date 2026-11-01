#include "pad/core/logic/ground_aim.hpp"

#include <algorithm>
#include <cmath>

#include "core/skill_area.hpp"

namespace uapad {
namespace {

namespace area = uagfx::skillarea;

constexpr double kDead = 0.35;           // the stick's lean that counts
constexpr double kCellsPerSecond = 12;   // the cursor at a full lean
constexpr double kPi = 3.14159265358979323846;

// The server's side (map_calc_dir's 0 north ... 6 east) nearest the map's (east, north)
int sideOf(double east, double north) {
    const int k = int(std::lround(std::atan2(north, east) / (kPi / 4)));
    return ((k % 8) + 8 + 6) % 8;
}

}  // namespace

GroundAim::Style GroundAim::styleOf(uint16_t skill) {
    switch (area::kindOf(skill)) {
        case area::Kind::Wall: case area::Kind::Trap: case area::Kind::Ahead: return Style::Side;
        default: return Style::Cursor;
    }
}

void GroundAim::start(uint16_t skill, uint16_t lv, int slot, uint16_t button, Cell me, Cell from, int range) {
    active_ = true;
    moved_ = false;
    skill_ = skill;
    lv_ = lv;
    slot_ = slot;
    button_ = button;
    range_ = std::max(1, range);
    style_ = styleOf(skill);
    most_ = std::min(range_, area::kindOf(skill) == area::Kind::Trap ? 3 : 4);
    at_ = from;
    fx_ = from.x;
    fy_ = from.y;
    keepInReach(me);
}

void GroundAim::step(double east, double north, Cell nudge, Cell me, const std::vector<Cell>& snaps, uint32_t dtMs) {
    if (!active_) return;
    const double lean = std::min(1.0, std::hypot(east, north));
    if (style_ == Style::Side) {
        if (lean > kDead) {
            const int far = 1 + int(std::lround((lean - kDead) / (1 - kDead) * double(most_ - 1)));
            const area::Cell one = area::step(sideOf(east, north));
            at_ = Cell{me.x + one.x * std::clamp(far, 1, most_), me.y + one.y * std::clamp(far, 1, most_)};
        }
    } else if (lean > kDead) {
        const double speed = kCellsPerSecond * lean * lean * double(dtMs) / 1000.0;
        fx_ += east / lean * speed;
        fy_ += north / lean * speed;
        at_ = Cell{int(std::lround(fx_)), int(std::lround(fy_))};
        moved_ = true;
    } else if (moved_) {  // let go: onto a monster or a friend within a cell
        moved_ = false;
        const Cell* near = nullptr;
        for (const Cell& c : snaps)
            if (cellDistance(c, at_) <= 1 && (!near || cellDistance(c, at_) < cellDistance(*near, at_))) near = &c;
        if (near) at_ = *near;
    }
    at_.x += nudge.x;
    at_.y += nudge.y;
    keepInReach(me);
    fx_ = std::clamp(fx_, double(at_.x) - 0.5, double(at_.x) + 0.5);  // the cursor stays with the cell it shows
    fy_ = std::clamp(fy_, double(at_.y) - 0.5, double(at_.y) + 0.5);
}

void GroundAim::keepInReach(Cell me) {
    at_.x = std::clamp(at_.x, me.x - range_, me.x + range_);
    at_.y = std::clamp(at_.y, me.y - range_, me.y + range_);
}

}  // namespace uapad
