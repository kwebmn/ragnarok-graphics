#include "core/key_chars.hpp"

namespace uagfx::keychars {

namespace {
unsigned keyOf(unsigned long long lParam) { return unsigned((lParam >> 16) & 0x1FF); }
}  // namespace

Press pressed(unsigned long long lParam, uint64_t nowMs, bool toGame) {
    Press p;
    p.key = keyOf(lParam);
    p.atMs = nowMs;
    p.toGame = toGame;
    return p;
}

bool keepLetter(const Press& last, unsigned long long lParam, uint64_t nowMs) {
    if (last.toGame || last.key == 0 || keyOf(lParam) != last.key) return false;
    return nowMs >= last.atMs && nowMs - last.atMs <= kWithinMs;
}

}  // namespace uagfx::keychars
