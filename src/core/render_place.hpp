#pragma once
#include <cstdint>
#include <string>

// 3.0.20-beta.83 (R-EGT98G: the 64-bit renderer gone with exit code 0xC0000409 five seconds after rachel's first
// frame, as on the owner's PC 16.11 at prontera - a fast fail runs no handler of the renderer's and its log tells
// nothing of those seconds): where the renderer's thread stands, kept in the bridge's block (core/bridge_protocol.hpp
// BlockHeader) by the renderer as it goes, read by the driver once the process is gone
namespace uagfx::renderplace {

enum Step : uint32_t {  // within a frame's record and its present (gpu/device.cpp Renderer::execute, present)
    kNone = 0,
    kGeometry,    // its vertices and indices put up
    kUploads,     // the game's textures written
    kDraws,       // its draws
    kAfterDraws,  // the casters and the own ground's notes
    kSubmit,      // the list to the GPU, the old resources let go
    kLooks,       // the whole textures' looks worked out from earlier uploads
    kCompose,     // the present: the picture put together (the world's end, the glow, the interface)
    kShow,        // the present: the swap chain's
};
const char* stepName(uint32_t step);  // "" for kNone and unknown ones

constexpr int kDone = 4;
struct Place {
    uint32_t message = 0;       // the message being taken (bridge::Msg; 0: none)
    uint32_t step = 0;          // where in it (Step)
    uint32_t sinceMs = 0;       // GetTickCount when it was taken
    uint32_t done[kDone] = {};  // the messages taken before it, the newest first (0: none)
};
const char* messageName(uint32_t message);  // a bridge message's name; "" unknown
// the driver's words: "in Execute for 5210 ms (its draws); before it: Present, Execute, Camera" or "between
// messages; before it: ..."
std::string describe(const Place& p, uint32_t nowMs);

// the renderer's step as it goes (gpu/device.cpp): to the sink uaro-render64.exe sets; nothing anywhere else
using StepSink = void (*)(uint32_t step);
void setStepSink(StepSink sink);
void step(uint32_t s);

}  // namespace uagfx::renderplace
