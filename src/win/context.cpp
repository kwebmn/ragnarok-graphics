#include "win/context.hpp"

#include <cstdio>

#include "core/live_config.hpp"
#include "core/scale.hpp"
#include "win/crisp/crisp.hpp"
#include "win/dpi.hpp"
#include "win/effect_hook.hpp"
#include "win/game_camera.hpp"
#include "win/sprite_hook.hpp"
#include "win/start_screen.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"
#include "win/window_mode.hpp"

namespace uagfx::win {
namespace {

// The window in front, for the log: its class and its program (no title: it may name the player).
std::string foregroundName() {
    const HWND fg = GetForegroundWindow();
    if (!fg) return "no window";
    char cls[64] = "";
    GetClassNameA(fg, cls, sizeof cls);
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    std::string exe = "an unknown program";
    using QueryName = BOOL(WINAPI*)(HANDLE, DWORD, LPSTR, PDWORD);
    static const auto query = reinterpret_cast<QueryName>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "QueryFullProcessImageNameA")));
    if (HANDLE p = query ? OpenProcess(0x1000 /* PROCESS_QUERY_LIMITED_INFORMATION */, FALSE, pid) : nullptr) {
        char path[MAX_PATH];
        DWORD n = MAX_PATH;
        if (query(p, 0, path, &n)) {
            exe = std::string(path, n);
            const size_t slash = exe.find_last_of("\\/");
            if (slash != std::string::npos) exe = exe.substr(slash + 1);
        }
        CloseHandle(p);
    }
    return std::string("\"") + cls + "\" of " + exe;
}

}  // namespace

Context& Context::get() {
    static Context c;
    return c;
}

Context::Context() {
    InitializeCriticalSection(&cs_);
    screenWidth = GetSystemMetrics(SM_CXSCREEN);  // what a windowed client sees until SetDisplayMode
    screenHeight = GetSystemMetrics(SM_CYSCREEN);
}

bool Context::open(std::string& error) {
    if (renderer) return true;
    // 0.40: what the start does, over the game's black window, until the renderer is ready (or is not).
    openStart(findGameWindow());
    struct CloseStart {
        ~CloseStart() { closeStart(); }
    } closeStartAtEnd;
    showStart(start::Progress{start::Stage::Renderer, 0, 0});
    gpu::Renderer* r = new gpu::Renderer;
    r->setStartSink([](void*, const start::Progress& p) { showStart(p); }, nullptr);
    bool remote = false;
    // 0.46: Direct3D 11 (graphics cards without Direct3D 12) - none of what needs Direct3D 12 or the 64-bit process.
    const bool d3d11 = startup().choice.api == Api::D3d11;
    if (d3d11) {
        std::string why;
        if (!r->createD3D11(gameDir() + "\\uaro-graphics\\shaders-d3d11.bin", why)) {
            error = "Direct3D 11: " + why;
            delete r;
            return false;
        }
        remote = true;  // made: no other renderer
        if (config.remix || config.bridge || config.dlss != "off" || config.ai.enabled)
            LOG_INFO("Direct3D 11: RTX Remix, the 64-bit renderer, DLSS and the AI textures need Direct3D 12 - off");
        if (drawnByRenderer(config.effects))  // 0.46.1
            LOG_INFO("Direct3D 11: the Effects page's lights, torches, water, shadows, weather and glow need "
                     "Direct3D 12 - the game's own effects shown; the level 99 aura and the skills stay softer");
    } else if (config.remix) {  // part 4 (test): Direct3D 9 for RTX Remix, its d3d9.dll next to uaRO.exe if there
        const std::string local = gameDir() + "\\d3d9.dll";
        const std::string dll = fileExists(local) ? local : systemDir() + "\\d3d9.dll";
        std::string why;
        remote = r->createRemix(dll, findGameWindow(), why);  // on the game's thread: DirectDrawCreateEx
        if (remote)
            LOG_INFO("remix: drawing through %s (Direct3D 9 fixed function, the world in world space)", dll.c_str());
        else
            LOG_ERROR("remix: %s; drawing with Direct3D 12", why.c_str());
    }
    if (config.frameGeneration && config.dlss == "off")
        LOG_INFO("frame generation %dx: off - it needs NVIDIA DLSS on (Smoothing page)", config.frameGeneration);
    // Part 3: the 64-bit renderer - DLSS and the AI textures need it.
    if (!remote && (config.bridge || config.dlss != "off" || config.ai.enabled)) {
        gpu::BridgeOptions o;
        o.hostExe = gameDir() + "\\uaro-graphics\\uaro-render64.exe";
        o.gameDir = gameDir();
        o.present = config.bridgePresent;
        o.dpiScale = config.bridgeDpiScale;
        o.logLevel = int(config.logLevel);
        o.dlss = config.dlss;
        o.dlssJitterInverted = config.dlssJitterInverted;
        o.dlssMotionInverted = config.dlssMotionInverted;
        o.frameGeneration = config.dlss != "off" ? config.frameGeneration : 0;  // it uses DLSS's depth and motion
        o.ai = config.ai;
        std::string why;
        remote = r->createRemote(o, why);
        if (remote)
            LOG_INFO("bridge: drawing in uaro-render64.exe (64-bit), pictures: %s", o.present.c_str());
        else
            LOG_ERROR("bridge: %s; drawing in the game's process", why.c_str());
    }
    r->setShaderCache(gameDir() + "\\uaro-graphics\\shaders-32.bin");  // 0.39.5
    if (!remote && !r->create(error)) {
        delete r;
        return false;
    }
    renderer = r;
    renderer->setUiSharp(config.uiFilter == UiFilter::Sharp);
    renderer->preparePipelines(gameDir() + "\\uaro-graphics-pipelines.txt");
    LOG_INFO("%s renderer on %s", d3d11 ? "Direct3D 11" : "Direct3D 12", renderer->adapterName().c_str());
    crisp::setWindowArt(config.windowArt);  // 0.41: before the first twin
    crisp::install(config.crispText);
    // AI textures (0.13): sprites through the game's sprite cache, cell by cell (sprite_hook.hpp); (0.14) the windows'
    // icons through crisp text's twins. 0.23: the torches' frames are known through it too - whenever the picture is
    // ours (not RTX Remix's), as the torches can be turned on in a running game.
    // 0.46.1: Direct3D 11 draws neither of them.
    const AiSettings& ai = config.ai;
    if (!d3d11 && ((ai.enabled && (ai.sprites() || ai.effects || ai.icons) && renderer->remote()) || !config.remix))
        installSpriteHook();
    installEffectHook();  // 0.27: the level 99 aura softer (Device7::record)
    renderer->setIconSink(&crisp::iconReady, nullptr);
    renderer->setGameDir(gameDir());  // 0.25: the maps' files (uaro-render64.exe has the folder already)
    crisp::setAiIcons(config.ai.enabled && config.ai.icons && renderer->remote() && config.crispText,
                      config.ai.uiPieces);
    return true;
}

void Context::close() {
    flush();
    if (scaleLog_.pending()) LOG_INFO("%s", scaleLine_.c_str());  // the last change, before the final stats
    logStats(true);
    crisp::uninstall();  // the game's methods back; the twins' textures go before the renderer
    delete renderer;     // waits for the GPU
    renderer = nullptr;
    record.reset();
    window = nullptr;
    windowApplied = false;
    scaleLog_ = Throttle(500);
    scaledWidth_ = scaledHeight_ = 0;
    scaledSamples_ = 1;
    scaledUpscale_ = false;
    scaleMade_ = true;
    lastOut_ = SIZE{};
    lastPlan_ = ScalePlan{};
}

bool Context::setTarget(int width, int height) {
    if (renderer) renderer->setWindow(window);
    if (!renderer || !renderer->setTarget(width, height)) return false;
    scaledWidth_ = width;  // the renderer is back at the game's size until the next present
    scaledSamples_ = 1;    // and at one sample per pixel
    scaledUpscale_ = false;
    scaledHeight_ = height;
    scaleMade_ = true;     // that target exists (0.4.2 review)
    record.reset();
    installShadowHandler();
    if (!shadow.resize(width, height, screenFormat)) {
        LOG_ERROR("cannot allocate the %dx%d CPU copy of the target", width, height);
        return false;
    }
    return true;
}

void Context::flush() {
    if (!renderer || record.empty()) return;
    // Also when execute throws (out of memory): played again, the record's draws would show twice (0.5.2 review 2c).
    struct Done {
        Context& c;
        ~Done() {
            c.record.reset();
            c.shadow.gpuChanged();
        }
    } done{*this};
    // Part 4: the world goes back into world space with the camera of this frame; 0.19: the modern water draws its
    // waves and reflections in the world, with the same camera; 0.23, 0.24: the torches' flicker and the smoke's
    // wisps keep to their places in the world with it; 0.25, 0.28: the map file's lamps and fireflies stand in it.
    // 0.46.1: Direct3D 11 draws none of them.
    const EffectSettings& fx = config.effects;
    const bool world = fx.water || fx.torch || fx.smoke || fx.lamps || fx.fireflies || fx.mapGlows || fx.rays ||
                       fx.lava || fx.actorShadows || fx.clouds || fx.haze || fx.heat || fx.portals || fx.weather;
    if (renderer->remix() || (enhanced_ && world && !renderer->d3d11())) {
        float view[12];
        const bool known = readGameCamera(view);
        renderer->setCamera(known, view);
    }
    renderer->execute(record);
}

bool Context::readTarget(std::vector<uint32_t>& argb) {
    flush();
    ++stats_.readBacks;
    ++totalReads_;
    return renderer && renderer->readTarget(argb);
}

void Context::writeTarget(const Rect& rect, const uint32_t* argb) {
    record.writeTarget(rect, argb);
    flushIfFull();
}

void Context::flushIfFull() {
    if (record.full()) flush();
}

void Context::present(const Rect& source, bool waitForVblank) {
    flush();
    effectFrame();  // 0.27: the effects' primitives of the next frame
    if (!renderer) return;
    watchSettings();
    if (window && IsIconic(window)) {  // minimised: nothing to show, and no vsync to hold the game's loop back
        noteWindow(true, false);
        Sleep(15);
        return;
    }
    const bool foreground = !window || GetForegroundWindow() == window;
    noteWindow(false, foreground);
    if (!windowApplied && window) {
        // Not earlier: a windowed client sizes its back buffer from the window's client area (L3039), and that
        // logical size must stay the one from its settings, whatever the window becomes. The flag goes first:
        // SetWindowPos sends WM_MOVE, and the client may present again from its handler (L457929).
        windowApplied = true;
        applyWindowMode(window, config, gameFullscreen, shadow.width(), shadow.height());
    }
    // Pause or Scroll Lock in the game window: every improvement off - the game's size stretched like Windows does,
    // no interface filter, no sharpening - and on again, to compare without a restart.
    const bool keyDown = window && GetForegroundWindow() == window &&
                         (GetAsyncKeyState(VK_PAUSE) < 0 || GetAsyncKeyState(VK_SCROLL) < 0);
    if (compareKey_.pressed(keyDown)) {
        enhanced_ = !enhanced_;
        LOG_INFO("compare key: improvements %s", enhanced_ ? "on" : "off");
    }
    // Stage 3: the window's real pixels and the scale for them. The first present switches from the game's size:
    // the start-up calibration has run by then (spec §4.3).
    const SIZE out = window ? clientPixels(window, config.highResolution) : SIZE{shadow.width(), shadow.height()};
    // Part 3: DLSS draws the world at its own size and shows it at the picture's - no supersampling on top of it
    // (DLAA at 150% would draw 2.25 times the pixels for nothing).
    const bool dlss = renderer->upscales() && config.dlss != "off";
    const int renderScale = dlss ? 100 : config.renderScale;
    if (dlss && config.renderScale > 100 && !renderScaleNoted_) {
        renderScaleNoted_ = true;
        LOG_INFO("render_scale %d%% is not used with DLSS: it draws the world at its own size", config.renderScale);
    }
    const ScalePlan plan = planScale(shadow.width(), shadow.height(), out.cx, out.cy, renderScale,
                                     config.highResolution && enhanced_);
    // Stage 6: twins follow the scale with every improvement on, so Pause leaves them as they are.
    crisp::setScale(planScale(shadow.width(), shadow.height(), out.cx, out.cy, renderScale, config.highResolution).k);
    // Stage 4: MSAA comes with the scale, so the calibration has drawn without it; Pause turns it off too.
    // Part 3: DLSS (in the 64-bit renderer) draws the world instead of MSAA; Pause turns it off as well. Without
    // DLSS in the renderer (no RTX card, no files) MSAA stays (0.8 review M4).
    const bool upscale = enhanced_ && dlss;
    const int samples = enhanced_ && !upscale && config.msaa > 1 ? config.msaa : 1;
    if (plan.targetWidth != scaledWidth_ || plan.targetHeight != scaledHeight_ || samples != scaledSamples_ ||
        upscale != scaledUpscale_) {
        scaledWidth_ = plan.targetWidth;  // also when it fails: no new attempt every frame
        scaledHeight_ = plan.targetHeight;
        scaledSamples_ = samples;
        scaledUpscale_ = upscale;
        scaleMade_ = renderer->setScale(plan.k, plan.targetWidth, plan.targetHeight, samples, upscale);
    }
    if (upscale) {  // the camera of the frame being shown: the motion of its pixels
        float view[12];
        const bool known = readGameCamera(view);
        renderer->setCamera(known, view);
        if (known != cameraKnown_) {
            cameraKnown_ = known;
            LOG_INFO("DLSS: the game's camera %s", known ? "found" : "not in view (no world shown, or another exe)");
        }
    }
    // AI textures: upscaled.grf notes the maps textures are seen on; 0.25: the map's file - its lamps and the torches
    // the game does not show.
    if ((config.ai.enabled && renderer->remote()) || !config.remix) {
        const std::string map = readGameMap();
        if (map != map_) {
            map_ = map;
            renderer->setMap(map);
            if (!map.empty()) LOG_INFO("map: %s", map.c_str());
        }
    }
    renderer->setDrawQuality(enhanced_ ? config.anisotropy : 0, enhanced_ && config.alphaToCoverage, enhanced_);
    renderer->setEffects(enhanced_ ? config.effects : effectsOff());  // Pause: the game's own picture
    // A line per change of the picture's geometry; while the window is being resized at most two a second and the
    // final state once it settles (stage 3 review M7); it says when the target could not be made (0.4.1 review).
    const bool changed = out.cx != lastOut_.cx || out.cy != lastOut_.cy || plan.k != lastPlan_.k ||
                         plan.targetWidth != lastPlan_.targetWidth || plan.targetHeight != lastPlan_.targetHeight ||
                         !(plan.picture == lastPlan_.picture);
    lastOut_ = out;
    lastPlan_ = plan;
    if (changed) {
        char line[320];
        std::snprintf(line, sizeof line,
                      "scale: game %dx%d, output %ldx%ld, s %.3f, r %.2f, k %.3f, target %dx%d%s, picture %dx%d at "
                      "%d,%d, %u reads of the frame so far",
                      shadow.width(), shadow.height(), out.cx, out.cy, plan.s, plan.r, plan.k, plan.targetWidth,
                      plan.targetHeight, scaleMade_ ? "" : " not made", plan.picture.width(), plan.picture.height(),
                      plan.picture.left, plan.picture.top, totalReads_);
        scaleLine_ = line;
    }
    const uint64_t now = GetTickCount64();
    if (changed ? scaleLog_.changed(now) : scaleLog_.due(now)) LOG_INFO("%s", scaleLine_.c_str());
    gpu::PresentOptions o;
    o.vsync = config.vsync && waitForVblank;
    o.allowTearing = config.allowTearing;
    o.fpsLimit = config.fpsLimit;
    o.sharpen = enhanced_ && config.sharpen > 0 ? float(config.sharpen - 1) / 99.0f : -1.0f;  // 1-100 -> CAS 0..1
    o.foreground = foreground;
    o.vsyncSetting = config.vsync;
    renderer->present(window, out.cx, out.cy, source, o);
    ++stats_.frames;
    logStats(false);
}

void Context::watchSettings() {
    const uint64_t now = GetTickCount64();
    if (settingsChecked_ && now - settingsChecked_ < 1000) return;
    settingsChecked_ = now;
    const std::string path = gameDir() + "\\uaro-graphics.ini";
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a)) return;  // no file: the defaults stay
    const uint64_t size = (uint64_t(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
    if (settingsKnown_ && CompareFileTime(&a.ftLastWriteTime, &settingsTime_) == 0 && size == settingsSize_) return;
    std::string text;
    if (!readTextFile(path, text)) return;  // the settings app is replacing it right now: the next look reads it
    settingsKnown_ = true;
    settingsTime_ = a.ftLastWriteTime;
    settingsSize_ = size;
    const LiveUpdate u = liveUpdate(config, parseConfig(text));
    if (u.applied.empty() && u.waiting.empty()) return;
    const Config was = config;
    config = u.next;  // render_scale, high_resolution, sharpen, vsync and fps_limit are read at every present
    if (config.uiFilter != was.uiFilter) renderer->setUiSharp(config.uiFilter == UiFilter::Sharp);
    if (config.crispText != was.crispText) {
        if (config.crispText) {
            crisp::install(true);
        } else {
            crisp::uninstall();
            LOG_INFO("crisp text: off (crisp_text = 0)");  // as at a start without it: the Diagnostics page reads it
        }
    }
    if (config.windowArt != was.windowArt) crisp::setWindowArt(config.windowArt);  // 0.41: the windows redrawn
    if (config.logLevel != was.logLevel) wlog::setLevel(config.logLevel);
    std::string line = "settings: uaro-graphics.ini changed:";
    for (size_t i = 0; i < u.applied.size(); ++i) line += (i ? ", " : " ") + u.applied[i];
    if (!u.waiting.empty()) line += u.applied.empty() ? " at the next start:" : "; at the next start:";
    for (size_t i = 0; i < u.waiting.size(); ++i) line += (i ? ", " : " ") + u.waiting[i];
    LOG_INFO("%s", line.c_str());
}

// 0.10 logs: with frame generation on the game stood still twice for seconds - minimised, it turned out, and the log
// could not say why. Now it says when the window goes behind another or is minimised, which window took the front,
// and when it is back.
void Context::noteWindow(bool minimized, bool foreground) {
    const uint64_t now = GetTickCount64();
    std::string line;
    if (minimized != minimized_) {
        minimized_ = minimized;
        if (minimized) {
            minimizedAt_ = now;
            line = "the game's window is minimized, nothing is drawn (in front: " + foregroundName() + ")";
        } else {
            char text[96];
            std::snprintf(text, sizeof text, "the game's window is back after %.1f s",
                          double(now - minimizedAt_) / 1000.0);
            line = text;
        }
    } else if (!minimized && foreground != foreground_) {
        foreground_ = foreground;
        if (!foreground) {
            behindAt_ = now;
            line = "the game's window is behind " + foregroundName();
        } else {
            char text[96];
            std::snprintf(text, sizeof text, "the game's window is in front again after %.1f s",
                          double(now - behindAt_) / 1000.0);
            line = text;
        }
    }
    if (line.empty() || windowLines_ > 40) return;
    ++windowLines_;
    LOG_INFO("%s%s", line.c_str(), windowLines_ > 40 ? " (its changes are not logged any more)" : "");
}

void Context::logStats(bool final) {
    const DWORD now = GetTickCount();
    if (!stats_.since) stats_.since = now;
    const DWORD ms = now - stats_.since;
    if (!final && ms < 30000) return;
    if (stats_.frames || stats_.readBacks)
        LOG_INFO("%u frames in %.1f s (%.1f fps), %u reads of the frame from the GPU%s", stats_.frames, ms / 1000.0,
                 ms ? stats_.frames * 1000.0 / ms : 0.0, stats_.readBacks, crisp::stats().c_str());
    stats_ = Stats{};
    stats_.since = now;
}

}  // namespace uagfx::win
