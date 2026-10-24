// 3.0.3-beta.5: the character's numbers as uaRO.exe keeps them - plain, and HP/SP masked (core/char_status)
#include <cstring>

#include "core/char_status.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

void put(uint8_t* block, uint32_t from, uint32_t address, uint32_t v) { std::memcpy(block + (address - from), &v, 4); }

// What the game leaves for a masked number: its value masked in the place `index`, with the key in use
void putMasked(uint8_t* block, uint32_t values, uint32_t indexAt, int index, uint32_t v) {
    uint32_t keyIndex = 0, key = 0;
    std::memcpy(&keyIndex, block + (charstatus::kKeyIndex - charstatus::kMaskedAt), 4);
    std::memcpy(&key, block + (charstatus::kKeys + 4 * keyIndex - charstatus::kMaskedAt), 4);
    put(block, charstatus::kMaskedAt, indexAt, uint32_t(index));
    put(block, charstatus::kMaskedAt, values + 4 * uint32_t(index), charstatus::mask(v, key));
}

}  // namespace

TEST(char_status_unmasks_as_the_games_code) {
    // worked out from the instructions at 0x63a7d0
    CHECK_EQ(charstatus::unmask(0x12345678u, 0x0BADF00Du), 0xBBCCFEA7u);
    CHECK_EQ(charstatus::unmask(0x00000000u, 0x7FFFFFFFu), 0x4E9C366Cu);
    CHECK_EQ(charstatus::unmask(0xDEADBEEFu, 0x00000005u), 0x63F34195u);
    CHECK_EQ(charstatus::unmask(0x000004D2u, 0x1234ABCDu), 0x12A7673Bu);
    CHECK_EQ(charstatus::unmask(0xCAFEBABEu, 0xFFFFFFF9u), 0xCAFEBABEu);  // -7: a remainder of -1, no step
    CHECK_EQ(charstatus::unmask(0x13572468u, 0xFFFFFFFAu), 0x9AFFA7D4u);  // -6: a remainder of 0, two steps
}

TEST(char_status_masked_comes_back) {
    const uint32_t keys[] = {0u, 1u, 5u, 6u, 11u, 0x7FFFu, 0x0BADF00Du, 0x80000000u, 0xFFFFFFFFu, 0xFFFFFFF4u};
    const uint32_t values[] = {0u, 1u, 85u, 1234u, 99999u, 0x7FFFFFFFu, 0xFFFFFFFFu};
    for (uint32_t k : keys)
        for (uint32_t v : values) CHECK_EQ(charstatus::unmask(charstatus::mask(v, k), k), v);
}

TEST(char_status_reads_the_blocks) {
    uint8_t plain[charstatus::kPlainSize] = {}, masked[charstatus::kMaskedSize] = {};
    put(plain, charstatus::kPlainAt, charstatus::kBaseExp, 452);
    put(plain, charstatus::kPlainAt, charstatus::kBaseLevel, 87);
    put(plain, charstatus::kPlainAt, charstatus::kBaseNext, 1000);
    put(plain, charstatus::kPlainAt, charstatus::kJobLevel, 50);
    put(plain, charstatus::kPlainAt, charstatus::kJobExp, 7);
    put(plain, charstatus::kPlainAt, charstatus::kJobNext, 70);
    put(plain, charstatus::kPlainAt, charstatus::kWeight, 1200);
    put(plain, charstatus::kPlainAt, charstatus::kWeightMost, 2400);
    put(masked, charstatus::kMaskedAt, charstatus::kKeyIndex, 4);
    for (int i = 0; i < charstatus::kPlaces; ++i)
        put(masked, charstatus::kMaskedAt, charstatus::kKeys + 4 * uint32_t(i), 0x1234567u * uint32_t(i + 3));
    putMasked(masked, charstatus::kHp, charstatus::kHpIndex, 3, 850);
    putMasked(masked, charstatus::kHpMost, charstatus::kHpMostIndex, 11, 1000);
    putMasked(masked, charstatus::kSp, charstatus::kSpIndex, 0, 40);
    putMasked(masked, charstatus::kSpMost, charstatus::kSpMostIndex, 7, 100);
    const charstatus::Status s = charstatus::fromMemory(plain, masked);
    CHECK_EQ(s.baseLevel, 87);
    CHECK_EQ(s.baseExp, 452);
    CHECK_EQ(s.baseNext, 1000);
    CHECK_EQ(s.jobLevel, 50);
    CHECK_EQ(s.jobExp, 7);
    CHECK_EQ(s.jobNext, 70);
    CHECK_EQ(s.weight, 1200);
    CHECK_EQ(s.weightMost, 2400);
    CHECK_EQ(s.hp, 850);
    CHECK_EQ(s.hpMost, 1000);
    CHECK_EQ(s.sp, 40);
    CHECK_EQ(s.spMost, 100);
}

TEST(char_status_an_index_out_of_the_places_reads_nothing) {
    uint8_t plain[charstatus::kPlainSize] = {}, masked[charstatus::kMaskedSize] = {};
    putMasked(masked, charstatus::kHp, charstatus::kHpIndex, 2, 850);
    put(masked, charstatus::kMaskedAt, charstatus::kHpIndex, 12);
    CHECK_EQ(charstatus::fromMemory(plain, masked).hp, 0);
    put(masked, charstatus::kMaskedAt, charstatus::kHpIndex, 2);
    put(masked, charstatus::kMaskedAt, charstatus::kKeyIndex, uint32_t(-1));
    CHECK_EQ(charstatus::fromMemory(plain, masked).hp, 0);
}

TEST(char_status_probes_fit_their_instructions) {
    CHECK(charstatus::kProbeCount == 18);
    for (size_t i = 0; i < charstatus::kProbeCount; ++i) {
        CHECK(charstatus::kProbes[i].size >= 5 && charstatus::kProbes[i].size <= 7);
        CHECK(charstatus::kProbes[i].at >= 0x582700 && charstatus::kProbes[i].at < 0x583540);  // the two handlers
    }
    const uint8_t text[] = {'a', 'b', 'c'};
    CHECK_EQ(charstatus::fnv1a(text, 3), 0x1A47E90Bu);
}
