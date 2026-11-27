// 3.0.24-beta.31 (R-F4GZ3T «побіліла земля» on gef_fild07 after a few fly wings; R-7XY449, R-3SA4RB before it): the
// game's ground draw sets only its light map stage's op (18, uaRO.exe 0x40e6d1) and leaves its arguments as another
// draw left them - its texture modes 6, 7 and 12 (0x410320) leave the light map's argument complemented or its alpha
// replicated, and the ground comes out white. The call goes to the driver's: both arguments as the device's start
// sets them (0x4040f2, 0x404116), then the op
#include <cstdint>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/ground_stage_exe.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(ground_stage_exe_its_bytes_and_steps) {
    // push 0x12; push 1; push 1; mov ecx, [eax]; push eax; call [ecx + 0x94] (SetTextureStageState)
    const uint8_t want[] = {0x6A, 0x12, 0x6A, 0x01, 0x6A, 0x01, 0x8B, 0x08, 0x50, 0xFF, 0x91, 0x94, 0x00, 0x00, 0x00};
    CHECK_EQ(groundstage_exe::kSize, sizeof want);
    CHECK(std::memcmp(groundstage_exe::kOriginal, want, sizeof want) == 0);
    CHECK_EQ(groundstage_exe::kAt, uint32_t(0x40E6C8));
    // the light map's stage as the device's start makes it, the op last (its answer the game's)
    CHECK_EQ(groundstage_exe::kStepCount, size_t(3));
    const groundstage_exe::Step* s = groundstage_exe::kSteps;
    CHECK(s[0].stage == 1 && s[0].type == d3d7::TSS_COLORARG1 && s[0].value == d3d7::TA_TEXTURE);
    CHECK(s[1].stage == 1 && s[1].type == d3d7::TSS_COLORARG2 && s[1].value == d3d7::TA_CURRENT);
    CHECK(s[2].stage == 1 && s[2].type == d3d7::TSS_COLOROP && s[2].value == d3d7::TOP_MODULATEALPHA_ADDCOLOR);
}

TEST(ground_stage_exe_the_patch) {
    uint8_t out[groundstage_exe::kSize];
    groundstage_exe::patch(0x10002000, out);
    CHECK(out[0] == 0x50 && out[1] == 0xE8);  // push eax (the device); call the driver's
    int32_t rel = 0;
    std::memcpy(&rel, out + 2, 4);
    CHECK_EQ(uint32_t(int32_t(groundstage_exe::kAt + 6) + rel), uint32_t(0x10002000));
    for (size_t i = 6; i < groundstage_exe::kSize; ++i) CHECK(out[i] == 0x90);  // to the game's next instruction
}
