#include "core/res_tree_exe.hpp"

namespace uagfx::res_tree_exe {

const uint8_t kFn[kFnSize] = {
    0x55, 0x8B, 0xEC, 0x8B, 0x41, 0x04, 0x56, 0x57, 0x8B, 0xF8, 0x8B, 0x70, 0x04, 0xA1, 0x98, 0xBB, 0x74, 0x00, 0x3B,
    0xF0, 0x74, 0x2D, 0x53, 0x8B, 0x5D, 0x08, 0x8B, 0x03, 0x8B, 0x4E, 0x0C, 0x50, 0xE8, 0xFB, 0xB5, 0xFF, 0xFF, 0x84,
    0xC0, 0x74, 0x05, 0x8B, 0x76, 0x08, 0xEB, 0x04, 0x8B, 0xFE, 0x8B, 0x36, 0x3B, 0x35, 0x98, 0xBB, 0x74, 0x00, 0x75,
    0xE0, 0x8B, 0xC7, 0x5B, 0x5F, 0x5E, 0x5D, 0xC2, 0x04, 0x00, 0x8B, 0xC7, 0x5F, 0x5E, 0x5D, 0xC2, 0x04, 0x00};

uint32_t lowerBound(uint32_t map, uint32_t keyRef, uint32_t nil, const std::function<uint32_t(uint32_t)>& read,
                    const std::function<bool(uint32_t, uint32_t)>& less, bool& broken) {
    const uint32_t head = read(map + 4);
    uint32_t best = head;
    uint32_t node = read(head + 4);
    constexpr int kMostSteps = 64;  // a tree's height; past it the tree is being changed: not found
    for (int steps = 0; node != nil; ++steps) {
        const uint32_t key = read(keyRef), its = read(node + 0xC);
        if (!key || !its || !node || steps >= kMostSteps) {
            broken = true;
            return head;
        }
        if (less(its, key)) {
            node = read(node + 8);
        } else {
            best = node;
            node = read(node);
        }
    }
    return best;
}

void callTo(uint32_t site, uint32_t target, uint8_t out[5]) {
    const uint32_t rel = target - (site + 5);
    out[0] = 0xE8;
    for (int i = 0; i < 4; ++i) out[1 + i] = uint8_t(rel >> (8 * i));
}

}  // namespace uagfx::res_tree_exe
