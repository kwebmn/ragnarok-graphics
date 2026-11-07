#include "core/model_fade.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::modelfade {

Focus focusOf(const camera::Projection& p, const float view[12], const float at[3]) {
    Focus f;
    float c[3];
    for (int j = 0; j < 3; ++j) c[j] = at[0] * view[j] + at[1] * view[3 + j] + at[2] * view[6 + j] + view[9 + j];
    if (!(c[2] > p.nearZ) || !std::isfinite(c[2])) return f;
    f.x = c[0] / c[2] * p.f0 + p.cx;
    f.y = c[1] / c[2] * p.f1 + p.cy;
    f.z = c[2];
    f.h = std::fabs(p.f1) * kBodyUnits / c[2];
    f.p = p;
    std::memcpy(f.view, view, sizeof f.view);
    float cam[3];  // the camera's place: w = M (c - t) at c = 0
    for (int i = 0; i < 3; ++i)
        cam[i] = -(view[3 * i] * view[9] + view[3 * i + 1] * view[10] + view[3 * i + 2] * view[11]);
    f.camX = cam[0];
    f.camZ = cam[2];
    f.ground = std::hypot(at[0] - cam[0], at[2] - cam[2]);
    f.known = std::isfinite(f.x) && std::isfinite(f.y) && f.h > 0 && std::isfinite(f.ground);
    return f;
}

void holeOf(const Focus& f, float out[8]) {
    for (int i = 0; i < 8; ++i) out[i] = 0.0f;
    const float up = std::hypot(f.view[4], f.view[5]);  // the world's y in the view: its row (0, y, z)
    if (!f.known || !(f.z > 0) || !(up > 1e-6f) || !(f.p.f1 != 0)) return;
    out[0] = f.x;
    out[1] = f.y;
    out[2] = f.h;
    out[3] = f.z;
    out[4] = f.ground;
    out[5] = f.view[4] / up;
    out[6] = f.view[5] / up;
    out[7] = std::fabs(f.p.f1);
}

float holeAt(const float hole[8], float gx, float gy, float z, int width, int height) {
    if (hole[3] <= 0.0f || z >= hole[3] - kNearer) return 1.0f;
    const float e =
        std::hypot((gx - hole[0]) / (hole[2] * 0.8f), (gy - (hole[1] - 0.55f * hole[2])) / (hole[2] * 0.95f));
    if (e >= 1.0f) return 1.0f;
    const float p[3] = {(gx - std::floor(width * 0.5f)) * z / hole[7], (gy - std::floor(height * 0.5f)) * z / hole[7],
                        z};
    const float up = p[1] * hole[5] + p[2] * hole[6];
    if (std::sqrt(std::max(p[0] * p[0] + p[1] * p[1] + p[2] * p[2] - up * up, 0.0f)) >= hole[4] - kNearer) return 1.0f;
    const float t = std::clamp((e - 0.55f) / 0.45f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace uagfx::modelfade
