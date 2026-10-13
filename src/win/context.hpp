#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/pixel.hpp"
#include "core/toggle_key.hpp"
#include "win/gpu/renderer.hpp"
#include "win/shadow.hpp"

namespace uagfx::win {

// What the DirectDraw objects share: the global lock (the client asks for DDSCL_MULTITHREADED; Bink may call from
// its own thread), the settings, the renderer, the frame being recorded, the display mode the client sees, the
// window and the CPU copy of the render target.
class Context {
public:
    static Context& get();
    void lock() { EnterCriticalSection(&cs_); }
    void unlock() { LeaveCriticalSection(&cs_); }

    Config config;
    gpu::Renderer* renderer = nullptr;
    FrameRecord record;
    int screenWidth = 0;
    int screenHeight = 0;
    PixelFormat screenFormat = PixelFormat::X8R8G8B8;
    HWND window = nullptr;
    bool windowApplied = false;  // [display] mode is applied at the first present, once the client's size is fixed
    bool vblankWaited = false;   // WaitForVerticalBlank came before the next Blt onto the primary
    bool gameFullscreen = false;  // the game asked for DDSCL_FULLSCREEN ([display] mode = auto follows it)
    Shadow shadow;

    bool open(std::string& error);          // the renderer, at DirectDrawCreateEx; false: use another DirectDraw
    void close();                           // the client released DirectDraw: the renderer goes with it
    bool setTarget(int width, int height);  // the renderer's target and the shadow, in the screen format
    void flush();                           // plays the record on the GPU
    bool readTarget(std::vector<uint32_t>& argb);
    void writeTarget(const Rect& rect, const uint32_t* argb);
    void flushIfFull();  // past FrameRecord::kFlushBytes: play the record now (a map's first frame, big writes)
    // Flip and WaitForVerticalBlank + Blt wait for vblank (when vsync is on); the loading bar's Blt shows at once
    // (spec §4.1), and so does Bink's.
    void present(const Rect& source, bool waitForVblank);

private:
    Context();
    // One log line every 30 seconds and one at close: frames shown and reads of the frame from the GPU. Reads
    // should stay near 0 while playing: the per-frame Lock must not cause any (Review Focus 1).
    void logStats(bool final);
    struct Stats {
        unsigned frames = 0;
        unsigned readBacks = 0;
        DWORD since = 0;
    };

    CRITICAL_SECTION cs_;
    Stats stats_;
    int scaledWidth_ = 0;  // the target size the last scale plan asked for (stage 3)
    int scaledHeight_ = 0;
    ToggleKey compareKey_;  // Pause or Scroll Lock: every improvement off and on, to compare
    bool enhanced_ = true;
};

// Holds the global lock for a scope; every DirectDraw / Direct3D method starts with one.
class Guard {
public:
    Guard() { Context::get().lock(); }
    ~Guard() { Context::get().unlock(); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
};

}  // namespace uagfx::win
