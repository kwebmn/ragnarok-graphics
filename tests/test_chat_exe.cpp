#include <climits>
#include <cstdint>

#include "core/chat_exe.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// Where the check's bytes at chat_exe::kCheck go on with eax = count: kKeep, kReset (fell through to it), or 0 for
// an instruction that is none of the five they are made of
uint32_t after(const uint8_t* code, int32_t count) {
    const uint32_t a = uint32_t(count);
    bool zf = false, sf = false, of = false, cf = false;
    for (size_t at = 0; at < chat_exe::kSize;) {
        const uint8_t* c = code + at;
        if (c[0] == 0x85 && c[1] == 0xC0) {  // test eax, eax
            zf = a == 0;
            sf = count < 0;
            of = cf = false;
            at += 2;
        } else if (c[0] == 0x83 && c[1] == 0xF8 && at + 3 <= chat_exe::kSize) {  // cmp eax, imm8
            const uint32_t b = uint32_t(int32_t(int8_t(c[2]))), r = a - b;
            zf = r == 0;
            sf = r >> 31;
            of = ((a ^ b) & (a ^ r)) >> 31;
            cf = a < b;
            at += 3;
        } else if ((c[0] == 0x7D || c[0] == 0x7E || c[0] == 0x76) && at + 2 <= chat_exe::kSize) {  // jge, jle, jbe
            const bool jump = c[0] == 0x7D ? sf == of : c[0] == 0x7E ? zf || sf != of : cf || zf;
            at += 2;
            if (jump) return chat_exe::kCheck + uint32_t(at) + uint32_t(int32_t(int8_t(c[1])));
        } else if (c[0] == 0x90) {  // nop
            at += 1;
        } else {
            return 0;
        }
    }
    return chat_exe::kCheck + chat_exe::kSize;
}

}  // namespace

TEST(chat_exe_games_check_never_starts_the_count_over) {
    // 2.6.12: R-PVF8AV - the game's own check keeps any count, 0xF7150 too: the line went 16 MB past the ten
    CHECK_EQ(chat_exe::kCheck + chat_exe::kSize, chat_exe::kReset);
    for (const int32_t count : {INT_MIN, -5, -1, 0, 3, 10, 11, 0xF7150, INT_MAX})
        CHECK_EQ(after(chat_exe::kOriginal, count), chat_exe::kKeep);
}

TEST(chat_exe_fixed_check_starts_over_a_count_out_of_the_ten) {
    for (int32_t count = 0; count <= 10; ++count) CHECK_EQ(after(chat_exe::kFixed, count), chat_exe::kKeep);
    for (const int32_t count : {INT_MIN, -5, -1, 11, 12, 0xF7150, INT_MAX})
        CHECK_EQ(after(chat_exe::kFixed, count), chat_exe::kReset);
}
