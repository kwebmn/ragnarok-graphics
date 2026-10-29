#include "core/char_status.hpp"

#include <cstring>

namespace uagfx::charstatus {
namespace {

int32_t at(const uint8_t* block, uint32_t from, uint32_t address) {
    int32_t v = 0;
    std::memcpy(&v, block + (address - from), 4);
    return v;
}

// The key's nibbles moved by the shifts of step i - the same both ways
uint32_t spread(uint32_t key, int i) {
    const int s28 = i % 28 + 1, s24 = i % 24 + 1, s20 = i % 20 + 1, s16 = i % 16 + 1;
    return ((key & 0xF0000000u) >> s28) + ((key & 0xF000u) << s16) + ((key & 0xF0000u) >> s16) +
           ((key & 0xF00u) << s20) + ((key & 0xF0u) << s24) + ((key & 0xF00000u) >> s20) + ((key & 0xFu) << s28) +
           ((key & 0xF000000u) >> s24);
}

// (key % 6) * 2 + 2 steps, the remainder signed: none for most negative keys
int steps(uint32_t key) { return (int32_t(key) % 6) * 2 + 2; }

// One of the masked numbers: its place's value with the key in use
int unmasked(const uint8_t* block, uint32_t values, uint32_t index) {
    const int32_t i = at(block, kMaskedAt, index), k = at(block, kMaskedAt, kKeyIndex);
    if (i < 0 || i >= kPlaces || k < 0 || k >= kPlaces) return 0;
    return int32_t(unmask(uint32_t(at(block, kMaskedAt, values + 4 * uint32_t(i))),
                          uint32_t(at(block, kMaskedAt, kKeys + 4 * uint32_t(k)))));
}

}  // namespace

const Probe kProbes[] = {
    {0x58346B, 6, {0x89, 0x15, 0x90, 0x1F, 0x77, 0x00}},        // mov [0x771f90], edx: the base exp
    {0x582B62, 6, {0x89, 0x0D, 0x94, 0x1F, 0x77, 0x00}},        // the base level
    {0x5834D7, 6, {0x89, 0x15, 0x9C, 0x1F, 0x77, 0x00}},        // the base's next
    {0x583213, 6, {0x89, 0x0D, 0xA0, 0x1F, 0x77, 0x00}},        // the job level
    {0x583486, 5, {0xA3, 0x44, 0x20, 0x77, 0x00}},              // the job exp
    {0x5834F1, 5, {0xA3, 0x54, 0x20, 0x77, 0x00}},              // the job's next
    {0x582E45, 6, {0x89, 0x0D, 0x48, 0x20, 0x77, 0x00}},        // the weight
    {0x582FCA, 6, {0x89, 0x0D, 0x50, 0x20, 0x77, 0x00}},        // its most
    {0x5828EE, 6, {0x8B, 0x15, 0xD4, 0x2B, 0x77, 0x00}},        // mov edx, [0x772bd4]: the key's index
    {0x5828F7, 7, {0x8B, 0x04, 0x95, 0xA0, 0x2C, 0x77, 0x00}},  // mov eax, [edx*4+0x772ca0]: the keys
    {0x58290A, 6, {0x8B, 0x0D, 0xCC, 0x2B, 0x77, 0x00}},        // HP's index
    {0x58291D, 7, {0x89, 0x04, 0x8D, 0x40, 0x2C, 0x77, 0x00}},  // mov [ecx*4+0x772c40], eax: HP
    {0x582A51, 6, {0x8B, 0x0D, 0xD8, 0x2B, 0x77, 0x00}},        // HP's most
    {0x582A64, 7, {0x89, 0x04, 0x8D, 0xE0, 0x2B, 0x77, 0x00}},
    {0x582AA4, 6, {0x8B, 0x0D, 0xD0, 0x2B, 0x77, 0x00}},        // SP
    {0x582AB7, 7, {0x89, 0x04, 0x8D, 0x70, 0x2C, 0x77, 0x00}},
    {0x582AF6, 6, {0x8B, 0x0D, 0xDC, 0x2B, 0x77, 0x00}},        // SP's most
    {0x582B09, 7, {0x89, 0x04, 0x8D, 0x10, 0x2C, 0x77, 0x00}},
};
const size_t kProbeCount = sizeof(kProbes) / sizeof(kProbes[0]);

uint32_t fnv1a(const uint8_t* bytes, size_t size) {
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < size; ++i) h = (h ^ bytes[i]) * 0x01000193u;
    return h;
}

uint32_t unmask(uint32_t stored, uint32_t key) {
    for (int i = steps(key) - 1; i >= 0; --i) {
        stored ^= spread(key, i);
        stored = (stored << 8) | (stored >> 24);
    }
    return stored;
}

uint32_t mask(uint32_t value, uint32_t key) {
    for (int i = 0; i < steps(key); ++i) value = spread(key, i) ^ ((value >> 8) | (value << 24));
    return value;
}

Status fromMemory(const uint8_t* plain, const uint8_t* masked) {
    Status s;
    s.baseExp = at(plain, kPlainAt, kBaseExp);
    s.baseLevel = at(plain, kPlainAt, kBaseLevel);
    s.baseNext = at(plain, kPlainAt, kBaseNext);
    s.jobLevel = at(plain, kPlainAt, kJobLevel);
    s.jobExp = at(plain, kPlainAt, kJobExp);
    s.jobNext = at(plain, kPlainAt, kJobNext);
    s.weight = at(plain, kPlainAt, kWeight);
    s.weightMost = at(plain, kPlainAt, kWeightMost);
    s.zeny = at(plain, kPlainAt, kZeny);  // 3.0.11-beta.36
    s.statusPoints = at(plain, kPlainAt, kStatusPoints);
    s.skillPoints = at(plain, kPlainAt, kSkillPoints);
    s.hp = unmasked(masked, kHp, kHpIndex);
    s.hpMost = unmasked(masked, kHpMost, kHpMostIndex);
    s.sp = unmasked(masked, kSp, kSpIndex);
    s.spMost = unmasked(masked, kSpMost, kSpMostIndex);
    return s;
}

}  // namespace uagfx::charstatus
