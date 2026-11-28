#include "core/water.hpp"

#include <cmath>
#include <cstring>

namespace uagfx::water {
namespace {

// A UV on the quarter grid, 0 to 1.
bool onGrid(float v) {
    const float q = v / kStep;
    return v >= -1e-4f && v <= 1.0f + 1e-4f && std::fabs(q - std::round(q)) < 1e-3f;
}

// One cell of the ground renderer's water quads, its four corners of one colour (`diffuse`).
bool cellQuad(const uint8_t* v, size_t stride, uint32_t vertexCount, uint32_t indexCount, bool strip,
              uint32_t& diffuse) {
    if (!v || !strip || vertexCount != 4 || indexCount != 0 || stride < 32) return false;
    float uv[4][2];
    for (int i = 0; i < 4; ++i) {
        uint32_t c = 0;
        std::memcpy(&c, v + size_t(i) * stride + 16, 4);
        if (i > 0 && c != diffuse) return false;
        diffuse = c;
        std::memcpy(uv[i], v + size_t(i) * stride + 24, sizeof uv[i]);
        if (!onGrid(uv[i][0]) || !onGrid(uv[i][1])) return false;
    }
    // The cell's corners in the order the game puts them: (u, v), (u + 1/4, v), (u, v + 1/4), (u + 1/4, v + 1/4).
    const auto same = [](float a, float b) { return std::fabs(a - b) < 1e-3f; };
    return same(uv[1][0] - uv[0][0], kStep) && same(uv[1][1], uv[0][1]) && same(uv[2][0], uv[0][0]) &&
           same(uv[2][1] - uv[0][1], kStep) && same(uv[3][0], uv[1][0]) && same(uv[3][1], uv[2][1]);
}

}  // namespace

bool isWaterQuad(const uint8_t* v, size_t stride, uint32_t vertexCount, uint32_t indexCount, bool strip) {
    uint32_t diffuse = 0;
    return cellQuad(v, stride, vertexCount, indexCount, strip, diffuse) && diffuse == kDiffuse;
}

bool isLavaQuad(const uint8_t* v, size_t stride, uint32_t vertexCount, uint32_t indexCount, bool strip) {
    uint32_t diffuse = 0;
    return cellQuad(v, stride, vertexCount, indexCount, strip, diffuse) && (diffuse >> 24) == 0xFFu;
}

float gameSin(int degrees) {
    if (degrees > 360 || degrees < -360) degrees %= 360;
    const int at = degrees < 0 ? 361 + degrees : degrees;  // below 0 the table one place late: sin(1 - n degrees)
    const float angle = float(at) * 3.14159265f * (1.0f / 180.0f);  // as the table was filled, in floats
    return float(std::sin(double(angle)));
}

float waveAt(int gx, int gy, int pitch, int phase, float level, float height) {
    if (height == 0.0f) return level;
    return level + gameSin((gx + gy - 1) * pitch + phase) * height;
}

int wavePhaseOf(const int* gx, const int* gy, const float* h, int n, int pitch, float level, float height) {
    int best = 0;
    double least = 1e30;
    for (int phase = -180; phase <= 180; ++phase) {
        double e = 0.0;
        for (int k = 0; k < n; ++k) {
            const double d = double(h[k]) - double(waveAt(gx[k], gy[k], pitch, phase, level, height));
            e += d * d;
        }
        if (e < least) least = e, best = phase;
    }
    return best;
}

int phaseAfter(int phase, int speed, int ticks) {
    ticks = ticks > 0 ? ticks % 360 : 0;  // a whole turn of ticks brings it back where it was
    for (int k = 0; k < ticks; ++k) {
        phase += speed;
        if (phase > 180) phase -= 360;
    }
    return phase;
}

float quadArea(const uint8_t* vertices, size_t stride, uint32_t vertexCount, uint32_t indexCount, bool strip) {
    if (!vertices || vertexCount != 4 || indexCount != 0 || !strip) return 0.0f;
    float p[4][2];
    for (int i = 0; i < 4; ++i) std::memcpy(p[i], vertices + size_t(i) * stride, sizeof p[i]);
    const auto tri = [&](int a, int b, int c) {
        return std::fabs((p[b][0] - p[a][0]) * (p[c][1] - p[a][1]) - (p[c][0] - p[a][0]) * (p[b][1] - p[a][1])) * 0.5f;
    };
    return tri(0, 1, 2) + tri(1, 3, 2);  // a strip: 0 1 2, then 2 1 3
}

}  // namespace uagfx::water
