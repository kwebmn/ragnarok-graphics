#include "core/exe_hot.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::exehot {

// push ebp; mov ebp, esp; mov eax, [ebp+0xC]; mov edx, [ebp+8]; push esi; mov esi, [ebp+0x10]; fld [eax+0x20]
const uint8_t kProjectBytes[kProjectSize] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x8B, 0x55,
                                             0x08, 0x56, 0x8B, 0x75, 0x10, 0xD9, 0x40, 0x20};
// push ebp; mov ebp, esp; push ecx; mov eax, [ebp+0xC]; mov ecx, [ebp+8]; sub eax, ecx; mov edx, eax; and edx, ..
const uint8_t kSortBytes[kSortSize] = {0x55, 0x8B, 0xEC, 0x51, 0x8B, 0x45, 0x0C, 0x8B,
                                       0x4D, 0x08, 0x2B, 0xC1, 0x8B, 0xD0, 0x83, 0xE2};

float depthScale(float nearZ, float farZ) { return float(double(farZ) / (double(farZ) - double(nearZ))); }

void project(float f0, float f1, int cx, int cy, float nearZ, float k, const float p[3], const float m[12],
             float out[4]) {
    const double x = p[0], y = p[1], z = p[2];
    const float rhw = float(1.0 / (m[2] * x + m[5] * y + m[8] * z + m[11]));
    out[3] = rhw;
    out[0] = float((x * m[0] + m[3] * y + m[6] * z + m[9]) * rhw * f0 + cx);
    out[1] = float((m[1] * x + m[4] * y + m[7] * z + m[10]) * f1 * rhw + cy);
    out[2] = float(double(k) * (1.0 - double(nearZ) * rhw));  // (1 / rhw - near) k rhw
}

void sortFaces(std::vector<FaceKey>& keys) {
    std::sort(keys.begin(), keys.end(), [](const FaceKey& a, const FaceKey& b) {
        return a.key != b.key ? a.key < b.key : a.order < b.order;
    });
}

bool sameBytes(const uint8_t* have, const uint8_t* want, size_t n) {
    return have && want && std::memcmp(have, want, n) == 0;
}

}  // namespace uagfx::exehot
