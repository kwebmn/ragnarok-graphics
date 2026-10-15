#include "core/ai/vram.hpp"

#include <algorithm>

namespace uagfx::ai {

std::vector<uint32_t> putAway(const std::vector<Copy>& copies, uint64_t total, uint64_t budget, uint64_t now,
                              uint64_t minAge) {
    std::vector<uint32_t> out;
    if (!budget || total <= budget) return out;
    std::vector<const Copy*> old;
    for (const Copy& c : copies)
        if (now > c.lastUse && now - c.lastUse > minAge) old.push_back(&c);
    std::sort(old.begin(), old.end(), [](const Copy* a, const Copy* b) {
        return a->lastUse != b->lastUse ? a->lastUse < b->lastUse : a->id < b->id;
    });
    const uint64_t target = budget - budget / 10;
    for (const Copy* c : old) {
        if (total <= target) break;
        out.push_back(c->id);
        total -= std::min(total, c->bytes);
    }
    return out;
}

uint64_t copiesBudget(uint64_t systemBudget) {
    const uint64_t mb = 1024ull * 1024;
    if (!systemBudget) return 1024 * mb;
    return std::max<uint64_t>(systemBudget / 10 * 4, 256 * mb);
}

}  // namespace uagfx::ai
