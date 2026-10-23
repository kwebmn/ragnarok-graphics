// 3.0.3-beta.2: the character's name as uaRO.exe keeps it - XOR a key of 64 bytes (core/char_name)
#include <cstring>
#include <string>

#include "core/char_name.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// uaRO.exe's key at 0x6e30a0 (the pointer at 0x6e2f78), its 64 bytes: fourteen Korean syllables twice in EUC-KR,
// four zeros and the next string's first four
const uint8_t kKey[64] = {
    0xb0, 0xa1, 0xb3, 0xaa, 0xb4, 0xd9, 0xb6, 0xf3, 0xb8, 0xb6, 0xb9, 0xd9,
    0xbb, 0xe7, 0xbe, 0xc6, 0xc0, 0xda, 0xc2, 0xf7, 0xc4, 0xab, 0xc5, 0xb8,
    0xc6, 0xc4, 0xc7, 0xcf, 0xb0, 0xa1, 0xb3, 0xaa, 0xb4, 0xd9, 0xb6, 0xf3,
    0xb8, 0xb6, 0xb9, 0xd9, 0xbb, 0xe7, 0xbe, 0xc6, 0xc0, 0xda, 0xc2, 0xf7,
    0xc4, 0xab, 0xc5, 0xb8, 0xc6, 0xc4, 0xc7, 0xcf, 0x00, 0x00, 0x00, 0x00,
    0x65, 0x66, 0x66, 0x65,
};

// What SetCharName (0x642550) leaves at CSession +0x618 for a name of `n` bytes
void store(const char* name, size_t n, uint8_t out[64]) {
    uint8_t plain[64] = {};
    std::memcpy(plain, name, n);
    for (int i = 0; i < 64; ++i) out[i] = uint8_t(plain[i] ^ kKey[i]);
}

void store(const char* name, uint8_t out[64]) { store(name, std::strlen(name), out); }

}  // namespace

TEST(char_name_comes_back_with_the_games_key) {
    uint8_t s[64];
    store("Lena", s);
    CHECK_EQ(charname::decode(s, kKey), std::string("Lena"));
    store("Big Bob", s);  // a space in it
    CHECK_EQ(charname::decode(s, kKey), std::string("Big Bob"));
    store("\xcf\xf0\xe8\xf1\xf2", s);  // in the game's code page (windows-1251 here), as it is
    CHECK_EQ(charname::decode(s, kKey), std::string("\xcf\xf0\xe8\xf1\xf2"));
    store("abcdefghijklmnopqrstuvw", s);  // 23: the longest the server has
    CHECK_EQ(charname::decode(s, kKey), std::string("abcdefghijklmnopqrstuvw"));
}

TEST(char_name_none_where_no_character_was_chosen) {
    const uint8_t zero[64] = {};
    CHECK(charname::decode(zero, kKey).empty());  // never set: the key itself would come out
    uint8_t s[64];
    store("abcdefghijklmnopqrstuvwxyz", s);  // no end in a name's 24 bytes: no name
    CHECK(charname::decode(s, kKey).empty());
    store("a\x01z", s);  // a control character: no name
    CHECK(charname::decode(s, kKey).empty());
    store("", 0, s);
    CHECK(charname::decode(s, kKey).empty());
}
