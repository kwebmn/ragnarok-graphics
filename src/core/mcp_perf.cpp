#include "core/mcp_perf.hpp"

#include <cmath>

namespace uagfx::mcpperf {
namespace {

double round2(double v) { return std::round(v * 100.0) / 100.0; }

}  // namespace

json::Value report(const framestats::Stats& s, const GpuTiming* gpu, const Facts& f) {
    json::Value r = json::Value::object();
    r.set("seconds", round2(s.seconds));
    r.set("frames", s.frames);
    r.set("fps", round2(s.fps));
    json::Value ms = json::Value::object();
    ms.set("p50", round2(s.p50));
    ms.set("p95", round2(s.p95));
    ms.set("p99", round2(s.p99));
    ms.set("worst", round2(s.worst));
    r.set("frameMs", ms);
    json::Value over = json::Value::object();
    over.set("25ms", s.over25);
    over.set("50ms", s.over50);
    over.set("100ms", s.over100);
    r.set("over", over);
    json::Value longest = json::Value::array();
    for (const framestats::Long& l : s.longest) {
        json::Value x = json::Value::object();
        x.set("agoS", round2(l.ago));
        x.set("ms", round2(l.ms));
        longest.push(x);
    }
    r.set("longest", longest);
    if (gpu && gpu->gpuMs >= 0) {
        json::Value g = json::Value::object();
        g.set("frameMs", round2(gpu->gpuMs));
        g.set("presentMs", round2(gpu->presentMs));
        g.set("cpuMs", round2(gpu->cpuMs));
        if (!gpu->upscaler.empty()) {
            g.set("upscaler", gpu->upscaler);
            g.set("upscaleMs", round2(gpu->upscaleMs));
        }
        g.set("reflex", gpu->reflex);
        g.set("shownPerFrame", round2(gpu->shown > 0 ? gpu->shown : 1.0));
        g.set("loading", gpu->loading);
        json::Value passes = json::Value::object();
        for (const auto& [name, v] : gpu->effects) passes.set(name, round2(v));
        g.set("passes", passes);
        r.set("gpu", g);
    }
    if (!f.api.empty()) r.set("api", f.api);
    if (!f.adapter.empty()) r.set("adapter", f.adapter);
    if (!f.window.empty()) r.set("window", f.window);
    if (!f.map.empty()) r.set("map", f.map);
    if (f.mains >= 0) {
        json::Value p = json::Value::object();
        p.set("mains", f.mains == 1);
        if (f.batteryPercent >= 0) p.set("batteryPercent", f.batteryPercent);
        if (!f.plan.empty()) p.set("plan", f.plan);
        r.set("power", p);
    }
    if (f.gameMemoryMb >= 0) {
        json::Value m = json::Value::object();
        m.set("game", round2(f.gameMemoryMb));
        r.set("memoryMb", m);
    }
    return r;
}

}  // namespace uagfx::mcpperf
