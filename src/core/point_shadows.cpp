#include "core/point_shadows.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace uagfx::point_shadows {

Face face(int f) {
    Face out{};
    const int a = std::clamp(f, 0, 5) / 2;
    const float s = (f % 2 == 0) ? 1.0f : -1.0f;
    out.fwd[a] = s;
    out.right[(a + 1) % 3] = 1.0f;
    out.up[(a + 2) % 3] = 1.0f;
    return out;
}

bool boxOnFace(const float lo[3], const float hi[3], int f) {
    const int a = std::clamp(f, 0, 5) / 2;
    const bool plus = f % 2 == 0;
    const float ahead = plus ? hi[a] : -lo[a];  // the furthest along the face's way
    if (ahead <= 0.0f) return false;
    for (int k = 1; k <= 2; ++k) {
        const int b = (a + k) % 3;
        if (lo[b] > ahead || hi[b] < -ahead) return false;  // wholly beside the pyramid
    }
    return true;
}

std::vector<int> pick(const std::vector<Candidate>& lights, float nearZ, int most) {
    std::vector<int> order(lights.size());
    std::iota(order.begin(), order.end(), 0);
    const auto score = [&](int i) {
        const Candidate& c = lights[size_t(i)];
        return c.strength * c.reach / std::max(c.view[2], std::max(nearZ, 1.0f));
    };
    const auto none = [&](int i) {
        const Candidate& c = lights[size_t(i)];
        return !(c.strength > 0.0f && c.reach > 0.0f);
    };
    order.erase(std::remove_if(order.begin(), order.end(), none), order.end());
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return score(a) > score(b); });
    if (int(order.size()) > most) order.resize(size_t(std::max(most, 0)));
    return order;
}

bool standingOf(const float world[4][3], const float screen[4][2], const float uv[4][2], Standing& out) {
    int order[4] = {0, 1, 2, 3};  // by the screen's y, the top first
    std::sort(order, order + 4, [&](int a, int b) { return screen[a][1] < screen[b][1]; });
    int tl = order[0], tr = order[1], bl = order[2], br = order[3];
    if (screen[tl][0] > screen[tr][0]) std::swap(tl, tr);
    if (screen[bl][0] > screen[br][0]) std::swap(bl, br);
    const int at[4] = {bl, br, tl, tr};
    for (int k = 0; k < 3; ++k) {
        out.bottom[k] = 0.5f * (world[bl][k] + world[br][k]);
        out.top[k] = 0.5f * (world[tl][k] + world[tr][k]);
    }
    float w2 = 0.0f, h2 = 0.0f;
    for (int k = 0; k < 3; ++k) {
        w2 += (world[br][k] - world[bl][k]) * (world[br][k] - world[bl][k]);
        h2 += (out.top[k] - out.bottom[k]) * (out.top[k] - out.bottom[k]);
    }
    out.halfWidth = 0.5f * std::sqrt(w2);
    for (int c = 0; c < 4; ++c) {
        out.uv[c][0] = uv[at[c]][0];
        out.uv[c][1] = uv[at[c]][1];
    }
    return out.halfWidth > 0.05f && h2 > 0.01f;
}

void facing(const Standing& s, const float light[3], float push, float out[4][3]) {
    float up[3], d[3];
    float ul = 0.0f;
    for (int k = 0; k < 3; ++k) {
        up[k] = s.top[k] - s.bottom[k];
        ul += up[k] * up[k];
    }
    ul = std::sqrt(std::max(ul, 1e-12f));
    for (int k = 0; k < 3; ++k) up[k] /= ul;
    float along = 0.0f;  // the way from the light, less its upright part
    for (int k = 0; k < 3; ++k) {
        d[k] = s.bottom[k] - light[k];
        along += d[k] * up[k];
    }
    float dl = 0.0f;
    for (int k = 0; k < 3; ++k) {
        d[k] -= along * up[k];
        dl += d[k] * d[k];
    }
    dl = std::sqrt(dl);
    if (dl < 1e-4f) {  // the light right over it: any way across
        d[0] = 1.0f;
        d[1] = d[2] = 0.0f;
        dl = 1.0f;
    }
    for (int k = 0; k < 3; ++k) d[k] /= dl;
    const float side[3] = {up[1] * d[2] - up[2] * d[1], up[2] * d[0] - up[0] * d[2], up[0] * d[1] - up[1] * d[0]};
    const float back = s.halfWidth + push;
    for (int k = 0; k < 3; ++k) {
        const float b = s.bottom[k] + d[k] * back, t = s.top[k] + d[k] * back;
        out[0][k] = b - side[k] * s.halfWidth;
        out[1][k] = b + side[k] * s.halfWidth;
        out[2][k] = t - side[k] * s.halfWidth;
        out[3][k] = t + side[k] * s.halfWidth;
    }
}

}  // namespace uagfx::point_shadows
