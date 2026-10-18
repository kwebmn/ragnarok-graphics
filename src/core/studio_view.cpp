#include "core/studio_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace uagfx::studio {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

bool startsWith(const std::string& s, const std::string& head) { return s.compare(0, head.size(), head) == 0; }

}  // namespace

View zoomAt(View v, float factor, float cursorX, float cursorY) {
    const float zoom = std::clamp(v.zoom * factor, kLeastZoom, kMostZoom);
    const float tx = v.x + cursorX / v.zoom, ty = v.y + cursorY / v.zoom;
    return View{zoom, tx - cursorX / zoom, ty - cursorY / zoom};
}

void sourceTexel(const View& v, int paneX, int paneY, int w, int h, int pw, int ph, int& sx, int& sy, bool& inside) {
    const float ox = v.x + (float(paneX) + 0.5f) / v.zoom, oy = v.y + (float(paneY) + 0.5f) / v.zoom;
    inside = ox >= 0 && oy >= 0 && ox < float(w) && oy < float(h);
    sx = std::clamp(int(std::floor(ox * float(pw) / float(w))), 0, std::max(pw - 1, 0));
    sy = std::clamp(int(std::floor(oy * float(ph) / float(h))), 0, std::max(ph - 1, 0));
}

std::vector<uint32_t> differenceImage(const uint32_t* a, int aw, int ah, const uint32_t* b, int bw, int bh, int w,
                                      int h) {
    std::vector<uint32_t> out(size_t(std::max(w, 0)) * size_t(std::max(h, 0)), 0xFF000000u);
    if (!a || !b || aw <= 0 || ah <= 0 || bw <= 0 || bh <= 0) return out;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const auto at = [&](const uint32_t* img, int iw, int ih) {
                return img[size_t(std::min(ih - 1, y * ih / h)) * size_t(iw) + size_t(std::min(iw - 1, x * iw / w))];
            };
            const uint32_t p = at(a, aw, ah), q = at(b, bw, bh);
            uint32_t d = 0xFF000000u;
            for (int c = 0; c < 3; ++c) {
                const int diff = std::abs(int(p >> (8 * c) & 0xFF) - int(q >> (8 * c) & 0xFF)) * 4;
                d |= uint32_t(std::min(diff, 255)) << (8 * c);
            }
            out[size_t(y) * size_t(w) + size_t(x)] = d;
        }
    return out;
}

bool rowShown(const catalog::FileEntry& f, const catalog::PictureOf& p, const RowFacts& facts, const RowFilter& r) {
    const std::string& n = r.nav;
    if (startsWith(n, "map:")) {
        const uint32_t m = uint32_t(std::strtoul(n.c_str() + 4, nullptr, 10));
        if (std::find(f.maps.begin(), f.maps.end(), m) == f.maps.end()) return false;
    } else if (startsWith(n, "folder:")) {
        if (!startsWith(f.name, n.substr(7))) return false;
    } else if (n == "state:missing") {
        if (facts.state != catalog::State::Missing) return false;
    } else if (n == "state:inpack") {
        if (facts.state != catalog::State::InPack) return false;
    } else if (n == "state:own") {
        if (facts.state != catalog::State::OwnAlways) return false;
    } else if (startsWith(n, "model:")) {
        if (facts.model != n.substr(6)) return false;
    } else if (n == "flagged") {
        if (!facts.flags) return false;
    } else if (n == "overrides") {
        if (!facts.overrides) return false;
    } else if (startsWith(n, "seen:")) {
        if (!r.keys.count(p.key)) return false;
    }
    if (r.text.empty()) return true;
    const std::string t = lower(r.text);
    if (lower(f.name).find(t) != std::string::npos || lower(p.key).find(t) != std::string::npos) return true;
    for (const std::string& k : p.lowKeys)  // 2.3.1: a game at a lower texture detail picks it by its lower key
        if (lower(k).find(t) != std::string::npos) return true;
    return false;
}

}  // namespace uagfx::studio
