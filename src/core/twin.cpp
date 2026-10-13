#include "core/twin.hpp"

#include <algorithm>

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
