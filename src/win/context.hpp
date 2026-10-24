#pragma once
#include <windows.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "core/light_mode.hpp"
#include "core/map_load.hpp"
#include "core/glint.hpp"
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
#include "core/window_panel.hpp"
#include "win/console.hpp"
#include "win/fps_overlay.hpp"
#include "win/gpu/renderer.hpp"
#include "win/front_game.hpp"
#include "win/several_windows.hpp"
#include "win/window_switch.hpp"
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
    bool firstPicture_ = false;  // 2.12: the first picture logged (core/safe_mode: the start went well)
    bool vblankWaited = false;   // WaitForVerticalBlank came before the next Blt onto the primary
    bool gameFullscreen = false;  // the game asked for DDSCL_FULLSCREEN ([display] mode = auto follows it)
    Shadow shadow;

    bool open(std::string& error);          // the renderer, at DirectDrawCreateEx; false: use another DirectDraw
    void close();                           // the client released DirectDraw: the renderer goes with it
    bool setTarget(int width, int height);  // the renderer's target and the shadow, in the screen format
    void flush();                           // plays the record on the GPU
    // 3.0.3-beta.12: the game's EndScene - the windows' panel into its scene (the game's cursor still in the record,
    // moved over it), then the scene to the GPU
    void endScene();
    bool readTarget(std::vector<uint32_t>& argb);
    void writeTarget(const Rect& rect, const uint32_t* argb);
    void flushIfFull();  // past FrameRecord::kFlushBytes: play the record now (a map's first frame, big writes)
    // Flip and WaitForVerticalBlank + Blt wait for vblank (when vsync is on); the loading bar's Blt shows at once
    // (spec §4.1), and so does Bink's.
    void present(const Rect& source, bool waitForVblank);
    bool enhanced() const { return enhanced_; }  // not paused (Pause or Scroll Lock: the game's own picture)
    hit_flash::Flashes flashes;  // 0.96: where hits landed (win/hit_hook.cpp), the monsters there flash
    hit_flash::Monsters monsters;  // 2.7.10: the monsters drawn last frame (Device7::record), each hit given to one
    // 2.7.11, diagnostics (the tester on 2.7.10: every hit «no monster drawn there»): a hit no monster took - the quads
    // drawn about its place over the next two frames go into the log (Device7::record), what they are and why
    struct HitProbe {
        float x = 0, y = 0;
        int frames = 0, lines = 0;
    } hitProbe;
    // 2.7.14: the cards' sparkles of this frame (Device7::record), drawn over the world once its sprites are - before
    // the first interface draw, or at the present (drawSparkles)
    struct Sparkle {
        DrawSetup setup;
        glint::Vertex v[glint::kSparkle];
    };
    std::vector<Sparkle> sparkles;
    // 2.7.25: Ruwach's orbs' heads this frame (device7), each a small ball of light drawn with the sparkles, and the
    // last orb sprite's setup made adding and untextured
    glint::OrbHeads orbHeads;
    DrawSetup orbSetup;
    int orbSprites = 0, orbOwn = 0, orbLogs = 0;  // 2.7.26: a frame's Ruwach sprites, the raised; lines logged
    void drawSparkles();
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
    // 3.0.2-beta.12: a map's loading measured (watchMapLoad) - the frames over it, the bridge's times and cps.dll's
    // count at its start; 3.0.2-beta.13: its loading pictures and the game's frames apart, the ones shown, the picks
    map_load::Watch mapLoad_;
    SeveralWindows severalWindows_;  // 3.0.3-beta.2: the name in the title, each character's window at its place
    WindowSwitch windowSwitch_;      // 3.0.3-beta.6: the windows' list the games share, the keys that switch them
    panel::FrameSlot panelSlot_;     // 3.0.3-beta.12: the panel into the game's scene at its EndScene, once a frame
    void behind();                   // 3.0.3-beta.12: behind another window, by the clock - what the presents did
    ULONGLONG windowsAt_ = 0;  // 3.0.3-beta.7: checked by the clock (a light window draws 10 frames a second)
    bool checkingWindows_ = false;  // 3.0.3-beta.4: a present from the game's WM_MOVE handler meanwhile skips it
    FrontGame frontGame_;      // 3.0.3-beta.3: which running game is the main one
    bool mainWindow_ = true;   // this one, at the last updateLight
    double frameMs_ = 0.0;  // this present's time (QueryPerformanceCounter, ms)
    uint32_t loadPictures_ = 0, loadShown_ = 0, loadGameFrames_ = 0;
    double loadShownMs_ = 0.0;  // the last loading picture shown
    double loadSent_ = 0.0, loadWaited_ = 0.0;
    uint32_t loadInflated_[3] = {};
    bool loadInflatedKnown_ = false;
    unsigned loadPicks_ = 0, loadChecked_ = 0;
    double loadTheirsMs_ = 0.0;
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
    ToggleKey reportKey_;  // 2.13: Ctrl+Shift+F10 - the same, and «Нове звернення» with it
    ToggleKey xrayKey_;  // 2.3: Ctrl+Shift+F9, uaRO Studio's X-ray
    ToggleKey pickKey_, abKey_;  // 2.3: Ctrl+Shift+F8 the texture under the cursor, Ctrl+Shift+F7 its A/B
    uint8_t studioSent_ = 0xFF;  // 2.3: the studio's keys the renderer has
    bool fpsShown_ = false;
    // 0.76: [frames] keep_fps - the frames counted since keepSince_, slow windows in a row, the effects turned off for
    // this game; the effects' costs written for the settings app every half minute
    void keepFrameRate(const Config& config, HWND window);
    void watchMapLoad(const std::string& map, bool world, bool picture);  // 3.0.2-beta.12
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
