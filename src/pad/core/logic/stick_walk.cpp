#include "pad/core/logic/stick_walk.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace uapad::stickwalk {
namespace {

constexpr uint16_t kDefaultSpeedMs = 150;
constexpr double kLeadMs = 600;
constexpr int kLeadLeast = 3, kLeadMost = 6;
constexpr double kQuarter = 3.14159265358979323846 / 4;

Way unit(double east, double north) {
    const double len = std::hypot(east, north);
    return len > 1e-9 ? Way{east / len, north / len} : Way{};
}

Way turned(Way w, double by) {
    const double c = std::cos(by), s = std::sin(by);
    return Way{w.east * c - w.north * s, w.east * s + w.north * c};
}

bool walks(const GroundGrid* ground, Cell c) { return !ground || !ground->known() || ground->at(c.x, c.y); }

// Half a cell a step along the way: the last walkable cell within `lead` cells; `cut` - stopped by one that does not
// walk
Cell march(Cell here, Way way, int lead, const GroundGrid* ground, bool& cut) {
    Cell dest = here;
    cut = false;
    for (int i = 1; i <= 2 * lead; ++i) {
        const double t = 0.5 * double(i);
        const Cell c{here.x + int(std::lround(way.east * t)), here.y + int(std::lround(way.north * t))};
        if (!walks(ground, c)) {
            cut = true;
            break;
        }
        dest = c;
    }
    return dest;
}

}  // namespace

Way wayOf(double sx, double sy, std::optional<std::pair<double, double>> look) {
    if (!look) return unit(sx, sy);
    const double e = look->first, n = look->second;
    return unit(sx * n + sy * e, -sx * e + sy * n);
}

double turnBetween(Way a, Way b) {
    const double dot = std::clamp(a.east * b.east + a.north * b.north, -1.0, 1.0);
    return std::acos(dot);
}

int lead(uint16_t speedMs) {
    const double speed = speedMs ? double(speedMs) : double(kDefaultSpeedMs);
    return std::clamp(int(std::lround(kLeadMs / speed)) + 1, kLeadLeast, kLeadMost);
}

Cell destination(Cell here, Way way, int lead, const GroundGrid* ground, bool* cut) {
    if (ground && ground->known() && !ground->at(here.x, here.y)) ground = nullptr;  // the grid not this map's
    bool stopped = false;
    const Cell straight = march(here, way, lead, ground, stopped);
    if (cut) *cut = stopped;
    if (straight != here || !ground) return straight;
    std::vector<Way> slides = {turned(way, kQuarter), turned(way, -kQuarter)};  // blocked at once: along the wall
    if (std::fabs(way.east) > 1e-9) slides.push_back(Way{way.east > 0 ? 1.0 : -1.0, 0});
    if (std::fabs(way.north) > 1e-9) slides.push_back(Way{0, way.north > 0 ? 1.0 : -1.0});
    Cell best = here;
    int bestRun = 0;
    double bestTurn = 10;
    for (const Way& w : slides) {
        bool slideCut = false;
        const Cell c = march(here, w, lead, ground, slideCut);
        const int run = cellDistance(here, c);
        const double turn = turnBetween(w, way);
        if (run > bestRun || (run == bestRun && run > 0 && turn < bestTurn)) {
            best = c;
            bestRun = run;
            bestTurn = turn;
            if (cut) *cut = slideCut;
        }
    }
    return best;  // nowhere: still cut (the straight march's wall)
}

Cell stopCell(Cell here, Way way, const GroundGrid* ground) {
    const Cell next{here.x + int(std::lround(way.east)), here.y + int(std::lround(way.north))};
    return walks(ground, next) ? next : here;
}

bool askAgain(const std::optional<Asked>& last, Cell here, Cell dest, Way way, int lead, uint32_t nowMs) {
    if (!last) return true;
    const uint32_t since = nowMs - last->atMs;
    if (since < kMinGapMs || dest == last->to) return false;
    if (turnBetween(way, last->way) > kTurnAgain) return true;
    const Way toDest = unit(dest.x - here.x, dest.y - here.y), toLast = unit(last->to.x - here.x, last->to.y - here.y);
    if (dest != here && last->to != here && turnBetween(toDest, toLast) > kTurnAgain) return true;  // moved aside
    if (cellDistance(here, last->to) <= std::max(1, lead / 2)) return true;
    return since >= kRefreshMs;
}

}  // namespace uapad::stickwalk
