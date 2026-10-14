#pragma once
#include <atomic>
#include <cstdint>

namespace uagfx::bridge {

// Part 3: the 32-bit driver hands every renderer call to uaro-render64.exe, which draws with Direct3D 12 in a 64-bit
// process (DLSS and frame generation exist only for 64-bit programs). They share one block of memory: BlockHeader,
// the ring from the driver to the renderer, then the ring back (core/bridge_ring.hpp).
constexpr uint32_t kMagic = 0x52423455;  // "U4BR"
constexpr uint32_t kVersion = 13;  // 2: uploads carry the sprite cache's cells (0.13); 3: icons (0.14); 4: what
                                  // sprite a cell's frame is of (0.16); 5: effects (0.18); 6: the water (0.19);
                                  // 7: soft shadows, the glow's light (0.20); 8: colours (0.21); 9: depth of
                                  // field (0.22); 10: smooth map light (0.22.6); 11: torches (0.23); 12: the
                                  // torches' own textures (0.23.1); 13: their flame's brightness (0.23.2)

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
    SetDrawQuality,     // i32 anisotropy, u8 alpha to coverage, u8 AI textures shown
    CreateTexture,      // u32 id, i32 width, height, levels
    ReleaseTexture,     // u32 id
    Execute,            // a frame record (writeRecord)
    ReadTarget,         // -> TargetPixels
    Present,            // u64 window, i32 output width, height, rect source, u8 vsync, u8 tearing, i32 fps limit,
                        // f32 sharpen, u8 the window in front
    SetSurface,         // u64 a DirectComposition surface handle, valid in the renderer's process
    Camera,             // u8 valid, 12 f32: the game's view matrix of the frame being drawn (DLSS)
    Quit,
    Map,                // str: the map the game shows, "" none (AI textures note the textures seen on it)
    IconWanted,         // u64 key, i32 width, height, then width * height ARGB: an icon of a window for the network
    SetEffects,         // u8 glow, i32 glow strength, size, light, u8 shadows, i32 their strength, size, u8 water,
                        // i32 waves, reflections, clarity, u8 colours, i32 saturation, contrast, brightness,
                        // warmth, vignette, u8 depth of field, i32 its strength, range, u8 smooth map light,
                        // u8 torches, i32 their glow, light, flicker, u8 the new flame, i32 its brightness
                        // ([effects], 0.18-0.23.2)
    TorchTexture,       // u32 id, u8 layer (torch::Layer): a texture that is a layer of the torches (0.23.1)
    // renderer -> driver
    Hello = 100,        // u8 ok, str adapter (ok) or what failed; u8 DLSS ready (when ok)
    Log,                // u8 level, str text
    TargetSet,          // u8 ok
    ScaleSet,           // u8 ok, i32 samples
    TargetPixels,       // u8 ok, i32 width, height, then width * height ARGB values
    NeedSurface,        // the game's window took no swap chain from this process: a composition surface instead
    Lost,               // str why: the renderer cannot draw any more
    UpscalerReady,      // DLSS came up after the Hello (it starts in the background)
    IconDone,           // u64 key, i32 width, height, then width * height ARGB: that icon made larger
};

}  // namespace uagfx::bridge
