#include <string>

#include "core/effect_costs.hpp"
#include "core/gpu_timing.hpp"
#include "core/live_config.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(gpu_timing_goes_over_the_bridge_whole) {
    // 2.2.1: the 64-bit renderer's measurement to the game's process (Msg::Timing)
    GpuTiming t;
    t.gpuMs = 7.25;
    t.upscaleMs = 2.5;
    t.upscaler = "DLSS Quality";
    t.serial = 41;
    t.effects = {{"grass", 2.375}, {"new_trees", 0.125}};
    std::vector<uint8_t> bytes;
    bridge::WireWriter w(bytes);
    writeTiming(w, t);
    bridge::WireReader rd(bytes.data(), bytes.size());
    GpuTiming back;
    CHECK(readTiming(rd, back));
    CHECK(back.gpuMs == 7.25 && back.upscaleMs == 2.5 && back.upscaler == "DLSS Quality" && back.serial == 41);
    CHECK(back.effects.size() == 2 && back.effects["grass"] == 2.375);
    bridge::WireReader cut(bytes.data(), 6);  // cut short: nothing taken
    GpuTiming none;
    CHECK(!readTiming(cut, none) && none.gpuMs < 0.0);
}

TEST(gpu_timing_names_the_passes_of_the_switches) {
    // the console sums a group's passes, each once: the motes are the weather's pass
    CHECK(effect_costs::passOf("effects.motes") == effect_costs::passOf("effects.weather"));
    CHECK(effect_costs::passOf("effects.grass") == "grass");
    CHECK(effect_costs::passOf("effects.materials").empty());
    const effect_costs::Costs c = {{"grass", 2.4}};
    CHECK(effect_costs::costOf(c, "effects.grass") > 2.3);
}

TEST(live_config_shows_the_frame_counter_at_once) {
    // 2.2.1: [frames] fps_counter from the console, the settings or Ctrl+Shift+F11 - no restart
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[frames]\nfps_counter = 1\n"));
    CHECK(u.next.fpsCounter);
    CHECK(u.waiting.empty());
    CHECK(appliesNow("frames.fps_counter"));
}

TEST(ai_kinds_shown_follow_the_file_at_once) {
    // 2.2.1: [ai] in a running game - a kind off hidden at once (aiNow), the started settings kept (ai)
    AiSettings a;
    a.enabled = true;
    a.ground = a.objects = a.icons = true;
    a.effects = false;
    CHECK_EQ(int(aiKindsShown(a)), 1 | 2 | 16);
    a.players = true;
    CHECK_EQ(int(aiKindsShown(a)), 1 | 2 | 4 | 16);
    a.enabled = false;
    CHECK_EQ(int(aiKindsShown(a)), 0);
    const Config running = parseConfig("[ai]\nenabled = 1\nground = 1\n");
    const LiveUpdate u = liveUpdate(running, parseConfig("[ai]\nenabled = 1\nground = 0\n"));
    CHECK(u.next.ai.ground && !u.next.aiNow.ground);  // the pack as started, the ground hidden now
    CHECK(running.aiNow.ground);
}
