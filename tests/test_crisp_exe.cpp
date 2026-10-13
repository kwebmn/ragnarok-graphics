#include <string>

#include "core/crisp_exe.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(crisp_exe_the_known_methods_pass) {
    uint32_t e[crisp_exe::kBitmapSlots];
    for (int i = 0; i < crisp_exe::kBitmapSlots; ++i) e[i] = crisp_exe::kBitmapMethods[i];
    CHECK(crisp_exe::checkBitmapMethods(e).empty());
    // As read from the .rdata of uaRO.exe (docs/research/text.md §1.1).
    CHECK_EQ(crisp_exe::kBitmapMethods[crisp_exe::kClear], 0x403020u);
    CHECK_EQ(crisp_exe::kBitmapMethods[crisp_exe::kDraw], 0x403880u);
}

TEST(crisp_exe_another_method_turns_crisp_text_off) {
    uint32_t e[crisp_exe::kBitmapSlots];
    for (int i = 0; i < crisp_exe::kBitmapSlots; ++i) e[i] = crisp_exe::kBitmapMethods[i];
    e[7] = 0x10001234;  // another exe, or something patched the vtable before us
    const std::string why = crisp_exe::checkBitmapMethods(e);
    CHECK(why.find("method 7") != std::string::npos);
    CHECK(why.find("0x10001234") != std::string::npos);
    CHECK(why.find("0x00403460") != std::string::npos);
}
