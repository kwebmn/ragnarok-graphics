#include "core/near_picture.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

namespace uagfx::pack {
namespace {

// The same picture decoded elsewhere and kept in 16 bits is some 4 apart at most; two pictures are tens apart.
constexpr double kNear = 8.0;

}  // namespace

Likeness likenessOf(const uint32_t* argb, int width, int height, int cw, int ch) {
    Likeness l;
    l.width = width;
    l.height = height;
    l.contentWidth = cw = std::min(cw, width);
    l.contentHeight = ch = std::min(ch, height);
    if (!argb || cw <= 0 || ch <= 0) return l;
    for (int cy = 0; cy < kLikenessCells; ++cy)
        for (int cx = 0; cx < kLikenessCells; ++cx) {
            const int x0 = cx * cw / kLikenessCells, x1 = std::max(x0 + 1, (cx + 1) * cw / kLikenessCells);
            const int y0 = cy * ch / kLikenessCells, y1 = std::max(y0 + 1, (cy + 1) * ch / kLikenessCells);
            uint64_t sum[3] = {0, 0, 0};
            uint64_t n = 0;
            for (int y = y0; y < y1 && y < ch; ++y)
                for (int x = x0; x < x1 && x < cw; ++x) {
                    const uint32_t p = argb[size_t(y) * size_t(width) + size_t(x)];
                    for (int c = 0; c < 3; ++c) sum[c] += (p >> (16 - 8 * c)) & 255;
                    ++n;
                }
            for (int c = 0; c < 3; ++c)
                l.grid[size_t((cy * kLikenessCells + cx) * 3 + c)] = uint8_t(n ? (sum[c] + n / 2) / n : 0);
        }
    return l;
}

std::string likenessText(const Likeness& l) {
    std::string t = std::to_string(l.width) + "\t" + std::to_string(l.height) + "\t" + std::to_string(l.contentWidth) +
                    "\t" + std::to_string(l.contentHeight) + "\t";
    static const char* const kHex = "0123456789abcdef";
    for (uint8_t v : l.grid) {
        t += kHex[v >> 4];
        t += kHex[v & 15];
    }
    return t;
}

bool parseLikeness(const std::string& text, Likeness& out) {
    Likeness l;
    int fields[4];
    size_t at = 0;
    for (int& f : fields) {
        const size_t tab = text.find('\t', at);
        if (tab == std::string::npos) return false;
        f = std::atoi(text.substr(at, tab - at).c_str());
        at = tab + 1;
    }
    l.width = fields[0];
    l.height = fields[1];
    l.contentWidth = fields[2];
    l.contentHeight = fields[3];
    if (l.width <= 0 || l.height <= 0 || l.contentWidth <= 0 || l.contentHeight <= 0 ||
        text.size() - at != l.grid.size() * 2)
        return false;
    const auto hex = [](char c) {
        return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    };
    for (size_t i = 0; i < l.grid.size(); ++i) {
        const int hi = hex(text[at + 2 * i]), lo = hex(text[at + 2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        l.grid[i] = uint8_t(hi << 4 | lo);
    }
    out = l;
    return true;
}

double distance(const Likeness& a, const Likeness& b) {
    // Each channel's shift over the whole picture first: a decoder's or 16 bits' darkening is no difference of the
    // pictures (an animation's frames differ by little more than it).
    double shift[3] = {0, 0, 0};
    const size_t cells = a.grid.size() / 3;
    for (size_t i = 0; i < a.grid.size(); ++i) shift[i % 3] += double(int(a.grid[i]) - int(b.grid[i]));
    for (double& s : shift) s /= double(cells);
    double sum = 0;
    for (size_t i = 0; i < a.grid.size(); ++i) sum += std::fabs(double(int(a.grid[i]) - int(b.grid[i])) - shift[i % 3]);
    return sum / double(a.grid.size()) + (std::fabs(shift[0]) + std::fabs(shift[1]) + std::fabs(shift[2])) / 12.0;
}

void NearPictures::add(const std::string& name, const Likeness& l) { all_.emplace_back(name, l); }

bool NearPictures::has(int width, int height) const {
    for (const auto& e : all_)
        if (e.second.width == width && e.second.height == height) return true;
    return false;
}

bool NearPictures::find(const uint32_t* argb, int width, int height, std::string& name) const {
    std::map<std::pair<int, int>, Likeness> game;  // the game texture's, once for each content size
    double best = kNear;
    bool found = false;
    for (const auto& e : all_) {
        const Likeness& l = e.second;
        if (l.width != width || l.height != height) continue;
        const auto size = std::make_pair(l.contentWidth, l.contentHeight);
        auto g = game.find(size);
        if (g == game.end())
            g = game.emplace(size, likenessOf(argb, width, height, l.contentWidth, l.contentHeight)).first;
        const double d = distance(g->second, l);
        if (d <= best) {
            best = d;
            name = e.first;
            found = true;
        }
    }
    return found;
}

}  // namespace uagfx::pack
