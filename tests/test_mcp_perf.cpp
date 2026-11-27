// 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 2): the MCP's perf - the game's frames of
// the seconds asked, the card's last half second (its passes), what the frame stands on (API, card, power, memory)
#include <cmath>
#include <string>

#include "core/frame_stats.hpp"
#include "core/gpu_timing.hpp"
#include "core/mcp.hpp"
#include "core/mcp_perf.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(mcp_perf_report) {
    framestats::Window w;
    uint64_t us = 1000000;
    w.present(us);
    for (int i = 1; i <= 300; ++i) {
        us += i == 150 ? 80000 : 16667;
        w.present(us);
    }
    GpuTiming g;
    g.gpuMs = 7.1, g.presentMs = 0.3, g.cpuMs = 4.2, g.upscaler = "DLSS Quality", g.upscaleMs = 2.6;
    g.reflex = true, g.shown = 1.0;
    g.effects = {{"grass", 0.4}, {"shadows", 1.25}};
    mcpperf::Facts f;
    f.api = "Direct3D 12", f.adapter = "NVIDIA GeForce RTX 3060 Laptop GPU", f.map = "prontera";
    f.window = "1920x1080", f.mains = 0, f.batteryPercent = 76, f.plan = "Balanced", f.gameMemoryMb = 812.4;
    const json::Value r = mcpperf::report(w.stats(us, 10), &g, f);
    CHECK(r["frames"].asInt() >= 295 && r["frames"].asInt() <= 300);
    CHECK(std::abs(r["fps"].asNumber() - 59.0) < 3.0);
    CHECK(std::abs(r["frameMs"]["p50"].asNumber() - 16.7) < 0.1);
    CHECK(std::abs(r["frameMs"]["worst"].asNumber() - 80.0) < 0.1);
    CHECK_EQ(r["over"]["50ms"].asInt(), int64_t(1));
    CHECK(std::abs(r["longest"][size_t(0)]["ms"].asNumber() - 80.0) < 0.1);
    CHECK(std::abs(r["gpu"]["frameMs"].asNumber() - 7.1) < 0.01);
    CHECK(std::abs(r["gpu"]["passes"]["shadows"].asNumber() - 1.25) < 0.01);
    CHECK_EQ(r["gpu"]["upscaler"].asString(), std::string("DLSS Quality"));
    CHECK(r["gpu"]["reflex"].asBool());
    CHECK_EQ(r["api"].asString(), std::string("Direct3D 12"));
    CHECK(!r["power"]["mains"].asBool(true) && r["power"]["batteryPercent"].asInt() == 76);
    CHECK(std::abs(r["memoryMb"]["game"].asNumber() - 812.4) < 0.05);
    // the card not heard from yet: no gpu member; the power unknown: none either
    mcpperf::Facts bare;
    const json::Value n = mcpperf::report(w.stats(us, 10), nullptr, bare);
    CHECK(!n.has("gpu") && !n.has("power") && n.has("frameMs"));
    // its arguments: seconds 1-60
    CHECK(mcp::checkArgs("perf", json::Value::object()).empty());
    json::Value bad = json::Value::object();
    bad.set("seconds", 61);
    CHECK(!mcp::checkArgs("perf", bad).empty());
    bad.set("seconds", 5);
    CHECK(mcp::checkArgs("perf", bad).empty());
}
