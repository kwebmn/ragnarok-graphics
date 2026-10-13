#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

namespace uagfx {

// Two 64-bit keys as one map key, compared whole: two different pairs never share an entry (1B review M6 - the
// sampler-pair cache used k0 * 1000003 ^ k1 as the key itself, so a collision bound the wrong samplers).
struct KeyPair {
    uint64_t a = 0;
    uint64_t b = 0;
    bool operator==(const KeyPair& o) const { return a == o.a && b == o.b; }
};

struct KeyPairHash {
    size_t operator()(const KeyPair& k) const { return std::hash<uint64_t>()(k.a * 1000003ull ^ k.b); }
};

}  // namespace uagfx
