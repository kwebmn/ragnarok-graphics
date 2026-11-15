#include "core/model_census.hpp"

#include <cstdio>
#include <cstdlib>

namespace uagfx::modelcensus {

// push ebp; mov ebp, esp; sub esp, 0x88
const uint8_t kNodeDrawBytes[kNodeDrawSize] = {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00};

void Check::add(const Check& c) {
    faces += c.faces;
    vertices += c.vertices;
    same += c.same;
    ratio += c.ratio;
    ratios += c.ratios;
    if (c.mostApart > mostApart) mostApart = c.mostApart;
}

namespace {

bool sameUv(const FaceColours& f, const DrawnVertex* d) {
    for (int i = 0; i < 3; ++i)
        if (d[i].u != f.uv[2 * i] || d[i].v != f.uv[2 * i + 1]) return false;
    return true;
}

}  // namespace

Check compare(const std::vector<FaceColours>& faces, const DrawnVertex* drawn, size_t count) {
    Check c;
    size_t next = 0;
    for (size_t at = 0; at + 3 <= count; at += 3) {
        size_t f = next;
        while (f < faces.size() && !sameUv(faces[f], drawn + at)) ++f;
        if (f == faces.size()) break;  // the block's rest
        next = f + 1;
        ++c.faces;
        for (int i = 0; i < 3; ++i) {
            const uint32_t mine = faces[f].colour[i], theirs = drawn[at + size_t(i)].diffuse;
            ++c.vertices;
            if ((mine & 0xFFFFFFu) == (theirs & 0xFFFFFFu)) {
                ++c.same;
                continue;
            }
            for (int b = 0; b < 3; ++b) {
                const int m = int((mine >> (8 * b)) & 0xFF), t = int((theirs >> (8 * b)) & 0xFF);
                if (std::abs(m - t) > c.mostApart) c.mostApart = std::abs(m - t);
                if (m > 8) {
                    c.ratio += double(t) / double(m);
                    ++c.ratios;
                }
            }
        }
    }
    return c;
}

void Tally::frame(const Frame& f, double frameMs) {
    ++frames_;
    nodes_ += f.nodes;
    faces_ += f.faces;
    ms_ += f.ms;
    frameMs_ += frameMs;
}

void Tally::check(const Check& c, Way way) {
    check_.add(c);
    ++ways_[int(way)];
}

std::string Tally::line() const {
    if (!frames_) return "";
    char text[640];
    int n = std::snprintf(text, sizeof text,
                          "models: the game's own a frame - %.0f nodes, %.0f faces, %.2f ms (%.0f %% of its frame)",
                          double(nodes_) / frames_, double(faces_) / frames_, ms_ / frames_,
                          frameMs_ > 0 ? ms_ * 100.0 / frameMs_ : 0.0);
    if (check_.vertices && n > 0 && size_t(n) < sizeof text) {
        n += std::snprintf(text + n, sizeof text - size_t(n),
                           "; colours: %u vertices of %u faces checked - %u (%.1f %%) the very same as in its memory",
                           check_.vertices, check_.faces, check_.same, check_.same * 100.0 / check_.vertices);
        if (check_.ratios && n > 0 && size_t(n) < sizeof text)
            n += std::snprintf(text + n, sizeof text - size_t(n), ", the rest x%.2f on average (%d apart at most)",
                               check_.ratio / check_.ratios, check_.mostApart);
        if (n > 0 && size_t(n) < sizeof text)
            std::snprintf(text + n, sizeof text - size_t(n),
                          "; ways: plain %u, night %u, half see-through %u, other %u", ways_[0], ways_[1], ways_[2],
                          ways_[3]);
    }
    return text;
}

}  // namespace uagfx::modelcensus
