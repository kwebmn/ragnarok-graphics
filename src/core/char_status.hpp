#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::charstatus {

// 3.0.3-beta.5 (the user: the window's title with the level, the percent and more): where uaRO.exe keeps its
// character's numbers - CSession's fields (core/char_name kSession), put there by the handlers of ZC_PAR_CHANGE
// (0x582700) and ZC_LONGPAR_CHANGE (0x5833d0). Read as two blocks: the plain numbers and the masked ones.
constexpr uint32_t kPlainAt = 0x771F90;
constexpr size_t kPlainSize = 0xC8;  // to the job's next at 0x772054
constexpr uint32_t kBaseExp = 0x771F90, kBaseLevel = 0x771F94, kBaseNext = 0x771F9C, kJobLevel = 0x771FA0;
constexpr uint32_t kJobExp = 0x772044, kWeight = 0x772048, kWeightMost = 0x772050, kJobNext = 0x772054;
// HP, SP and their most: each in one of 12 places (chosen anew at every change), masked with one of 12 keys - the
// value at kX + 4 * [kXIndex], its key at kKeys + 4 * [kKeyIndex]; FUN_0063a670 masks, FUN_0063a7d0 unmasks
constexpr uint32_t kMaskedAt = 0x772BCC;
constexpr size_t kMaskedSize = 0x104;  // to the last key at 0x772CCC
constexpr uint32_t kHpIndex = 0x772BCC, kSpIndex = 0x772BD0, kKeyIndex = 0x772BD4, kHpMostIndex = 0x772BD8,
                   kSpMostIndex = 0x772BDC;
constexpr uint32_t kHpMost = 0x772BE0, kSpMost = 0x772C10, kHp = 0x772C40, kSp = 0x772C70, kKeys = 0x772CA0;
constexpr int kPlaces = 12;

// The instructions of this build that put them there: each as here, and the unmasking's code (kUnmaskSize bytes at
// kUnmaskAt, FNV-1a kUnmaskFnv) too, or nothing is read
struct Probe {
    uint32_t at;
    uint8_t size;
    uint8_t bytes[7];
};
extern const Probe kProbes[];
extern const size_t kProbeCount;
constexpr uint32_t kUnmaskAt = 0x63A7D0;
constexpr size_t kUnmaskSize = 0x12F;
constexpr uint32_t kUnmaskFnv = 0x415DAA42;
uint32_t fnv1a(const uint8_t* bytes, size_t size);

uint32_t unmask(uint32_t stored, uint32_t key);  // as FUN_0063a7d0
uint32_t mask(uint32_t value, uint32_t key);     // as FUN_0063a670

// The numbers as the game has them; 0 for one it has not had (the weight in whole units: the game keeps a tenth of
// the server's)
struct Status {
    int baseLevel = 0, jobLevel = 0;
    int baseExp = 0, baseNext = 0, jobExp = 0, jobNext = 0;  // next 0: the last level
    int hp = 0, hpMost = 0, sp = 0, spMost = 0;
    int weight = 0, weightMost = 0;
};
// From the blocks read at kPlainAt (kPlainSize bytes) and kMaskedAt (kMaskedSize); an index out of the 12 places
// leaves its number 0
Status fromMemory(const uint8_t* plain, const uint8_t* masked);

}  // namespace uagfx::charstatus
