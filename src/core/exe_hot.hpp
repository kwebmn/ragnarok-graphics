#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uagfx::exehot {

// 3.0.19-beta.49 (the owner's «давай» - uaRO.exe's hot parts rewritten; its __ftol is fisttp since 2.5.20, core/
// ftol_exe): two more of the game's thread's busiest, sent to ours (win/exe_hot) when their first bytes are this
// build's.

// Its vertex projection (FUN_00412CB0, thiscall on the renderer's view - f0 +0, f1 +4, the screen's centre +0x2C
// and +0x30 as integers; a point, the 4 x 3 view matrix, out x y z rhw; 102 calls, ~3.4 % of the thread): three
// divisions a vertex, far / (far - near) one of them each time. Ours: one, the depth k (1 - near rhw) - the same
// numbers within a float's last digit (its near 0x695748, its far 0x695744: 10 and 1500)
constexpr uint32_t kProject = 0x412CB0, kNear = 0x695748, kFar = 0x695744;
constexpr size_t kProjectSize = 16;
extern const uint8_t kProjectBytes[kProjectSize];
float depthScale(float nearZ, float farZ);  // k: far / (far - near)
void project(float f0, float f1, int cx, int cy, float nearZ, float k, const float p[3], const float m[12],
             float out[4]);

// Its faces' sort before they are drawn (FUN_00416850, MSVC 6's std::sort's loop on the renderer's list of faces
// +0x16C: by a face's +0x10 - its texture - then +0x14, unsigned; ~3.7 %): every comparison through two faces'
// pointers. Ours: the keys read once, in a row, then sorted - alike ones as they came
constexpr uint32_t kSort = 0x416850, kFaceKey1 = 0x10, kFaceKey2 = 0x14;
constexpr size_t kSortSize = 16;
extern const uint8_t kSortBytes[kSortSize];
struct FaceKey {
    uint64_t key = 0;
    uint32_t order = 0;
    uint32_t face = 0;
};
constexpr uint64_t faceKey(uint32_t first, uint32_t second) { return uint64_t(first) << 32 | second; }
void sortFaces(std::vector<FaceKey>& keys);

bool sameBytes(const uint8_t* have, const uint8_t* want, size_t n);

}  // namespace uagfx::exehot
