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

// 3.0.19-beta.53 (the owner's «берись»; R-GHP9SJ's profile of the 3D view: the game's own busiest place, 3.9 % of
// its thread). Its vertex pool grown for a model's faces (FUN_00412980 -> MSVC 6's vector<32-byte vertex>::insert,
// FUN_00414790, thiscall: the place, a count, a blank vertex; ret 0xC - its one caller): a blank copied into every
// place each frame (rep movs of 8 a vertex), then the model's kept faces write theirs whole and the culled ones'
// places are never read (FUN_004DE9F0). Ours: the end moved past them, nothing written; another place or no room -
// the game's own (win/exe_hot: its trampoline)
constexpr uint32_t kGrow = 0x414790;
constexpr size_t kGrowSize = 7;
extern const uint8_t kGrowBytes[kGrowSize];
struct GameVector {  // MSVC 6's, its addresses: the allocator's word, +4 begin, +8 end, +0xC its room's end
    uint32_t allocator = 0, begin = 0, end = 0, room = 0;
};
bool growAtEnd(GameVector& v, uint32_t at, uint32_t n);  // false: the game's own insert

// The model's projection loop (FUN_004DE9F0, 0x4DEAAF-0x4DEADD: FUN_00412CB0 called a vertex - the count at
// [ebp-0x34], the points in edi, 12 bytes each, the 4 x 3 matrix at [ebp-0x88], out kModelOut, 32 bytes each, the
// renderer [kRenderer]) - its bytes checked before it is sent to ours, which projects them in a row: each vertex's
// x y z rhw as project() makes it, the rest of it untouched
constexpr uint32_t kModelLoop = 0x4DEAAF, kModelLoopEnd = 0x4DEADD, kModelOut = 0x731150, kRenderer = 0x6EE3D8;
constexpr size_t kModelLoopSize = kModelLoopEnd - kModelLoop;
extern const uint8_t kModelLoopBytes[kModelLoopSize];
void projectMany(float f0, float f1, int cx, int cy, float nearZ, float k, const float* points, const float m[12],
                 float* out, int count);
// The way from the loop to ours, made for the place `at` it runs from: the count and the matrix from the game's frame
// to `call` (cdecl: the renderer, the points, the matrix, out, the count), then the loop's state on its way out - esi
// past the last out vertex, edi past the last point, its counter [ebp-0x14] and eax 0 - and on at kModelLoopEnd
constexpr size_t kModelStubSize = 61;
void modelStub(uint32_t at, uint32_t call, uint8_t out[kModelStubSize]);

bool sameBytes(const uint8_t* have, const uint8_t* want, size_t n);

}  // namespace uagfx::exehot
