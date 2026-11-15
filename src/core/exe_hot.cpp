#include "core/exe_hot.hpp"

#include <algorithm>
#include <cstring>
#include <initializer_list>

namespace uagfx::exehot {

// push ebp; mov ebp, esp; mov eax, [ebp+0xC]; mov edx, [ebp+8]; push esi; mov esi, [ebp+0x10]; fld [eax+0x20]
const uint8_t kProjectBytes[kProjectSize] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x8B, 0x55,
                                             0x08, 0x56, 0x8B, 0x75, 0x10, 0xD9, 0x40, 0x20};
// push ebp; mov ebp, esp; push ecx; mov eax, [ebp+0xC]; mov ecx, [ebp+8]; sub eax, ecx; mov edx, eax; and edx, ..
const uint8_t kSortBytes[kSortSize] = {0x55, 0x8B, 0xEC, 0x51, 0x8B, 0x45, 0x0C, 0x8B,
                                       0x4D, 0x08, 0x2B, 0xC1, 0x8B, 0xD0, 0x83, 0xE2};

// push ebp; mov ebp, esp; sub esp, 0xC; push ebx
const uint8_t kGrowBytes[kGrowSize] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C, 0x53};
// mov ecx, [ebp-0x34]; mov esi, 0x731150; mov [ebp-0x14], ecx; mov ecx, [0x6EE3D8]; lea edx, [ebp-0x88]; push esi;
// push edx; push edi; call 0x412CB0; mov eax, [ebp-0x14]; add esi, 0x20; add edi, 0xC; dec eax; mov [ebp-0x14], eax;
// jne 0x4DEABA
const uint8_t kModelLoopBytes[kModelLoopSize] = {
    0x8B, 0x4D, 0xCC, 0xBE, 0x50, 0x11, 0x73, 0x00, 0x89, 0x4D, 0xEC, 0x8B, 0x0D, 0xD8, 0xE3, 0x6E,
    0x00, 0x8D, 0x95, 0x78, 0xFF, 0xFF, 0xFF, 0x56, 0x52, 0x57, 0xE8, 0xE2, 0x41, 0xF3, 0xFF, 0x8B,
    0x45, 0xEC, 0x83, 0xC6, 0x20, 0x83, 0xC7, 0x0C, 0x48, 0x89, 0x45, 0xEC, 0x75, 0xDD};

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

void projectMany(float f0, float f1, int cx, int cy, float nearZ, float k, const float* points, const float m[12],
                 float* out, int count) {
    for (int i = 0; i < count; ++i, points += 3, out += 8) project(f0, f1, cx, cy, nearZ, k, points, m, out);
}

void modelStub(uint32_t at, uint32_t call, uint8_t out[kModelStubSize]) {
    uint8_t* b = out;
    const auto put = [&b](std::initializer_list<uint8_t> bytes) {
        for (const uint8_t v : bytes) *b++ = v;
    };
    const auto word = [&b](uint32_t v) {
        std::memcpy(b, &v, 4);
        b += 4;
    };
    put({0x8B, 0x45, 0xCC});                    // mov eax, [ebp-0x34]: the count
    put({0x8D, 0x95, 0x78, 0xFF, 0xFF, 0xFF});  // lea edx, [ebp-0x88]: the matrix
    put({0x50, 0x68});                          // push eax; push out
    word(kModelOut);
    put({0x52, 0x57, 0xFF, 0x35});  // push edx; push edi (the points); push [the renderer]
    word(kRenderer);
    put({0xE8});  // call ours
    word(call - (at + 28));
    put({0x83, 0xC4, 0x14});  // add esp, 20
    put({0x8B, 0x45, 0xCC});  // mov eax, [ebp-0x34]
    put({0x8D, 0x0C, 0x40});  // lea ecx, [eax + eax * 2]
    put({0x8D, 0x3C, 0x8F});  // lea edi, [edi + ecx * 4]: past the last point
    put({0x8B, 0xF0, 0xC1, 0xE6, 0x05, 0x81, 0xC6});  // mov esi, eax; shl esi, 5; add esi, out
    word(kModelOut);
    put({0x33, 0xC0, 0x89, 0x45, 0xEC});  // xor eax, eax; mov [ebp-0x14], eax
    put({0xE9});                          // jmp past the loop
    word(kModelLoopEnd - (at + uint32_t(kModelStubSize)));
}

bool growAtEnd(GameVector& v, uint32_t at, uint32_t n) {
    if (at != v.end || v.room < v.end || (v.room - v.end) / 32 < n) return false;
    v.end += n * 32;
    return true;
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
