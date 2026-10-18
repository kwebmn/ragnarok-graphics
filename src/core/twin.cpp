#include "core/twin.hpp"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace uagfx {

bool transparentAtUpload(uint32_t p) { return (p >> 19 & 31) == 31 && (p >> 11 & 31) == 0 && (p >> 3 & 31) == 31; }

uint32_t twinValue(uint32_t p) { return transparentAtUpload(p) ? 0u : 0xFF000000u | (p & 0xFFFFFFu); }

uint32_t over(uint32_t src, uint32_t dst) {
    const uint32_t keep = 255 - (src >> 24);
    if (keep == 0) return src;
    if (src == 0) return dst;
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8) {
        const uint32_t c = (src >> s & 255) + ((dst >> s & 255) * keep + 127) / 255;
        out |= std::min(c, 255u) << s;
    }
    return out;
}

bool copyFromRegion(int x, int y, int w, int h, int width, int height, Rect& out) {
    if (width <= 0 || height <= 0 || uint32_t(x) >= uint32_t(width) || uint32_t(y) >= uint32_t(height)) return false;
    w = std::min(w, width - x);
    h = std::min(h, height - y);
    if (w <= 0 || h <= 0) return false;
    out = Rect{x, y, x + w, y + h};
    return true;
}

int copyPixelsRows(const uint32_t* dib, int width, int height, int x, int y, int h) {
    const int64_t size = int64_t(width) * height;
    for (int r = 0; r < h; ++r) {
        const int64_t i = (int64_t(y) + r) * width + x;
        if (i >= 0 && i < size && dib[i] == 0) return r;
    }
    return std::max(h, 0);
}

uint64_t hashPixels(const uint32_t* dib, int width, Rect r) {
    uint64_t h = 1469598103934665603ull;
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x) {
            h ^= dib[size_t(y) * size_t(width) + size_t(x)] & 0xFFFFFFu;
            h *= 1099511628211ull;
        }
    return h;
}

TwinPicture::TwinPicture(const uint32_t* dib, int width, int height, double K)
    : w_(std::max(width, 0)), h_(std::max(height, 0)), k_(K), ex_(size_t(w_) + 1), ey_(size_t(h_) + 1) {
    for (int g = 0; g <= w_; ++g) ex_[size_t(g)] = twinEdge(g, K);
    for (int g = 0; g <= h_; ++g) ey_[size_t(g)] = twinEdge(g, K);
    twin_.assign(size_t(twinWidth()) * size_t(twinHeight()), 0u);
    copy_.assign(size_t(w_) * size_t(h_), 0u);
    for (int y = 0; y < h_; ++y) fillRow(dib, y, 0, w_);
}

size_t TwinPicture::bytesFor(int width, int height, double K) {
    const size_t twin = size_t(std::max(twinEdge(width, K), 0)) * size_t(std::max(twinEdge(height, K), 0));
    return (twin + size_t(std::max(width, 0)) * size_t(std::max(height, 0))) * sizeof(uint32_t);
}

// The blocks of game pixels [x0, x1) of row y take dib's values, and so does the copy: the first twin row of the
// blocks pixel by pixel, the other rows as copies of it.
void TwinPicture::fillRow(const uint32_t* dib, int y, int x0, int x1) {
    const size_t tw = size_t(twinWidth());
    const uint32_t* src = dib + size_t(y) * size_t(w_);
    uint32_t* copy = copy_.data() + size_t(y) * size_t(w_);
    uint32_t* first = twin_.data() + size_t(ey_[size_t(y)]) * tw;
    for (int x = x0; x < x1; ++x) {
        copy[x] = src[x];
        std::fill(first + ex_[size_t(x)], first + ex_[size_t(x) + 1], twinValue(src[x]));
    }
    const int left = ex_[size_t(x0)];
    const int right = ex_[size_t(x1)];
    for (int ty = ey_[size_t(y)] + 1; ty < ey_[size_t(y) + 1]; ++ty)
        std::copy(first + left, first + right, twin_.data() + size_t(ty) * tw + size_t(left));
    touch(Rect{left, ey_[size_t(y)], right, ey_[size_t(y) + 1]});
}

void TwinPicture::overwrite(const uint32_t* dib, Rect r) {
    if (!clip(r)) return;
    for (int y = r.top; y < r.bottom; ++y) fillRow(dib, y, r.left, r.right);
}

int TwinPicture::sync(const uint32_t* dib, Rect r) {
    if (!clip(r)) return 0;
    int n = 0;
    for (int y = r.top; y < r.bottom; ++y) {
        const uint32_t* src = dib + size_t(y) * size_t(w_);
        const uint32_t* copy = copy_.data() + size_t(y) * size_t(w_);
        // 2.5.20: a row the same to the byte changed nothing - a window drawn again as it was (most, most frames)
        if (std::memcmp(src + r.left, copy + r.left, size_t(r.width()) * 4) == 0) continue;
        for (int x = r.left; x < r.right;) {
            if (((src[x] ^ copy[x]) & 0xFFFFFFu) == 0) {
                ++x;
                continue;
            }
            const int run = x;  // changed pixels side by side: one fillRow; the unchanged keep their twin
            while (x < r.right && ((src[x] ^ copy[x]) & 0xFFFFFFu) != 0) ++x;
            fillRow(dib, y, run, x);
            n += x - run;
        }
    }
    return n;
}

void TwinPicture::absorb(const uint32_t* dib, Rect r) {
    if (!clip(r)) return;
    for (int y = r.top; y < r.bottom; ++y) {
        const size_t i = size_t(y) * size_t(w_) + size_t(r.left);
        std::copy_n(dib + i, size_t(r.width()), copy_.data() + i);
    }
}

void TwinPicture::copyPixels(const uint32_t* dib, int x, int y, int w, int rows, const uint32_t* src, bool key) {
    const int64_t size = int64_t(w_) * h_;
    const auto skipped = [key](uint32_t v) { return key && v == 0xFFFF00FFu; };
    for (int r = 0; r < rows && w > 0; ++r) {
        const uint32_t* line = src + size_t(r) * size_t(w);
        for (int c = 0; c < w;) {
            if (skipped(line[c])) {
                ++c;
                continue;
            }
            const int run = c;  // written pixels side by side: their flat indices, cut to the picture, row by row
            while (c < w && !skipped(line[c])) ++c;
            int64_t a = std::max<int64_t>((int64_t(y) + r) * w_ + x + run, 0);
            const int64_t end = std::min<int64_t>((int64_t(y) + r) * w_ + x + c, size);
            while (a < end) {
                const int gy = int(a / w_);
                const int gx = int(a % w_);
                const int n = int(std::min<int64_t>(end - a, w_ - gx));
                fillRow(dib, gy, gx, gx + n);
                a += n;
            }
        }
    }
}

void TwinPicture::copyFrom(const uint32_t* dib, const TwinPicture& child, int x, int y, int w, int h) {
    Rect r{x, y, x + w, y + h};
    if (x < 0 || y < 0 || !clip(r)) return;
    if (child.k_ != k_) {
        sync(dib, r);
        return;
    }
    // The child's twin from its corner; where this picture's blocks need one more column or row than the child
    // has, its last one repeats (spec §3).
    const int cw = child.ex_[size_t(std::min(r.width(), child.w_))];
    const int ch = child.ey_[size_t(std::min(r.height(), child.h_))];
    if (cw > 0 && ch > 0) {
        const int x0 = ex_[size_t(r.left)];
        const int x1 = ex_[size_t(r.right)];
        const int y0 = ey_[size_t(r.top)];
        const int y1 = ey_[size_t(r.bottom)];
        const size_t tw = size_t(twinWidth());
        const size_t ctw = size_t(child.twinWidth());
        for (int py = y0; py < y1; ++py) {
            const uint32_t* src = child.twin_.data() + size_t(std::min(py - y0, ch - 1)) * ctw;
            uint32_t* dst = twin_.data() + size_t(py) * tw;
            for (int px = x0; px < x1; ++px) dst[px] = over(src[std::min(px - x0, cw - 1)], dst[px]);
        }
        touch(Rect{x0, y0, x1, y1});
    }
    // A child pixel the game copies - anything but exactly 0xFFFF00FF - that its upload makes transparent is a hole in
    // this picture, as in the game; "over" kept this picture's pixel (0.5.0 review M1).
    const auto hole = [](uint32_t v) { return v != 0xFFFF00FFu && transparentAtUpload(v); };
    for (int j = 0; j < std::min(r.height(), child.h_); ++j) {
        const uint32_t* line = child.copy_.data() + size_t(j) * size_t(child.w_);
        const int cols = std::min(r.width(), child.w_);
        for (int i = 0; i < cols;) {
            if (!hole(line[i])) {
                ++i;
                continue;
            }
            const int run = i;
            while (i < cols && hole(line[i])) ++i;
            fillRow(dib, r.top + j, r.left + run, r.left + i);
        }
    }
    absorb(dib, r);
}

void TwinPicture::texels(const Rect& r, std::vector<uint32_t>& out) const {
    out.resize(size_t(std::max(r.width(), 0)) * size_t(std::max(r.height(), 0)));
    for (int y = r.top; y < r.bottom; ++y)
        std::copy_n(twin_.data() + size_t(y) * size_t(twinWidth()) + size_t(r.left), size_t(r.width()),
                    out.data() + size_t(y - r.top) * size_t(r.width()));
}

Rect TwinPicture::takeChanged() {
    const Rect r = changed_;
    changed_ = Rect{};
    return r;
}

bool TwinPicture::changedPicture(const uint32_t* dib, Rect r, Rect& box, std::vector<uint32_t>& argb) const {
    if (!clipRect(Rect{0, 0, w_, h_}, r)) return false;
    box = Rect{r.right, r.bottom, r.left, r.top};
    const auto differs = [&](int x, int y) {
        const size_t i = size_t(y) * size_t(w_) + size_t(x);
        return ((dib[i] ^ copy_[i]) & 0x00FFFFFFu) != 0;
    };
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x)
            if (differs(x, y)) {
                box.left = std::min(box.left, x);
                box.top = std::min(box.top, y);
                box.right = std::max(box.right, x + 1);
                box.bottom = std::max(box.bottom, y + 1);
            }
    if (box.empty()) return false;
    argb.assign(size_t(box.width()) * size_t(box.height()), 0);
    for (int y = box.top; y < box.bottom; ++y)
        for (int x = box.left; x < box.right; ++x) {
            const uint32_t p = dib[size_t(y) * size_t(w_) + size_t(x)];
            if (differs(x, y) && !transparentAtUpload(p))
                argb[size_t(y - box.top) * size_t(box.width()) + size_t(x - box.left)] = p | 0xFF000000u;
        }
    return true;
}

void TwinPicture::paint(Rect r, const uint32_t* argb, int w, int h, bool overTwin) {
    if (r.empty() || r.left < 0 || r.top < 0 || r.right > w_ || r.bottom > h_ || w <= 0 || h <= 0) return;
    const int tx0 = ex_[size_t(r.left)], tx1 = ex_[size_t(r.right)];
    const int ty0 = ey_[size_t(r.top)], ty1 = ey_[size_t(r.bottom)];
    if (tx1 <= tx0 || ty1 <= ty0) return;
    const double sx = double(w) / double(tx1 - tx0), sy = double(h) / double(ty1 - ty0);
    for (int ty = ty0; ty < ty1; ++ty) {
        const double y0 = double(ty - ty0) * sy, y1 = y0 + sy;
        for (int tx = tx0; tx < tx1; ++tx) {
            const double x0 = double(tx - tx0) * sx, x1 = x0 + sx;
            double acc[4] = {0, 0, 0, 0}, area = 0;  // premultiplied blue, green, red; alpha
            for (int py = int(y0); py < std::min(h, int(std::ceil(y1))); ++py) {
                const double wy = std::min(y1, py + 1.0) - std::max(y0, double(py));
                if (wy <= 0) continue;
                for (int px = int(x0); px < std::min(w, int(std::ceil(x1))); ++px) {
                    const double wx = std::min(x1, px + 1.0) - std::max(x0, double(px));
                    if (wx <= 0) continue;
                    const uint32_t p = argb[size_t(py) * size_t(w) + size_t(px)];
                    const double f = wx * wy, a = double(p >> 24) / 255.0;
                    for (int c = 0; c < 3; ++c) acc[c] += f * a * double((p >> (8 * c)) & 255);
                    acc[3] += f * a;
                    area += f;
                }
            }
            uint32_t& d = twin_[size_t(ty) * size_t(twinWidth()) + size_t(tx)];
            if (area <= 0 || acc[3] <= 0) {
                if (!overTwin) d = 0;  // a hole, as the game's magenta makes one
                continue;
            }
            uint32_t src = uint32_t(std::lround(std::min(acc[3] / area, 1.0) * 255.0)) << 24;
            for (int c = 0; c < 3; ++c)
                src |= uint32_t(std::min<long>(std::lround(acc[c] / area), 255)) << (8 * c);
            d = overTwin ? over(src, d) : src;
        }
    }
    touch(Rect{tx0, ty0, tx1, ty1});
}

bool TwinPicture::twinOf(Rect r, Rect& out) const {
    if (r.empty() || r.left < 0 || r.top < 0 || r.right > w_ || r.bottom > h_) return false;
    out = Rect{ex_[size_t(r.left)], ey_[size_t(r.top)], ex_[size_t(r.right)], ey_[size_t(r.bottom)]};
    return !out.empty();
}

void TwinPicture::keep(Rect r, std::vector<uint32_t>& out) const {
    Rect t;
    if (twinOf(r, t))
        texels(t, out);
    else
        out.clear();
}

bool TwinPicture::put(Rect r, const std::vector<uint32_t>& in) {
    Rect t;
    if (!twinOf(r, t) || in.size() != size_t(t.width()) * size_t(t.height())) return false;
    for (int y = t.top; y < t.bottom; ++y)
        std::copy_n(in.data() + size_t(y - t.top) * size_t(t.width()), size_t(t.width()),
                    twin_.data() + size_t(y) * size_t(twinWidth()) + size_t(t.left));
    touch(t);
    return true;
}

void TwinPicture::touch(Rect r) {
    if (!clipRect(Rect{0, 0, twinWidth(), twinHeight()}, r)) return;
    if (changed_.empty()) {
        changed_ = r;
        return;
    }
    changed_.left = std::min(changed_.left, r.left);
    changed_.top = std::min(changed_.top, r.top);
    changed_.right = std::max(changed_.right, r.right);
    changed_.bottom = std::max(changed_.bottom, r.bottom);
}

}  // namespace uagfx
