#pragma once
#include <atomic>
#include <cstdint>

namespace uagfx::bridge {

// Part 3: the 32-bit driver hands every renderer call to uaro-render64.exe, which draws with Direct3D 12 in a 64-bit
// process (DLSS and frame generation exist only for 64-bit programs). They share one block of memory: BlockHeader,
// the ring from the driver to the renderer, then the ring back (core/bridge_ring.hpp).
constexpr uint32_t kMagic = 0x52423455;  // "U4BR"
constexpr uint32_t kVersion = 1;

struct alignas(16) BlockHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t toHost;    // bytes of the driver -> renderer ring, from the end of this header
    uint32_t toClient;  // bytes of the renderer -> driver ring, after it
    std::atomic<uint32_t> presented;  // pictures the renderer has shown
    uint32_t reserved[11];
};
static_assert(sizeof(BlockHeader) == 64, "the same layout in both processes");

// Texture ids the driver gives out itself, so creating one never waits for the renderer; the renderer's own ids
// (its white texture) stay below.
constexpr uint32_t kFirstClientTexture = 0x40000000;

enum class Msg : uint32_t {
    // driver -> renderer
    SetUiSharp = 1,     // u8 sharp
    PreparePipelines,   // str file
    SetTarget,          // i32 width, height -> TargetSet
    SetScale,           // f64 k, i32 width, height, samples, u8 upscale -> ScaleSet
    SetDrawQuality,     // i32 anisotropy, u8 alpha to coverage
    CreateTexture,      // u32 id, i32 width, height, levels
    ReleaseTexture,     // u32 id
    Execute,            // a frame record (writeRecord)
    ReadTarget,         // -> TargetPixels
    Present,            // u64 window, i32 output width, height, rect source, u8 vsync, u8 tearing, i32 fps limit,
                        // f32 sharpen
    SetSurface,         // u64 a DirectComposition surface handle, valid in the renderer's process
    Camera,             // u8 valid, 12 f32: the game's view matrix of the frame being drawn (DLSS)
    Quit,
    // renderer -> driver
    Hello = 100,        // u8 ok, str adapter (ok) or what failed
    Log,                // u8 level, str text
    TargetSet,          // u8 ok
    ScaleSet,           // u8 ok, i32 samples
    TargetPixels,       // u8 ok, i32 width, height, then width * height ARGB values
    NeedSurface,        // the game's window took no swap chain from this process: a composition surface instead
    Lost,               // str why: the renderer cannot draw any more
};

}  // namespace uagfx::bridge
