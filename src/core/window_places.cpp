#include "core/window_places.hpp"

#include <cerrno>
#include <cstdlib>

namespace uagfx::places {
namespace {

// A whole number at `p` up to a space, the place after it into `end`
bool number(const char* p, int& out, const char*& end) {
    char* stop = nullptr;
    errno = 0;
    const long v = std::strtol(p, &stop, 10);
    if (stop == p || *stop != ' ' || errno != 0 || v < -1000000 || v > 1000000) return false;
    out = int(v);
    end = stop + 1;
    return true;
}

}  // namespace

Places parse(const std::string& text) {
    Places out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == ';') continue;
        Place p;
        const char* rest = nullptr;
        if (!number(line.c_str(), p.x, rest) || !number(rest, p.y, rest) || !*rest) continue;
        out[rest] = p;
    }
    return out;
}

Step Tracker::step(const std::string& name, bool movable, Place now, uint64_t ms, const Lookup& lookup) {
    Step s;
    if (name.empty() || !movable) {
        placed_ = name;
        known_ = false;
        return s;
    }
    if (name != placed_) {  // the character came
        placed_ = name;
        at(now);
        const std::optional<Place> kept = lookup ? lookup(name) : std::nullopt;
        if (kept && (kept->x != now.x || kept->y != now.y)) {
            s.what = Step::Do::Move;
            s.place = *kept;
        }
        return s;
    }
    if (!known_) {
        at(now);
        return s;
    }
    if (now.x == at_.x && now.y == at_.y) return s;
    if (now.x != moved_.x || now.y != moved_.y) {  // moved: kept once it stays
        moved_ = now;
        movedAt_ = ms;
        return s;
    }
    if (ms - movedAt_ < kStayMs) return s;
    at_ = now;
    s.what = Step::Do::Keep;
    s.place = now;
    return s;
}

void Tracker::at(Place where) {
    at_ = moved_ = where;
    known_ = true;
}

std::string format(const Places& places) {
    std::string out =
        "; uaRO Graphics: where each character's game window was put - x y name (the game writes this file)\n";
    for (const auto& [name, p] : places) out += std::to_string(p.x) + " " + std::to_string(p.y) + " " + name + "\n";
    return out;
}

}  // namespace uagfx::places
