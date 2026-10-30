#include "core/hud_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace uagfx::hudlayout {
namespace {

const char* const kKeys[kParts] = {"character", "minimap", "buffs", "tracker", "hotbar", "target", "party", "chat"};

float within(float size) { return std::clamp(size, kLeastSize, kMostSize); }

// The nearest line one of a box's edges (or its middle) can move to, nearer than `far`: the shift and the line
struct Best {
    int shift = 0, line = -1, far = 0;
    void take(int from, int to) {
        const int d = to - from;
        if (std::abs(d) < far && (line < 0 || std::abs(d) < std::abs(shift))) {
            shift = d;
            line = to;
        }
    }
};

}  // namespace

const char* keyOf(Part p) { return int(p) < kParts ? kKeys[int(p)] : ""; }

Layout parse(const std::string& text) {
    Layout out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::string key;
        int anchor = 0;
        float dx = 0, dy = 0, size = 1;
        if (!(words >> key >> anchor >> dx >> dy >> size) || anchor < -1 || anchor > 8 || !std::isfinite(dx) ||
            !std::isfinite(dy) || !std::isfinite(size))
            continue;
        for (int i = 0; i < kParts; ++i)
            if (key == kKeys[i]) out[size_t(i)] = Place{anchor >= 0, std::max(anchor, 0), dx, dy, within(size)};
    }
    return out;
}

std::string text(const Layout& layout) {
    std::string out;
    for (int i = 0; i < kParts; ++i) {
        const Place& p = layout[size_t(i)];
        if (!p.moved && std::fabs(p.size - 1) < 1e-3f) continue;
        char line[96];
        std::snprintf(line, sizeof line, "%s %d %.1f %.1f %.2f\n", kKeys[i], p.moved ? p.anchor : -1, p.dx, p.dy,
                      p.size);
        out += line;
    }
    return out;
}

void apply(const Place& p, float k, int targetW, int targetH, int w, int h, int& x, int& y) {
    if (!p.moved || k <= 0) return;
    const float fx = float(p.anchor % 3) * 0.5f, fy = float(p.anchor / 3) * 0.5f;
    x = int(std::lround(fx * float(targetW) + p.dx * k - fx * float(w)));
    y = int(std::lround(fy * float(targetH) + p.dy * k - fy * float(h)));
    x = std::clamp(x, 0, std::max(0, targetW - w));
    y = std::clamp(y, 0, std::max(0, targetH - h));
}

Place placeOf(const Box& b, float k, int targetW, int targetH, float size) {
    Place p;
    p.moved = true;
    p.size = within(size);
    if (k <= 0 || targetW <= 0 || targetH <= 0) return p;
    const float cx = float(b.x) + float(b.w) / 2, cy = float(b.y) + float(b.h) / 2;
    const int column = cx < float(targetW) / 3 ? 0 : cx < float(targetW) * 2 / 3 ? 1 : 2;
    const int row = cy < float(targetH) / 3 ? 0 : cy < float(targetH) * 2 / 3 ? 1 : 2;
    p.anchor = column + 3 * row;
    const float fx = float(column) * 0.5f, fy = float(row) * 0.5f;
    p.dx = (float(b.x) + fx * float(b.w) - fx * float(targetW)) / k;
    p.dy = (float(b.y) + fy * float(b.h) - fy * float(targetH)) / k;
    return p;
}

Snapped snap(const Box& m, const std::vector<Box>& others, int targetW, int targetH, int within) {
    Best bx{0, -1, within}, by{0, -1, within};
    bx.take(m.x, 0);
    bx.take(m.x + m.w, targetW);
    bx.take(m.x + m.w / 2, targetW / 2);
    by.take(m.y, 0);
    by.take(m.y + m.h, targetH);
    by.take(m.y + m.h / 2, targetH / 2);
    for (const Box& o : others) {
        for (const int edge : {o.x, o.x + o.w}) bx.take(m.x, edge);
        for (const int edge : {o.x, o.x + o.w}) bx.take(m.x + m.w, edge);
        for (const int edge : {o.y, o.y + o.h}) by.take(m.y, edge);
        for (const int edge : {o.y, o.y + o.h}) by.take(m.y + m.h, edge);
    }
    return Snapped{m.x + (bx.line >= 0 ? bx.shift : 0), m.y + (by.line >= 0 ? by.shift : 0), bx.line, by.line};
}

Box clearOf(const Box& moving, const std::vector<Box>& others, int targetW, int targetH, int gap) {
    const auto overlaps = [&](const Box& b) {
        for (const Box& o : others)
            if (b.x < o.x + o.w && b.x + b.w > o.x && b.y < o.y + o.h && b.y + b.h > o.y) return true;
        return false;
    };
    if (!overlaps(moving)) return moving;
    const auto kept = [&](Box b) {
        b.x = std::clamp(b.x, 0, std::max(0, targetW - b.w));
        b.y = std::clamp(b.y, 0, std::max(0, targetH - b.h));
        return b;
    };
    const auto far = [&](const Box& b) {
        return int64_t(b.x - moving.x) * (b.x - moving.x) + int64_t(b.y - moving.y) * (b.y - moving.y);
    };
    // beside each of the others, then beside them again from there: the nearest place clear of them all
    std::vector<Box> tried{moving};
    Box best = moving;
    int64_t bestFar = -1;
    for (int round = 0; round < 3 && bestFar < 0; ++round) {
        std::vector<Box> next;
        for (const Box& from : tried)
            for (const Box& o : others)
                for (const Box& b : {Box{o.x - from.w - gap, from.y, from.w, from.h},
                                     Box{o.x + o.w + gap, from.y, from.w, from.h},
                                     Box{from.x, o.y - from.h - gap, from.w, from.h},
                                     Box{from.x, o.y + o.h + gap, from.w, from.h}}) {
                    const Box c = kept(b);
                    if (!overlaps(c)) {
                        if (bestFar < 0 || far(c) < bestFar) {
                            best = c;
                            bestFar = far(c);
                        }
                    } else if (next.size() < 256) {
                        next.push_back(c);
                    }
                }
        tried = std::move(next);
    }
    return best;
}

float stepped(float size, int steps) {
    const float v = std::round((size + float(steps) * kSizeStep) / kSizeStep) * kSizeStep;
    return within(v);
}

}  // namespace uagfx::hudlayout
