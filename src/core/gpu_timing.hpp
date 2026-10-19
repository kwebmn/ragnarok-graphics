#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "core/bridge_wire.hpp"

namespace uagfx {

// 2.2.1: the graphics card's work, the last half second's average a frame - for the frame counter and the console
// (~), to see what an effect costs as it is switched: the frames a second stay at the limit (60), the card's
// milliseconds do not. The 64-bit renderer sends it to the game's process (bridge Msg::Timing).
struct GpuTiming {
    double gpuMs = -1.0;     // the frame's whole work; < 0: not measured
    double upscaleMs = 0.0;  // of it the upscaler's (DLSS)
    std::string upscaler;    // its name, "" none
    std::map<std::string, double> effects;  // the passes timed on their own, by effect_costs' names ("grass")
    uint32_t serial = 0;  // a new measurement, a new number
    // 2.2.2: the shaders the drawing process compiled without their newest features (UAGFX_SAFE), for the statistics
    int safeShaders = 0;
    // 2.5.24: frames shown a frame of the game with DLSS frame generation (the generated ones too, 2-6) - the counter
    // counts the game's frames alone; 0: none generated
    double shown = 0.0;
    // 2.5.26: NVIDIA Reflex holds the renderer's frames until the card is about to be free - the game then draws no
    // frame ahead of the screen (Remote::present)
    bool reflex = false;
};

inline void writeTiming(bridge::WireWriter& w, const GpuTiming& t) {
    w.f32(float(t.gpuMs));
    w.f32(float(t.upscaleMs));
    w.str(t.upscaler);
    w.u32(t.serial);
    w.u32(uint32_t(t.safeShaders > 0 ? t.safeShaders : 0));
    w.u8(uint8_t(t.effects.size() < 255 ? t.effects.size() : 255));
    int n = 0;
    for (const auto& [name, ms] : t.effects) {
        if (++n > 255) break;
        w.str(name);
        w.f32(float(ms));
    }
    w.f32(float(t.shown));  // 2.5.24
    w.u8(t.reflex ? 1 : 0);  // 2.5.26
}

inline bool readTiming(bridge::WireReader& rd, GpuTiming& t) {
    GpuTiming r;
    r.gpuMs = rd.f32();
    r.upscaleMs = rd.f32();
    r.upscaler = rd.str();
    r.serial = rd.u32();
    r.safeShaders = int(rd.u32() & 0xFFFF);
    const int n = rd.u8();
    for (int i = 0; i < n && rd.ok(); ++i) {
        const std::string name = rd.str();
        const double ms = rd.f32();
        if (rd.ok()) r.effects[name] = ms;
    }
    r.shown = rd.f32();  // 2.5.24
    r.reflex = rd.u8() != 0;  // 2.5.26
    if (!rd.ok()) return false;
    t = r;
    return true;
}

}  // namespace uagfx
