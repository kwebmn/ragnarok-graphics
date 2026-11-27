#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::groundstage_exe {

// 3.0.24-beta.31 (R-F4GZ3T «побіліла земля» on gef_fild07 after a few fly wings - the own ground's copy of the game's
// ground draw: stage 1 op 18 (34, 1) where it was (2, 1); R-7XY449, R-3SA4RB before it): the game's frame
// (FUN_0040e5a0) sets only the light map stage's op for its ground (18, lightmap + its alpha x the ground) and leaves
// its arguments as the last draw before it left them. Its device's start (0x4040e0..0x404128) makes them the light
// map's texture and the current colour; its texture modes (0x410320) 6 and 12 leave the texture complemented, 7 its
// alpha replicated (the alpha + the alpha x the ground: white where the light map has no shadow). The call at kAt
// goes to the driver's (stdcall, the device): the arguments as the device's start sets them, then the op.
constexpr uint32_t kAt = 0x40E6C8;  // push 0x12; push 1; push 1; mov ecx, [eax]; push eax; call [ecx + 0x94]
constexpr size_t kSize = 15;
extern const uint8_t kOriginal[kSize];

struct Step {
    uint32_t stage, type, value;  // SetTextureStageState's
};
constexpr size_t kStepCount = 3;
extern const Step kSteps[kStepCount];  // the op last: its answer the game's

// The patch at kAt: push eax (the device); call `fn`; nops to the game's next instruction (kAt + kSize)
void patch(uint32_t fn, uint8_t out[kSize]);

}  // namespace uagfx::groundstage_exe
