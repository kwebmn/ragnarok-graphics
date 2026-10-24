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
    return placeView(p, c, out);
}

bool placeView(const camera::Projection& p, const float c[3], Tl2& out) {
    if (c[2] <= p.nearZ) return false;
    out.x = c[0] / c[2] * p.f0 + p.cx;
    out.y = c[1] / c[2] * p.f1 + p.cy;
    out.z = depthOf(p, c[2]);
    out.rhw = 1.0f / c[2];
    return true;
}

int pastDepth(const float zd[3], float from, float out[4][3]) {
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        const int j = (i + 1) % 3;
        const bool here = zd[i] >= from, next = zd[j] >= from;
        if (here) {
            for (int k = 0; k < 3; ++k) out[n][k] = k == i ? 1.0f : 0.0f;
            ++n;
        }
        if (here != next) {  // the edge crosses the plane
            const float t = (from - zd[i]) / (zd[j] - zd[i]);
            for (int k = 0; k < 3; ++k) out[n][k] = (k == i ? 1.0f - t : 0.0f) + (k == j ? t : 0.0f);
            ++n;
        }
    }
    return n;
}

bool boxInView(const View& v, const camera::Projection& p, int width, int height, const float lo[3], const float hi[3],
               float most, float& nearest, float* farthest) {
    // 3.0.4-beta.12: by the view's planes - through the eye and the screen's edges, the near one, `most` - which hold
    // for corners behind the eye too: all eight outside one of them, none of it shows
    const float l = -p.cx / p.f0, r = (float(width) - p.cx) / p.f0, t = -p.cy / p.f1, b = (float(height) - p.cy) / p.f1;
    int out[6] = {0, 0, 0, 0, 0, 0};
    nearest = 1e30f;
    if (farthest) *farthest = -1e30f;
    for (int k = 0; k < 8; ++k) {
        const float w[3] = {k & 1 ? hi[0] : lo[0], k & 2 ? hi[1] : lo[1], k & 4 ? hi[2] : lo[2]};
        float c[3];
        toView(v, w, c);
        nearest = std::min(nearest, std::max(c[2], 0.0f));
        if (farthest) *farthest = std::max(*farthest, c[2]);
        out[0] += c[2] <= p.nearZ;
        out[1] += c[2] > most;
        out[2] += c[0] < l * c[2];
        out[3] += c[0] > r * c[2];
        out[4] += c[1] < t * c[2];
        out[5] += c[1] > b * c[2];
    }
    for (int n : out)
        if (n == 8) return false;
    return true;
}

void Shading::reset(int width, int height, float zoom) {
    width_ = std::max(width, 0);
    height_ = std::max(height, 0);
    zoom_ = zoom > 0.0f ? zoom : 1.0f;
    corners_.assign(size_t(width_ + 1) * size_t(height_ + 1), 0u);
    std::fill(sum_, sum_ + 4, 0);
    seen_ = 0;
    changed_.clear();
    tooMany_ = false;
    toldMean_ = 0xFFFFFFFFu;
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
    if (changed_.size() < 65536)
        changed_.push_back(uint32_t(j) * uint32_t(width_ + 1) + uint32_t(i));
    else
        tooMany_ = true;
}

uint32_t Shading::mean() const {
    if (!seen_) return 0xFFFFFFFFu;
    uint32_t m = 0;
    for (int k = 0; k < 4; ++k) m |= uint32_t((sum_[k] + seen_ / 2) / seen_) << (k * 8);
    return m;
}

void Shading::takeChanged(std::vector<uint32_t>& corners, bool& mean) {
    corners.swap(changed_);
    changed_.clear();
    const uint32_t now = this->mean();
    bool moved = tooMany_;
    for (int k = 0; k < 32; k += 8)
        moved = moved || std::abs(int((now >> k) & 0xFF) - int((toldMean_ >> k) & 0xFF)) > 4;
    if (moved) toldMean_ = now;
    tooMany_ = false;
    mean = moved;
}

uint32_t Shading::cornerOr(int i, int j, uint32_t none) const {
    i = std::clamp(i, 0, width_);
    j = std::clamp(j, 0, height_);
    const uint32_t c = corners_.empty() ? 0u : corners_[size_t(j) * size_t(width_ + 1) + size_t(i)];
    return c ? c : none;
}

uint32_t Shading::diffuseAt(float x, float z) const {
    const uint32_t mean = this->mean();
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
