#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uapad {

// Addresses X in [lo, hi) that the code calls through as `call dword ptr [X]` (bytes FF 15 X), sorted and
// unique. The uaRO client keeps its send/recv function pointers in such variables (CRagConnection).
std::vector<uint32_t> indirectCallTargets(const uint8_t* code, size_t len, uint32_t lo, uint32_t hi);

}  // namespace uapad
