#pragma once
#include <windows.h>
#include <x86intrin.h>

#include <cstdint>

namespace uagfx::gpu {

// 3.0.19-beta.52 (R-GHP9SJ; core/frame_pace ranOf): the calling thread's cycles (QueryThreadCycleTime) and the time
// stamp counter now - two marks tell how much of the span between them the thread ran (a thread kept from the
// processor runs less of it than it waits)
struct RunMark {
    uint64_t cycles = 0, tsc = 0;
    static RunMark now() {
        RunMark m;
        ULONG64 c = 0;
        if (QueryThreadCycleTime(GetCurrentThread(), &c)) m.cycles = c;
        m.tsc = __rdtsc();
        return m;
    }
};

// A span's cycles and ticks added to two totals at its end (or at stop())
struct RunTime {
    uint64_t& cycles;
    uint64_t& tsc;
    RunMark start = RunMark::now();
    bool going = true;
    RunTime(uint64_t& c, uint64_t& t) : cycles(c), tsc(t) {}
    ~RunTime() { stop(); }
    RunTime(const RunTime&) = delete;
    RunTime& operator=(const RunTime&) = delete;
    void stop() {
        if (!going) return;
        going = false;
        const RunMark end = RunMark::now();
        cycles += end.cycles - start.cycles;
        tsc += end.tsc - start.tsc;
    }
};

}  // namespace uagfx::gpu
