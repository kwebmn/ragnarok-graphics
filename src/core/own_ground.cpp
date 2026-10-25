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

namespace {

constexpr float kBelowOne = 0.99999994f;  // the largest float under 1

// 3.0.4-beta.15: depthOf from the depth's inverse - one division a vertex, not four (R-N6FC5V: 7.7 ms a frame)
float depthFromInverse(const camera::Projection& p, float zd, float inv) {
    const float a = p.farZ / (p.farZ - p.nearZ), b = p.farZ * p.nearZ / (p.farZ - p.nearZ);
    const float split = p.farZ - kSqueeze;
    if (zd <= split) return a - b * inv;
    const float at = a - b / split;
    return std::min(at + (1.0f - at) * (1.0f - split * inv), kBelowOne);
}

}  // namespace

float depthOf(const camera::Projection& p, float zd) {
    const float z = std::max(zd, p.nearZ);
    return depthFromInverse(p, z, 1.0f / z);
}

bool place(const View& v, const camera::Projection& p, const map::Axes& axes, const float file[3], Tl2& out) {
    const float w[3] = {file[0] * float(axes.x), file[1] * float(axes.y), file[2] * float(axes.z)};
    float c[3];
    toView(v, w, c);
    return placeView(p, c, out);
}

bool placeView(const camera::Projection& p, const float c[3], Tl2& out) {
    if (c[2] <= p.nearZ) return false;
    const float inv = 1.0f / c[2];
    out.x = c[0] * inv * p.f0 + p.cx;
    out.y = c[1] * inv * p.f1 + p.cy;
    out.z = depthFromInverse(p, c[2], inv);
    out.rhw = inv;
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

int nearDepth(const float zd[3], float to, float out[4][3]) {
    const float turned[3] = {-zd[0], -zd[1], -zd[2]};  // this side of `to`: the far side of its depths turned round
    return pastDepth(turned, -to, out);
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

void Shading::note(float x, float z, uint32_t diffuse, bool own) {
    const int i = int(std::lround(x / zoom_ + float(width_) / 2.0f));
    const int j = int(std::lround(z / zoom_ + float(height_) / 2.0f));
    if (i < 0 || j < 0 || i > width_ || j > height_ || corners_.empty() || !diffuse) return;
    uint32_t& c = corners_[size_t(j) * size_t(width_ + 1) + size_t(i)];
    if (c == diffuse || (c && !own)) return;
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
        out |= uint32_t(std::clamp(a + (b - a) * tz, 0.0f, 255.0f) + 0.5f) << k;  // lround: a library call
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

Room roomFor(uint64_t free, uint64_t models, uint64_t fine) {
    if (free >= kKeepFree) return Room::Whole;
    if (free + models >= kKeepFree) return Room::NoModels;
    if (free + models + fine >= kKeepFree) return Room::Coarse;
    return Room::None;
}

std::vector<float> cornerNormals(const gnd::Ground& g) {
    const int w = std::max(g.width, 0), h = std::max(g.height, 0);
    std::vector<float> n(size_t(w + 1) * size_t(h + 1) * 3, 0.0f);
    const float zoom = g.zoom > 0.0f ? g.zoom : 1.0f;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const gnd::Cell& c = g.cells[size_t(y) * size_t(w) + size_t(x)];
            if (c.top < 0) continue;
            const float dx = ((c.h[1] - c.h[0]) + (c.h[3] - c.h[2])) / (2.0f * zoom);
            const float dz = ((c.h[2] - c.h[0]) + (c.h[3] - c.h[1])) / (2.0f * zoom);
            for (int k = 0; k < 4; ++k) {
                float* p = &n[(size_t(y + (k >> 1)) * size_t(w + 1) + size_t(x + (k & 1))) * 3];
                p[0] += dx, p[1] -= 1.0f, p[2] += dz;
            }
        }
    for (size_t k = 0; k + 2 < n.size(); k += 3) {
        const float l = std::sqrt(n[k] * n[k] + n[k + 1] * n[k + 1] + n[k + 2] * n[k + 2]);
        if (l < 1e-6f) {
            n[k] = n[k + 2] = 0.0f, n[k + 1] = -1.0f;  // no cell round it: flat
            continue;
        }
        n[k] /= l, n[k + 1] /= l, n[k + 2] /= l;
    }
    return n;
}

void normalAt(const std::vector<float>& corners, int width, int height, float zoom, float x, float z, float out[3]) {
    out[0] = out[2] = 0.0f, out[1] = -1.0f;
    if (width <= 0 || height <= 0 || corners.size() < size_t(width + 1) * size_t(height + 1) * 3 || zoom <= 0.0f)
        return;
    const float gx = std::clamp(x / zoom + float(width) / 2.0f, 0.0f, float(width));
    const float gz = std::clamp(z / zoom + float(height) / 2.0f, 0.0f, float(height));
    const int x0 = std::min(int(gx), width - 1), z0 = std::min(int(gz), height - 1);
    const float fx = gx - float(x0), fz = gz - float(z0);
    float n[3] = {0.0f, 0.0f, 0.0f};
    for (int k = 0; k < 4; ++k) {
        const int cx = x0 + (k & 1), cz = z0 + (k >> 1);
        const float w = ((k & 1) ? fx : 1.0f - fx) * ((k >> 1) ? fz : 1.0f - fz);
        const float* c = &corners[(size_t(cz) * size_t(width + 1) + size_t(cx)) * 3];
        for (int a = 0; a < 3; ++a) n[a] += c[a] * w;
    }
    const float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (l > 1e-6f)
        for (int a = 0; a < 3; ++a) out[a] = n[a] / l;
}

float reliefLight(const float n[3], const float toSun[3], float ambient, float strength) {
    const float sl = std::sqrt(toSun[0] * toSun[0] + toSun[1] * toSun[1] + toSun[2] * toSun[2]);
    if (sl < 1e-6f) return 1.0f;
    const float L[3] = {toSun[0] / sl, toSun[1] / sl, toSun[2] / sl};
    const float lit = std::max(n[0] * L[0] + n[1] * L[1] + n[2] * L[2], 0.0f), flat = std::max(-L[1], 0.0f);
    const float k = (ambient + (1.0f - ambient) * lit) / std::max(ambient + (1.0f - ambient) * flat, 1e-3f);
    return 1.0f + (std::clamp(k, 0.6f, 1.4f) - 1.0f) * strength;
}

}  // namespace uagfx::own_ground
