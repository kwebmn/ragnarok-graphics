#include <unordered_map>

#include "core/key_pair.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(key_pair_keeps_pairs_with_the_same_hash_apart) {
    const uint64_t x = 0x1234;
    const KeyPair p{0, x};
    const KeyPair q{1, 1000003ull ^ x};
    CHECK((p.a * 1000003ull ^ p.b) == (q.a * 1000003ull ^ q.b));  // the old key was the same for both
    std::unordered_map<KeyPair, int, KeyPairHash> m;
    m[p] = 1;
    m[q] = 2;
    CHECK_EQ(m.size(), size_t(2));
    CHECK_EQ(m[p], 1);
    CHECK_EQ(m[q], 2);
}
