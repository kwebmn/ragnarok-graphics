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

bool faceOf(const float d[3], int& f, float& u, float& v) {
    const float ax = std::fabs(d[0]), ay = std::fabs(d[1]), az = std::fabs(d[2]);
    int a = 0;
    if (ay > ax && ay >= az) a = 1;
    else if (az > ax && az > ay) a = 2;
    const float m = std::fabs(d[a]);
    if (!(m > 0.0f)) return false;
    f = a * 2 + (d[a] >= 0.0f ? 0 : 1);
    u = d[(a + 1) % 3] / m;
    v = d[(a + 2) % 3] / m;
    return true;
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

}  // namespace uagfx::point_shadows
