#pragma once
#include <cstddef>
#include <cstdint>

// 3.0.5-beta.11 (R-455GWU: a third of the 32-bit game's thread in one std::unordered_map look-up on «Високі»):
// std::hash of a 64-bit number in a 32-bit build is its low half alone (libstdc++'s size_t is 32 bits there) - a key
// of two halves (a cell's x and z, a strip's group and texture) put every key of one z or one texture into one
// bucket, chains of hundreds. This mixes the whole number into a size_t (murmur3's finalizer).
namespace uagfx {

struct Hash64 {
    size_t operator()(uint64_t k) const {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdull;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ull;
        k ^= k >> 33;
        return size_t(k);
    }
    size_t operator()(int64_t k) const { return (*this)(uint64_t(k)); }
};

}  // namespace uagfx
