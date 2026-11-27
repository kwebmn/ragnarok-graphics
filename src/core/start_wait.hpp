#pragma once
#include <cstdint>

// 3.0.24-beta.36 (the laptop's run, AMD Radeon 610M): the game gave the 64-bit renderer 90 s in all for its start
// (1.8.1); it was still compiling its shaders, telling each one done (Msg::StartProgress), when it was ended - and as
// it kept none of them, each start went the same way. The game waits while the renderer is heard from: 90 s of silence
// at most (the old whole wait), 5 minutes in all.
namespace uagfx::startwait {

constexpr uint32_t kQuietMs = 90000;
constexpr uint32_t kMostMs = 300000;

struct Wait {
    explicit Wait(uint64_t nowMs) : startMs(nowMs), heardMs(nowMs) {}
    void heard(uint64_t nowMs) { heardMs = nowMs; }
    // How long to wait on from `nowMs`; 0: the wait is over
    uint32_t left(uint64_t nowMs) const;

    uint64_t startMs;
    uint64_t heardMs;
};

}  // namespace uagfx::startwait
