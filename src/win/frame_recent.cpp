#include "win/frame_recent.hpp"

namespace uagfx::win::framerecent {
namespace {

framepace::Recent& ring() {
    static framepace::Recent r;
    return r;
}

}  // namespace

void add(const framepace::GameFrame& f) { ring().add(f, double(GetTickCount64())); }

std::string around(ULONGLONG atMs, ULONGLONG halfMs) {
    framepace::GameFrame longest;
    uint32_t frames = 0, reads = 0;
    double readMs = 0.0;
    const double at = double(atMs), half = double(halfMs);
    if (!ring().longestBetween(at - half, at + half, longest, frames, reads, readMs)) return {};
    return framepace::eventWords(longest, frames, reads, readMs, 2.0 * half / 1000.0);
}

}  // namespace uagfx::win::framerecent
