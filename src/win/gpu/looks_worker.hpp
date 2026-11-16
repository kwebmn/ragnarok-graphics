#pragma once
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <deque>

#include "core/upload_later.hpp"

namespace uagfx::gpu {

// 3.0.20-beta.36 (the owner's «6,7,8,9», 9 - the frame's work on more threads; R-JJE8HA's GTX 970 1.00 ms a frame on
// the textures, the owner's 0.14-0.31): the whole textures' work after their upload (core uploadlater lookAt - their
// fire, light and leaves, the AI pack's view and key) on a thread of its own under the renderer's and the game's
// (below normal), while they draw - the renderer handing it kInFlight jobs at most and taking what it made at its next
// picture, the AI pack's own part there. No thread: the renderer works on them itself, as before. The renderer's
// thread alone calls it.
class LooksWorker {
public:
    static constexpr size_t kInFlight = 8;
    LooksWorker() = default;
    LooksWorker(const LooksWorker&) = delete;
    LooksWorker& operator=(const LooksWorker&) = delete;
    ~LooksWorker();
    bool running();  // its thread started at the first ask; false: none
    bool room() const { return given_ < kInFlight; }
    void give(uploadlater::Job&& j, bool aiKey);
    bool take(uploadlater::Looks& out);  // one it made; false: none yet

private:
    struct Work {
        uploadlater::Job job;
        bool aiKey = false;
    };
    static DWORD WINAPI run(void* self);
    void loop();
    SRWLOCK lock_ = SRWLOCK_INIT;
    CONDITION_VARIABLE wake_ = CONDITION_VARIABLE_INIT;
    std::deque<Work> todo_;
    std::deque<uploadlater::Looks> done_;
    size_t given_ = 0;  // given and not taken back (the renderer's thread alone)
    HANDLE thread_ = nullptr;
    bool tried_ = false, stop_ = false;
    // 3.0.20-beta.40 (the audit): the last time it gave anything back (or was first given work) - none for kLateMs
    // with work in hand: the processor's threads all busy over it, its priority the renderer's from then on
    unsigned long long progressAt_ = 0;
    bool raised_ = false;
    static constexpr unsigned long long kLateMs = 500;
};

}  // namespace uagfx::gpu
