#pragma once
#include <windows.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "core/light_mode.hpp"
#include "core/appear.hpp"
#include "core/hit_flash.hpp"
#include "core/config.hpp"
#include "core/daylight.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/pixel.hpp"
#include "core/scale.hpp"
#include "core/throttle.hpp"
#include "core/toggle_key.hpp"
#include "core/usage.hpp"
#include "win/console.hpp"
#include "win/fps_overlay.hpp"
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
    // 2.2: the effects as drawn - the set and the groups made into the switches (effect_groups::effective). 2.5.11:
    // made once a change of `config` (settingsChanged) - each of the game's draws asks for them, and made anew each
    // time they were 2 % of the game's thread with Direct3D 11
    const EffectSettings& effects() const;
    void settingsChanged() { effectsMade_ = false; }  // `config` replaced
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
    bool enhanced() const { return enhanced_; }  // not paused (Pause or Scroll Lock: the game's own picture)
    appear::Tracker appear;  // 0.93: the sprites coming into view (Device7::record)
    hit_flash::Flashes flashes;  // 0.96: where hits landed (win/hit_hook.cpp), the monsters there flash
    double nowMs() const;    // a millisecond clock for the effects made as the game draws

private:
    Context();
    // One log line every 30 seconds and one at close: frames shown and reads of the frame from the GPU. Reads
    // should stay near 0 while playing: the per-frame Lock must not cause any (Review Focus 1).
    void logStats(bool final);
    // Stage 2 (spec §3.4): uaro-graphics.ini is looked at once a second while the game runs, and what applies now
    // is applied at once.
    void watchSettings();
    void noteWindow(bool minimized, bool foreground);  // a line when the game's window changes (0.10.1)
    struct Stats {
        unsigned frames = 0;
        unsigned readBacks = 0;
        DWORD since = 0;
        double cpuMs = -1.0;  // 2.5.15: the game's thread's processor time at the window's start (threadCpuMs)
    };

    CRITICAL_SECTION cs_;
    Stats stats_;
    int scaledWidth_ = 0;  // the target size the last scale plan asked for (stage 3)
    int scaledHeight_ = 0;
    int scaledSamples_ = 1;  // and its samples per pixel (stage 4)
    bool scaledUpscale_ = false;  // and whether DLSS draws the world (part 3)
    bool cameraKnown_ = false;    // the game's camera was read last frame (part 3, for the log)
    std::string map_;  // the map the game showed at the last present (AI textures)
    bool renderScaleNoted_ = false;  // the log has said render_scale is not used with DLSS
    ToggleKey compareKey_;  // Pause or Scroll Lock: every improvement off and on, to compare
    bool enhanced_ = true;
    mutable EffectSettings effects_;  // 2.5.11: effects(), made from `config`
    mutable bool effectsMade_ = false;
    // 2.5.20: the light modes (core/light_mode.hpp) - what they take from `config` now, and the laptop's power
    light::Overrides light_;
    bool onBattery_ = false;
    ULONGLONG powerChecked_ = 0;
    void updateLight();
    ToggleKey fpsKey_;  // 0.54: Ctrl+Shift+F11 - the frame counter
    // 2.2.1: the statistics of the settings played with, with the player's leave ([stats] send; core/usage.hpp)
    usage::Recorder usage_;
    usage::Setup usageSetup_;
    ULONGLONG usageLast_ = 0, usageSetupAt_ = 0;
    uint32_t usageGpuSerial_ = 0;
    bool usageSafe_ = false;  // 2.2.2: the drawing process made shaders without their newest features
    void noteUsage(bool foreground, long outWidth, long outHeight, const GpuTiming* gpu);
    void writeUsage(const std::string& lines);
    ToggleKey shotKey_;  // 0.54: Ctrl+Shift+F12 - a screenshot at the picture's full size
    ToggleKey xrayKey_;  // 2.3: Ctrl+Shift+F9, uaRO Studio's X-ray
    ToggleKey pickKey_, abKey_;  // 2.3: Ctrl+Shift+F8 the texture under the cursor, Ctrl+Shift+F7 its A/B
    uint8_t studioSent_ = 0xFF;  // 2.3: the studio's keys the renderer has
    bool fpsShown_ = false;
    // 0.76: [frames] keep_fps - the frames counted since keepSince_, slow windows in a row, the effects turned off for
    // this game; the effects' costs written for the settings app every half minute
    void keepFrameRate(const Config& config, HWND window);
    uint64_t keepSince_ = 0;
    unsigned keepFrames_ = 0;
    int keepSlow_ = 0;
    std::set<std::string> keepOff_;
    uint64_t costsWritten_ = 0;
    FpsOverlay fps_;
    Console console_;               // 1.9.3: ~ - the effects' switches over the game
    std::string consoleOff_;        // the effect the console holds off, and the settings without it
    EffectSettings compareFx_;
    daylight::ServerClock serverClock_;  // 2.6.19: the Clock's «От сервера»
    Throttle scaleLog_{500};  // the scale line while the window is being resized (stage 3 review M7)
    SIZE lastOut_{};
    ScalePlan lastPlan_;
    unsigned totalReads_ = 0;  // since start, in the scale line: the start-up calibration came first (review M4)
    std::string scaleLine_;    // the latest scale line: written now, by the throttle later, or at close
    bool scaleMade_ = true;    // whether setScale made that target (0.4.1 review)
    uint64_t settingsChecked_ = 0;  // GetTickCount64 of the last look at uaro-graphics.ini
    bool settingsKnown_ = false;    // its write time and size below are those of the text read last
    FILETIME settingsTime_{};
    uint64_t settingsSize_ = 0;
    bool minimized_ = false;  // the game's window, as the last present saw it
    bool foreground_ = true;
    uint64_t minimizedAt_ = 0;
    uint64_t behindAt_ = 0;
    int windowLines_ = 0;
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
