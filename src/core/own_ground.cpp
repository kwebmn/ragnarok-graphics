#include "core/own_ground.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::own_ground {

bool viewOf(const float viewToWorld[12], View& out) {
    const float* m = viewToWorld;
    const float a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], g = m[5], h = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * h) + c * (d * i - e * h);
    if (std::fabs(det) < 1e-8f) return false;
    const float r[9] = {(e * k - g * i) / det, (c * i - b * k) / det, (b * g - c * e) / det,
                        (g * h - d * k) / det, (a * k - c * h) / det, (c * d - a * g) / det,
                        (d * i - e * h) / det, (b * h - a * i) / det, (a * e - b * d) / det};
    std::copy(r, r + 9, out.r);
    std::copy(m + 9, m + 12, out.eye);
    return true;
}

float depthOf(const camera::Projection& p, float zd) {
    const auto game = [&p](float z) { return p.farZ * (1.0f - p.nearZ / z) / (p.farZ - p.nearZ); };
    const float split = p.farZ - kSqueeze;
    if (zd <= split) return game(std::max(zd, p.nearZ));
    const float at = game(split);
    return std::min(at + (1.0f - at) * (1.0f - split / zd), std::nextafter(1.0f, 0.0f));
}

bool place(const View& v, const camera::Projection& p, const map::Axes& axes, const float file[3], Tl2& out) {
    const float w[3] = {file[0] * float(axes.x), file[1] * float(axes.y), file[2] * float(axes.z)};
    float c[3];
    toView(v, w, c);
    if (c[2] <= p.nearZ) return false;
    out.x = c[0] / c[2] * p.f0 + p.cx;
    out.y = c[1] / c[2] * p.f1 + p.cy;
    out.z = depthOf(p, c[2]);
    out.rhw = 1.0f / c[2];
    return true;
}

bool boxInView(const View& v, const camera::Projection& p, int width, int height, const float lo[3], const float hi[3],
               float most, float& nearest) {
    int front = 0, left = 0, right = 0, above = 0, below = 0;
    nearest = 1e30f;
    for (int k = 0; k < 8; ++k) {
        const float w[3] = {k & 1 ? hi[0] : lo[0], k & 2 ? hi[1] : lo[1], k & 4 ? hi[2] : lo[2]};
        float c[3];
        toView(v, w, c);
        nearest = std::min(nearest, std::max(c[2], 0.0f));
        if (c[2] <= p.nearZ) continue;
        ++front;
        const float x = c[0] / c[2] * p.f0 + p.cx, y = c[1] / c[2] * p.f1 + p.cy;
        left += x < 0.0f;
        right += x > float(width);
        above += y < 0.0f;
        below += y > float(height);
    }
    if (front == 0 || nearest > most) return false;
    if (front < 8) return true;  // round the eye: no side to tell
    return left < 8 && right < 8 && above < 8 && below < 8;
}

void Shading::reset(int width, int height, float zoom) {
    width_ = std::max(width, 0);
    height_ = std::max(height, 0);
    zoom_ = zoom > 0.0f ? zoom : 1.0f;
    corners_.assign(size_t(width_ + 1) * size_t(height_ + 1), 0u);
    std::fill(sum_, sum_ + 4, 0);
    seen_ = 0;
    beginFrame();
    fit();
}

void Shading::note(float x, float z, uint32_t diffuse) {
    const int i = int(std::lround(x / zoom_ + float(width_) / 2.0f));
    const int j = int(std::lround(z / zoom_ + float(height_) / 2.0f));
    if (i < 0 || j < 0 || i > width_ || j > height_ || corners_.empty() || !diffuse) return;
    uint32_t& c = corners_[size_t(j) * size_t(width_ + 1) + size_t(i)];
    if (c == diffuse) return;
    for (int k = 0; k < 4; ++k) {
        if (c) sum_[k] -= (c >> (k * 8)) & 0xFF;
        sum_[k] += (diffuse >> (k * 8)) & 0xFF;
    }
    seen_ += c ? 0 : 1;
    c = diffuse;
}

uint32_t Shading::cornerOr(int i, int j, uint32_t none) const {
    i = std::clamp(i, 0, width_);
    j = std::clamp(j, 0, height_);
    const uint32_t c = corners_.empty() ? 0u : corners_[size_t(j) * size_t(width_ + 1) + size_t(i)];
    return c ? c : none;
}

uint32_t Shading::diffuseAt(float x, float z) const {
    uint32_t mean = 0xFFFFFFFFu;
    if (seen_) {
        mean = 0;
        for (int k = 0; k < 4; ++k) mean |= uint32_t((sum_[k] + seen_ / 2) / seen_) << (k * 8);
    }
    const float fx = x / zoom_ + float(width_) / 2.0f, fz = z / zoom_ + float(height_) / 2.0f;
    const int i = int(std::floor(fx)), j = int(std::floor(fz));
    const float tx = std::clamp(fx - float(i), 0.0f, 1.0f), tz = std::clamp(fz - float(j), 0.0f, 1.0f);
    const uint32_t c00 = cornerOr(i, j, mean), c10 = cornerOr(i + 1, j, mean), c01 = cornerOr(i, j + 1, mean),
                   c11 = cornerOr(i + 1, j + 1, mean);
    uint32_t out = 0;
    for (int k = 0; k < 32; k += 8) {
        const float a = float((c00 >> k) & 0xFF) + (float((c10 >> k) & 0xFF) - float((c00 >> k) & 0xFF)) * tx;
        const float b = float((c01 >> k) & 0xFF) + (float((c11 >> k) & 0xFF) - float((c01 >> k) & 0xFF)) * tx;
        out |= uint32_t(std::clamp(int(std::lround(a + (b - a) * tz)), 0, 255)) << k;
    }
    return out;
}

void Shading::beginFrame() {
    n_ = sz_ = sa_ = szz_ = sza_ = 0.0;
    fogged_ = false;
}

void Shading::fog(float zd, uint32_t specular) {
    const double a = double(specular >> 24);
    n_ += 1.0;
    sz_ += zd;
    sa_ += a;
    szz_ += double(zd) * zd;
    sza_ += double(zd) * a;
    fogged_ = fogged_ || a < 255.0;
    specularRgb_ = specular & 0xFFFFFFu;
}

void Shading::fit() {
    fogA_ = 255.0f;
    fogB_ = 0.0f;
    if (!fogged_ || n_ < 2.0) return;
    const double d = n_ * szz_ - sz_ * sz_;
    if (std::fabs(d) < 1e-9) {
        fogA_ = float(sa_ / n_);  // one depth: its alpha
        return;
    }
    const double b = (n_ * sza_ - sz_ * sa_) / d;
    fogA_ = float((sa_ - b * sz_) / n_);
    fogB_ = float(b);
}

uint32_t Shading::specularAt(float zd) const {
    const int a = std::clamp(int(std::lround(fogA_ + fogB_ * zd)), 0, 255);
    return uint32_t(a) << 24 | specularRgb_;
}

}  // namespace uagfx::own_ground
