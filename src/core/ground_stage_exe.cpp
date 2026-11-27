#include "core/ground_stage_exe.hpp"

#include <cstring>

#include "core/d3d7.hpp"

namespace uagfx::groundstage_exe {

const uint8_t kOriginal[kSize] = {0x6A, 0x12, 0x6A, 0x01, 0x6A, 0x01, 0x8B, 0x08,
                                  0x50, 0xFF, 0x91, 0x94, 0x00, 0x00, 0x00};

const Step kSteps[kStepCount] = {
    {1, d3d7::TSS_COLORARG1, d3d7::TA_TEXTURE},              // 0x4040f2
    {1, d3d7::TSS_COLORARG2, d3d7::TA_CURRENT},              // 0x404116
    {1, d3d7::TSS_COLOROP, d3d7::TOP_MODULATEALPHA_ADDCOLOR},  // the game's own call
};

void patch(uint32_t fn, uint8_t out[kSize]) {
    out[0] = 0x50;  // push eax
    out[1] = 0xE8;  // call rel32
    const int32_t rel = int32_t(fn - (kAt + 6));
    std::memcpy(out + 2, &rel, 4);
    std::memset(out + 6, 0x90, kSize - 6);
}

}  // namespace uagfx::groundstage_exe
