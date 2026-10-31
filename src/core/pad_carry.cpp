#include "core/pad_carry.hpp"

#include "core/equip_window.hpp"

namespace uagfx::padcarry {

bool takes(const Thing& t, const Place& p) {
    switch (p.to) {
        case To::Slot: return t.from == From::Bag && t.location != 0 && equipwin::fits(t.location, p.index);
        case To::Cell: return t.from == From::Bag || t.from == From::Skill;
        case To::Bag: return t.from == From::Storage;
        case To::Storage:
        case To::Trade: return t.from == From::Bag;
    }
    return false;
}

std::vector<Place> placesFor(const Thing& t, const std::vector<Place>& all) {
    std::vector<Place> out;
    for (const Place& p : all)
        if (takes(t, p)) out.push_back(p);
    return out;
}

int nextPlace(const std::vector<Place>& places, int from, Dir d) {
    if (from < 0 || from >= int(places.size())) return from;
    const float fx = places[size_t(from)].x + places[size_t(from)].w / 2;
    const float fy = places[size_t(from)].y + places[size_t(from)].h / 2;
    int best = from;
    float bestScore = 0;
    for (size_t i = 0; i < places.size(); ++i) {
        if (int(i) == from) continue;
        const float dx = places[i].x + places[i].w / 2 - fx, dy = places[i].y + places[i].h / 2 - fy;
        const float along = d == Dir::Right ? dx : d == Dir::Left ? -dx : d == Dir::Down ? dy : -dy;
        const float across = d == Dir::Right || d == Dir::Left ? (dy < 0 ? -dy : dy) : (dx < 0 ? -dx : dx);
        if (along <= 1 || across > 2 * along) continue;  // not that way (more across than twice along: aside)
        const float score = along + 3 * across;
        if (best == from || score < bestScore) {
            best = int(i);
            bestScore = score;
        }
    }
    return best;
}

int nearestPlace(const std::vector<Place>& places, float x, float y) {
    int best = -1;
    float bestD = 0;
    for (size_t i = 0; i < places.size(); ++i) {
        const float dx = places[i].x + places[i].w / 2 - x, dy = places[i].y + places[i].h / 2 - y;
        const float d = dx * dx + dy * dy;
        if (best < 0 || d < bestD) {
            best = int(i);
            bestD = d;
        }
    }
    return best;
}

}  // namespace uagfx::padcarry
