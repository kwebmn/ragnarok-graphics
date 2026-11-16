#pragma once
#include <cstdint>

// 3.0.20-beta.76: DLSS comes up in the 64-bit renderer's background (NVIDIA's own start, 8-9.5 s) and the picture then
// goes over to Streamline's factory on the renderer's own thread - the card waited for, the swap chain let go and
// made anew, the world's size changed. The owner's logs (96 reports): 58 games with the map standing 4-6 s in and
// DLSS coming up 2.5-8 s after the map's name (its start running over the map's own start) and 20 with the map
// after it - fine every time; the one game where the map's name came 0.4 s before the start's end (16.11 22:40:54,
// beta.68) - the renderer silent from the map's first frame, gone five seconds later (exit code 0xC0000409, no
// report: core/report_upload reasonOfLog now takes it). A map's first second and a half is its heaviest - every
// picture goes up, its world is made on other threads (the models' pictures up 1.4 s in). So the handing over keeps
// out of a map's first kMapQuietMs - kMostHeldMs in all at most. The cause itself is not known (one case, no
// dump): the renderer's hang watch (host/hang_watch) tells where the thread stands next time.
namespace uagfx::latestart {

constexpr uint64_t kMapQuietMs = 3000;
constexpr uint64_t kMostHeldMs = 30000;

// whether the handing over may go at `now` (milliseconds): the last map's name came at `mapAt` (0: none so far),
// it first came due at `askedAt`
constexpr bool quiet(uint64_t now, uint64_t mapAt, uint64_t askedAt) {
    return !mapAt || now - mapAt >= kMapQuietMs || now - askedAt >= kMostHeldMs;
}

}  // namespace uagfx::latestart
