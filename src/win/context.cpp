#include "win/context.hpp"

#include <cstdio>
#include <fstream>

#include "core/effect_costs.hpp"
#include "core/live_config.hpp"
#include "core/scale.hpp"
#include "win/crisp/crisp.hpp"
#include "win/dpi.hpp"
#include "win/effect_hook.hpp"
#include "win/hit_hook.hpp"
#include "win/pick_hook.hpp"
#include "win/sound_hook.hpp"
#include "win/gpu/adapters.hpp"
#include "win/game_camera.hpp"
#include "win/sprite_hook.hpp"
#include "win/start_screen.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/profiler.hpp"
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
        if (config.remix || config.bridge || config.dlss != "off")
            LOG_INFO("Direct3D 11: RTX Remix, the 64-bit renderer and DLSS need Direct3D 12 - off");
        if (drawnByRenderer(config.effects))  // 0.46.1
            LOG_INFO("Direct3D 11: the Effects page's colours, the characters' shadows, the torches' light, (0.99) "
                     "the water and (1.0) the glow are drawn; the lamps, weather and the rest need Direct3D 12 - the "
                     "game's own shown there");
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
    gpu::setChosenAdapter(config.gpu);  // 0.50.2
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
        o.gpu = config.gpu;
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
    renderer->setStateCache(config.stateCache);  // 0.88
    if (!config.stateCache) LOG_INFO("Direct3D 11: every state set again each draw ([debug] state_cache = 0)");
    renderer->setMergeDraws(config.mergeDraws);  // 0.90
    if (!config.mergeDraws && d3d11)
        LOG_INFO("Direct3D 11: each of the game's draws by itself ([debug] merge_draws = 0)");
    renderer->preparePipelines(gameDir() + "\\uaro-graphics-pipelines.txt");
    LOG_INFO("%s renderer on %s", d3d11 ? "Direct3D 11" : "Direct3D 12", renderer->adapterName().c_str());
    fpsShown_ = config.fpsCounter;  // 0.54
    fps_.setLabel(std::string(d3d11 ? "Direct3D 11" : "Direct3D 12") + (config.dlss != "off" ? " + DLSS" : ""));
    // 0.47: the AI textures with Direct3D 11 - the texture pack's pictures, in the game's process.
    const bool aiHere = d3d11 && config.ai.enabled && renderer->startAiTextures(gameDir(), config.ai);
    crisp::setWindowArt(config.windowArt);  // 0.41: before the first twin
    crisp::install(config.crispText);
    // AI textures (0.13): sprites through the game's sprite cache, cell by cell (sprite_hook.hpp); (0.14) the windows'
    // icons through crisp text's twins. 0.23: the torches' frames are known through it too - whenever the picture is
    // ours (not RTX Remix's), as the torches can be turned on in a running game.
    // 0.46.1: Direct3D 11 draws no torches; 0.47: its AI textures take the sprite frames when they are on.
    const AiSettings& ai = config.ai;
    const bool cells = ai.enabled && (ai.sprites() || ai.effects || ai.icons);
    // 0.61: Direct3D 11 draws the characters' shadows and the torches' light now: they know the sprites by it too.
    const bool d3d11Hook = (aiHere && cells) || config.effects.actorShadows || config.effects.torch ||
                           config.effects.appear || config.effects.glint ||
                           config.effects.hitFlash;  // 0.93, 0.95, 0.96: the sprites' kinds
    if (d3d11 ? d3d11Hook : ((cells && renderer->remote()) || !config.remix)) installSpriteHook();
    installEffectHook();  // 0.27: the level 99 aura softer (Device7::record)
    installPickHook(config.fastPick);  // 0.89: the cell under the mouse without testing the whole map
    installHitHook();                  // 0.96: the hits' sounds (the flash itself is effects.hit_flash)
    renderer->setIconSink(&crisp::iconReady, nullptr);
    renderer->setGameDir(gameDir());  // 0.25: the maps' files (uaro-render64.exe has the folder already)
    crisp::setAiIcons(config.ai.enabled && config.ai.icons && (renderer->remote() || aiHere) && config.crispText,
                      config.ai.uiPieces);
    return true;
}

void Context::close() {
    Profiler::get().stop();  // 0.87
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

double Context::nowMs() const {
    static LARGE_INTEGER freq{};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return double(t.QuadPart) * 1000.0 / double(freq.QuadPart);
}

void Context::flushIfFull() {
    if (record.full()) flush();
}

void Context::present(const Rect& source, bool waitForVblank) {
    flush();
    effectFrame();  // 0.27: the effects' primitives of the next frame
    {
        static double last = 0;  // 0.93: the sprites drawn are the next frame's known ones
        const double now = nowMs();
        appear.frame(last ? now - last : 1e9);
        last = now;
    }
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
    // 0.54: Ctrl+Shift+F11 - the frame counter
    const bool fpsDown = window && GetForegroundWindow() == window && GetAsyncKeyState(VK_CONTROL) < 0 &&
                         GetAsyncKeyState(VK_SHIFT) < 0 && GetAsyncKeyState(VK_F11) < 0;
    if (fpsKey_.pressed(fpsDown)) fpsShown_ = !fpsShown_;
    // 0.54: Ctrl+Shift+F12 - the picture as shown (4K, not the game's 1920x1080) into the game's ScreenShot folder
    const bool shotDown = window && GetForegroundWindow() == window && GetAsyncKeyState(VK_CONTROL) < 0 &&
                          GetAsyncKeyState(VK_SHIFT) < 0 && GetAsyncKeyState(VK_F12) < 0;
    std::string screenshot;
    if (shotKey_.pressed(shotDown)) {
        const std::string folder = gameDir() + "\\ScreenShot";
        CreateDirectoryA(folder.c_str(), nullptr);
        SYSTEMTIME t{};
        GetLocalTime(&t);
        char name[64];
        std::snprintf(name, sizeof name, "\\uaro-%04u%02u%02u-%02u%02u%02u.png", t.wYear, t.wMonth, t.wDay, t.wHour,
                      t.wMinute, t.wSecond);
        screenshot = folder + name;
    }
    // Stage 3: the window's real pixels and the scale for them. The first present switches from the game's size:
    // the start-up calibration has run by then (spec §4.3).
    const SIZE out = window ? clientPixels(window, config.highResolution) : SIZE{shadow.width(), shadow.height()};
    // Part 3: DLSS draws the world at its own size and shows it at the picture's - no supersampling on top of it
    // (DLAA at 150% would draw 2.25 times the pixels for nothing).
    const bool dlss = renderer->upscales() && config.dlss != "off";
    // 0.62: [display] ui_native - supersampling the world alone (Direct3D 12 in this process, no DLSS): the target the
    // picture's size, the world render_scale times it, made smaller into it; the interface at the picture's pixels.
    const bool nativeWanted = !dlss && config.uiNative && config.highResolution && config.renderScale > 100 &&
                              renderer->canSupersample();
    const bool uiNative =
        nativeWanted && renderer->setSupersample(enhanced_ ? config.renderScale / 100.0 : 1.0) && enhanced_;
    if (!nativeWanted) renderer->setSupersample(1.0);  // turned off in a running game
    // 0.75: [quality] taa - the renderer's own, where neither DLSS nor ui_native is used
    const bool taaWanted = !dlss && !nativeWanted && config.taa && renderer->canSupersample();
    const bool taa = taaWanted && renderer->setTaa(enhanced_) && enhanced_;
    if (!taaWanted) renderer->setTaa(false);
    const int renderScale = dlss || uiNative || taa ? 100 : config.renderScale;
    if (dlss && config.renderScale > 100 && !renderScaleNoted_) {
        renderScaleNoted_ = true;
        LOG_INFO("render_scale %d%% is not used with DLSS: it draws the world at its own size", config.renderScale);
    }
    const ScalePlan plan = planScale(shadow.width(), shadow.height(), out.cx, out.cy, renderScale,
                                     config.highResolution && enhanced_);
    // Stage 6: twins follow the scale with every improvement on, so Pause leaves them as they are.
    crisp::setScale(planScale(shadow.width(), shadow.height(), out.cx, out.cy, nativeWanted ? 100 : renderScale,
                              config.highResolution)
                        .k);
    // Stage 4: MSAA comes with the scale, so the calibration has drawn without it; Pause turns it off too.
    // Part 3: DLSS (in the 64-bit renderer) draws the world instead of MSAA; Pause turns it off as well. Without
    // DLSS in the renderer (no RTX card, no files) MSAA stays (0.8 review M4).
    const bool upscale = enhanced_ && (dlss || uiNative || taa);
    const int samples = enhanced_ && !upscale && config.msaa > 1 ? config.msaa : 1;
    if (plan.targetWidth != scaledWidth_ || plan.targetHeight != scaledHeight_ || samples != scaledSamples_ ||
        upscale != scaledUpscale_) {
        scaledWidth_ = plan.targetWidth;  // also when it fails: no new attempt every frame
        scaledHeight_ = plan.targetHeight;
        scaledSamples_ = samples;
        scaledUpscale_ = upscale;
        scaleMade_ = renderer->setScale(plan.k, plan.targetWidth, plan.targetHeight, samples, upscale);
    }
    if (upscale && (dlss || taa)) {  // the camera of the frame being shown: the motion of its pixels
        float view[12];
        const bool known = readGameCamera(view);
        renderer->setCamera(known, view);
        if (known != cameraKnown_) {
            cameraKnown_ = known;
            LOG_INFO("%s: the game's camera %s", taa ? "TAA" : "DLSS",
                     known ? "found" : "not in view (no world shown, or another exe)");
        }
    }
    // AI textures: the pack's index notes the maps textures are seen on (making them); 0.25: the map's file - its lamps
    // and the torches the game does not show.
    if ((config.ai.enabled && renderer->remote()) || !config.remix) {
        const std::string map = readGameMap();
        if (map != map_) {
            map_ = map;
            renderer->setMap(map);
            if (!map.empty()) LOG_INFO("map: %s", map.c_str());
        }
    }
    renderer->setDrawQuality(enhanced_ ? config.anisotropy : 0, enhanced_ && config.alphaToCoverage, enhanced_);
    keepFrameRate(config, window);
    Profiler::get().run(GetCurrentThreadId(), config.profile);  // 0.87: [debug] profile
    EffectSettings fx = config.effects;  // 0.63: how much night now, by the clock; 0.74: the minute of the day
    if (fx.night || fx.dayCycle) {
        SYSTEMTIME t{};
        GetLocalTime(&t);
        fx.nightNow = nightAmount(fx, t.wHour, t.wMinute);
        if (fx.dayCycle) fx.dayNow = t.wHour * 60 + t.wMinute;
    }
    for (const std::string& k : keepOff_) effect_costs::drop(fx, k);  // 0.76
    renderer->setEffects(enhanced_ ? fx : effectsOff());  // Pause: the game's own picture
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
    // 0.85, [frames] save_power: in the background (another window in front, minimised) 15 frames a second, with no
    // map shown (the login, the character select) 30 - what the player is not looking at needs no more.
    o.fpsLimit = config.fpsLimit;
    const int idle = !foreground ? 15 : map_.empty() ? 30 : 0;
    if (config.savePower && idle && (o.fpsLimit <= 0 || o.fpsLimit > idle)) o.fpsLimit = idle;
    o.sharpen = enhanced_ && config.sharpen > 0 ? float(config.sharpen - 1) / 99.0f : -1.0f;  // 1-100 -> CAS 0..1
    o.foreground = foreground;
    o.vsyncSetting = config.vsync;
    o.screenshot = screenshot;
    o.hdr = config.hdr && enhanced_;  // 0.65: Pause shows the SDR picture
    soundFrame(foreground);  // 0.91: quieter in the background; the crowd's limiter thinned out
    renderer->present(window, out.cx, out.cy, source, o);
    fps_.present(window, fpsShown_);
    ++stats_.frames;
    logStats(false);
}

// 0.76: every half minute the effects' costs into uaro-graphics\effect-costs.txt (the settings app shows them);
// [frames] keep_fps - the frames counted in five-second windows while the game is in front with every improvement on;
// two slow windows in a row (under 90 % of it) turn the costliest effect off for the rest of the game.
void Context::keepFrameRate(const Config& config, HWND window) {
    const uint64_t now = GetTickCount64();
    effect_costs::Costs costs;
    const bool measured = renderer && renderer->effectCosts(costs);
    if (measured && now - costsWritten_ >= 30000) {
        costsWritten_ = now;
        std::ofstream f(gameDir() + "\\uaro-graphics\\effect-costs.txt", std::ios::binary | std::ios::trunc);
        f << effect_costs::format(costs);
    }
    if (config.keepFps <= 0) {
        keepOff_.clear();  // turned off: the effects come back
        keepSince_ = 0;
        return;
    }
    if (!enhanced_ || !window || GetForegroundWindow() != window || IsIconic(window)) {
        keepSince_ = 0;  // a frame rate in the background says nothing
        return;
    }
    if (!keepSince_) {
        keepSince_ = now;
        keepFrames_ = 0;
        return;
    }
    ++keepFrames_;
    if (now - keepSince_ < 5000) return;
    const double fps = double(keepFrames_) * 1000.0 / double(now - keepSince_);
    keepSince_ = now;
    keepFrames_ = 0;
    keepSlow_ = fps < 0.9 * double(config.keepFps) ? keepSlow_ + 1 : 0;
    if (keepSlow_ < 2) return;
    keepSlow_ = 0;
    EffectSettings fx = config.effects;
    for (const std::string& k : keepOff_) effect_costs::drop(fx, k);
    const std::string key = effect_costs::pickToDrop(fx, measured ? costs : effect_costs::Costs{}, keepOff_,
                                                     startup().choice.api == Api::D3d11);
    if (key.empty()) return;
    keepOff_.insert(key);
    const double ms = effect_costs::costOf(costs, key);
    char cost[48] = "";
    if (ms >= 0) std::snprintf(cost, sizeof cost, " (it cost %.2f ms)", ms);
    LOG_INFO("keep_fps: %.0f fps, under %d - effects.%s off for this game%s", fps, config.keepFps, key.c_str(), cost);
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
