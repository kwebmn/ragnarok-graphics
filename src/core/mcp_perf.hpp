#pragma once
#include <string>

#include "core/frame_stats.hpp"
#include "core/gpu_timing.hpp"
#include "core/json.hpp"

// 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 2): the MCP's perf - the game's frames of
// the seconds asked (core/frame_stats), the card's last half second (GpuTiming, its passes), what the frames stand on
namespace uagfx::mcpperf {

struct Facts {
    std::string api, adapter, map, window, plan;
    int mains = -1, batteryPercent = -1;  // -1 unknown
    double gameMemoryMb = -1;             // the game's process (its working set); < 0 unknown
};
json::Value report(const framestats::Stats& s, const GpuTiming* gpu, const Facts& f);

}  // namespace uagfx::mcpperf
