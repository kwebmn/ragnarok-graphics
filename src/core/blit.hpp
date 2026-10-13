#pragma once
#include <cstdint>

namespace uagfx {

// DDBLT / DDBLTFAST flags the client uses (ddraw.h values).
namespace ddblt {
constexpr uint32_t ASYNC = 0x200;
constexpr uint32_t COLORFILL = 0x400;
constexpr uint32_t DDFX = 0x800;
constexpr uint32_t KEYSRC = 0x8000;
constexpr uint32_t WAIT = 0x1000000;
}  // namespace ddblt
namespace ddbltfast {
constexpr uint32_t SRCCOLORKEY = 0x1;
constexpr uint32_t DESTCOLORKEY = 0x2;
constexpr uint32_t WAIT = 0x10;
}  // namespace ddbltfast

// Where a surface's pixels live: CPU memory (offscreen surfaces, textures), the GPU render target (the back
// buffer the client draws into), or the window (the primary surface).
enum class Place { Cpu, Target, Front };

enum class BlitAction {
    Fill,         // DDBLT_COLORFILL on a CPU surface
    FillTarget,   // DDBLT_COLORFILL on the render target
    CopyCpu,      // CPU surface -> CPU surface (mip levels, glyphs, big pictures)
    ReadTarget,   // render target -> CPU surface (calibration readback, fade save)
    WriteTarget,  // CPU surface -> render target (fade restore, pictures bigger than a texture)
    Present,      // render target -> primary: show a frame (windowed present, loading bar, Bink)
    Unsupported,
};

struct BlitRequest {
    Place dst = Place::Cpu;
    Place src = Place::Cpu;
    bool hasSource = false;
    uint32_t flags = 0;
};

BlitAction routeBlit(const BlitRequest& r);

}  // namespace uagfx
