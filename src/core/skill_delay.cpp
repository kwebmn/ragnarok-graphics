#include "core/skill_delay.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>

namespace uagfx::skilldelay {
namespace {

struct Delay {
    uint16_t id;
    int ms[10];
};
#include "core/skill_delay.inc"

}  // namespace

int afterCast(int skill, int level) {
    const auto at = std::lower_bound(std::begin(kDelays), std::end(kDelays), skill,
                                     [](const Delay& d, int v) { return int(d.id) < v; });
    if (at == std::end(kDelays) || int(at->id) != skill) return 0;
    return at->ms[std::clamp(level, 1, 10) - 1];
}

int withDex(int ms, int dex) { return std::max(0, ms * std::max(0, 150 - dex) / 150); }

float left(unsigned long long from, int ms, unsigned long long now) {
    if (ms <= 0 || now < from || now - from >= static_cast<unsigned long long>(ms)) return 0.0f;
    return 1.0f - float(now - from) / float(ms);
}

}  // namespace uagfx::skilldelay
