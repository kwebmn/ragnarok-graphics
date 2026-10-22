#include <string>

#include "core/sha256.hpp"
#include "test.hpp"

using namespace uagfx;

// 2.10: the installer checks the site's archives by their SHA-256 (FIPS 180-4's examples).
TEST(sha256_of_the_standard_messages) {
    CHECK_EQ(sha256::hexOf(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(sha256::hexOf("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK_EQ(sha256::hexOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

TEST(sha256_in_parts_is_the_same) {
    const std::string part(997, 'a');
    sha256::Hasher h;
    size_t left = 1000000;
    while (left) {
        const size_t n = left < part.size() ? left : part.size();
        h.add(part.data(), n);
        left -= n;
    }
    CHECK_EQ(h.hex(), std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}
