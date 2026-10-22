#include "pad/core/call_targets.hpp"
#include "test.hpp"

using namespace uapad;

TEST(call_targets_finds_indirect_calls_into_a_range) {
    // call [0x6ee6e4]; nop; call [0x6ee66c]; call [0x695450] (an import slot, outside .data); call [0x6ee6e4] again
    const uint8_t code[] = {0xff, 0x15, 0xe4, 0xe6, 0x6e, 0x00, 0x90, 0xff, 0x15, 0x6c, 0xe6, 0x6e, 0x00,
                            0xff, 0x15, 0x50, 0x54, 0x69, 0x00, 0xff, 0x15, 0xe4, 0xe6, 0x6e, 0x00};
    std::vector<uint32_t> t = indirectCallTargets(code, sizeof code, 0x6b7000, 0x6ec000 + 0x35000);
    CHECK(t == (std::vector<uint32_t>{0x6ee66c, 0x6ee6e4}));  // sorted, unique, only inside the range
}

TEST(call_targets_ignores_truncated_and_other_opcodes) {
    const uint8_t code[] = {0xff, 0x25, 0xe4, 0xe6, 0x6e, 0x00,  // jmp [X] is not a call
                            0xa1, 0xe4, 0xe6, 0x6e, 0x00,        // mov eax,[X] neither
                            0xff, 0x15, 0xe4, 0xe6};             // cut off
    CHECK(indirectCallTargets(code, sizeof code, 0x600000, 0x800000).empty());
    CHECK(indirectCallTargets(code, 0, 0x600000, 0x800000).empty());
}
