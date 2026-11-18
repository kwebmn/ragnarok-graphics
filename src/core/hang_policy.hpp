#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// 3.0.22-beta.8 (R-JW5W4N / R-EZ7BUH «зависло. на 42%»: the 64-bit renderer gone with exit code 0xC0000409 two
// seconds into prontera, its thread 2.2 s in one CreateTexture - «stood in CreateTexture for 2187 ms»; a fast fail runs
// no handler of ours and leaves no dump): when the renderer's watch (host/hang_watch) tells that its thread stands in
// one message, and when it writes the renderer's own minidump while the thread still stands there. A texture's
// creation or release never takes a second: told from one second, the dump at that first tell (once a game); the
// other messages - a frame's draws at a map's start, the pipelines made at the renderer's start - from two seconds,
// no dump. No Windows here.
namespace uagfx::hangpolicy {

// The moments (ms into one message, `message` a bridge::Msg; 0 the look at DLSS's start) its stand is told
const std::vector<uint64_t>& tellsFor(uint32_t message);
// The tell `tell` (its index in tellsFor) writes the dump: a texture's message, its first tell, none written yet
bool dumpAt(uint32_t message, size_t tell, bool dumped);

}  // namespace uagfx::hangpolicy
