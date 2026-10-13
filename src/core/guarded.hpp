#pragma once

namespace uagfx {

// Runs work; an exception from it goes to onFailure instead of up, and nothing leaves this function (0.5.0 review
// M3): the crisp text hooks run inside the game's MSVC frames, which a C++ exception from the driver must never
// cross. true when work finished.
template <typename Work, typename Failure>
bool guarded(Work&& work, Failure&& onFailure) noexcept {
    try {
        work();
        return true;
    } catch (...) {
        try {
            onFailure();
        } catch (...) {
        }
        return false;
    }
}

}  // namespace uagfx
