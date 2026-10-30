#include "win/context.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include "core/effect_costs.hpp"
#include "core/effect_exe.hpp"
#include "core/effect_groups.hpp"
#include "core/first_game.hpp"
#include "core/layout.hpp"
#include "core/live_config.hpp"
#include "core/outdoors.hpp"
#include "core/scale.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/schema.hpp"
#include "core/texture_levels.hpp"
#include "core/ui_motion.hpp"
#include "core/ui_style.hpp"
#include "core/version.hpp"
#include "win/blind_patch.hpp"
#include "win/chat_patch.hpp"
#include "win/crisp/crisp.hpp"
#include "win/dpi.hpp"
#include "win/effect_hook.hpp"
#include "win/ftol_patch.hpp"
#include "core/safe_mode.hpp"
#include "win/hit_hook.hpp"
#include "win/map_start.hpp"
#include "win/pick_hook.hpp"
#include "win/world_mouse.hpp"
#include "win/server_night.hpp"
#include "win/answer_card.hpp"
#include "win/minimap_proto.hpp"
#include "win/reference_window.hpp"
#include "win/buff_column.hpp"
#include "win/buff_overlay.hpp"
#include "win/cast_bar.hpp"
#include "win/char_block.hpp"
#include "win/code_page.hpp"
#include "win/party_panel.hpp"
#include "win/hotbar.hpp"
#include "win/chat_window.hpp"
#include "win/item_window.hpp"
#include "win/stat_window.hpp"
#include "win/target_card.hpp"
#include "win/notifier.hpp"
#include "win/platform.hpp"
#include "win/shadow_patch.hpp"
#include "win/sound_engine.hpp"
#include "win/sound_hook.hpp"
#include "win/gpu/adapters.hpp"
#include "win/game_camera.hpp"
#include "win/sprite_hook.hpp"
#include "win/start_screen.hpp"
#include "win/texture_hook.hpp"
#include "win/log.hpp"
#include "win/low_hp_edge.hpp"
#include "win/thread_time.hpp"
#include "win/ui_cost.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/profiler.hpp"
#include "win/real_ddraw.hpp"
#include "win/window_mode.hpp"
#include "win/windows_draw.hpp"
#include "win/world_dim.hpp"

namespace uagfx::win {
namespace {

std::wstring wideOf(const std::string& utf8) {  // 2.15: a text of strings.cpp for the overlay
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring w(size_t(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), &w[0], n);
    return w;
}

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

// 2.2.1: the settings played with, a line a stretch (core/usage.hpp) - with the player's leave only, while the game is
// in front and shows a map; the setup looked at once a second, not each frame.
void Context::noteUsage(bool foreground, long outWidth, long outHeight, const GpuTiming* gpu) {
    const ULONGLONG now = GetTickCount64();
    const double seconds = usageLast_ ? double(now - usageLast_) / 1000.0 : 0.0;
    usageLast_ = now;
    if (!config.statsSend || !foreground || map_.empty() || !renderer) return;
    if (!usage_.begun()) {
        const Startup& s = startup();
        usage::Machine m;
        m.version = kVersion;
        m.card = s.adapter;
        m.api = s.choice.renderer != Renderer::Uaro ? "system" : s.choice.api == Api::D3d11 ? "d3d11" : "d3d12";
        m.vendor = s.vendor;
        m.vramMb = s.vramMb;
        m.level = s.cardLevel;
        m.cpu = s.computer.cpu;  // 2.5.15
        m.cores = s.computer.cores;
        m.threads = s.computer.threads;
        m.ramMb = s.computer.ramMb;
        m.remote = renderer->remote();  // 2.2.2
        LARGE_INTEGER q;
        QueryPerformanceCounter(&q);
        usage_.begin(m, uint64_t(q.QuadPart) * 0x9e3779b97f4a7c15ull ^ (uint64_t(GetCurrentProcessId()) << 32) ^ now);
    }
    if (!usageSetupAt_ || now - usageSetupAt_ >= 1000) {
        usageSetupAt_ = now;
        usage::Setup u;
        u.game = std::to_string(screenWidth) + "x" + std::to_string(screenHeight);
        u.output = std::to_string(outWidth) + "x" + std::to_string(outHeight);
        u.dlss = config.dlss;
        u.preset = effect_groups::presetToken(config.effects.preset);
        for (int g = 0; g < kEffectGroups; ++g)
            u.groups += std::string(g ? "," : "") + effect_groups::groupWord(g) + ":" +
                        (config.effects.groups[g] ? "1" : "0") + ":" + std::to_string(config.effects.groupStrength[g]);
        const EffectSettings drawn = enhanced_ ? effects() : effectsOff();
        for (int i = 0; i < effect_groups::memberCount(); ++i) {
            const effect_groups::Member& m = effect_groups::members()[i];
            if (drawn.*m.on) u.effects += std::string(u.effects.empty() ? "" : ",") + m.key;
        }
        u.aiKinds = enhanced_ ? aiKindsShown(config.aiNow) : 0;
        u.aiModel = config.ai.model + "/" + config.ai.spriteModel;
        u.fpsLimit = config.fpsLimit;  // 2.2.2
        u.vsync = config.vsync;
        u.safe = usageSafe_;
        usageSetup_ = u;
    }
    double gpuMs = -1.0;
    if (gpu && gpu->serial != usageGpuSerial_) {
        usageGpuSerial_ = gpu->serial;
        gpuMs = gpu->gpuMs;
        usageSafe_ = gpu->safeShaders > 0;
    }
    std::string lines;
    usage_.frame(seconds, outdoors(map_), gpuMs, usageSetup_, lines);
    writeUsage(lines);
}

// The lines to uaro-graphics\usage.jsonl, which the settings app sends and empties; past 4 MB nothing more (the app
// not run for long, or no leave in it any more).
void Context::writeUsage(const std::string& lines) {
    if (lines.empty()) return;
    const std::string path = gameDir() + "\\uaro-graphics\\usage.jsonl";
    WIN32_FILE_ATTRIBUTE_DATA a{};
    const bool full = GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a) &&
                      (a.nFileSizeHigh || a.nFileSizeLow > (4u << 20));
    if (full) return;
    std::ofstream f(path, std::ios::binary | std::ios::app);
    f.write(lines.data(), std::streamsize(lines.size()));
}

EffectSettings Context::madeEffects(const EffectSettings& file) const {
    // 2.5.20: as the light modes leave them - super-light none, the battery's at most the weak set's
    EffectSettings e = light::effectsOf(effect_groups::effective(file, cardLevel()), light_);
    // 3.0.4-beta.35: a card weak by its memory - the grass thinned (its roots on every second row), even raised by its
    // speed; keep_fps thins it too (keepOff_'s grass_thin)
    e.grassThin = startup().memoryLevel == 1;
    return e;
}

const EffectSettings& Context::effects() const {
    if (test_.running()) return testFx_;  // 3.0.5-beta.6: the set «Тест графіки» shows
    if (!effectsMade_) {
        effects_ = madeEffects(config.effects);
        effectsMade_ = true;
    }
    return effects_;
}

Context& Context::get() {
    static Context c;
    return c;
}

Context::Context() {
    InitializeCriticalSection(&cs_);
    // 3.0.3-beta.11 (the user: «счетчик кадров замер»): the frame counter and the console are windows of their own over
    // all others - hidden when the game window goes behind, not left over another game window with what they last said
    windowSwitch_.onLeave = [this] {
        fps_.hide();
        console_.hide();
    };
    windowSwitch_.onBehind = [this] { behind(); };  // 3.0.3-beta.12
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
    ownWake();  // 3.0.3-beta.9: made before the 64-bit renderer starts - it opens it by this game's process
    gpu::Renderer* r = new gpu::Renderer;
    r->setStartSink([](void*, const start::Progress& p) { showStart(p); }, nullptr);
    bool remote = false;
    // 0.46: Direct3D 11 (graphics cards without Direct3D 12) - none of what needs Direct3D 12 or the 64-bit process.
    const bool d3d11 = startup().choice.api == Api::D3d11;
    if (d3d11) {
        std::string why;
        const std::string shaders = ourFile(layout::File::ShadersD3d11);
        bool made = false;
        if (config.renderThread) {  // 2.5.12 (test): on a thread of its own - the game's thread only writes frames down
            gpu::BridgeOptions o;
            o.thread = true;
            o.gameDir = gameDir();
            o.shaderCache = shaders;
            o.ai = config.ai;
            o.stateCache = config.stateCache;
            o.mergeDraws = config.mergeDraws;
            o.shaderVariants = config.shaderVariants;  // 2.5.16
            o.logLevel = int(config.logLevel);
            made = r->createRemote(o, why);
            if (made)
                LOG_INFO("Direct3D 11: drawing on a thread of its own ([renderer] render_thread = 1)");
            else
                LOG_ERROR("Direct3D 11's thread: %s; drawing on the game's thread", why.c_str());
        }
        if (!made && !r->createD3D11(shaders, why)) {
            error = "Direct3D 11: " + why;
            delete r;
            return false;
        }
        remote = true;  // made: no other renderer
        if (config.remix || config.bridge || config.dlss != "off")
            LOG_INFO("Direct3D 11: RTX Remix, the 64-bit renderer and DLSS need Direct3D 12 - off");
        if (drawnByRenderer(effects()))  // 0.46.1; 2.5.9: every effect of Direct3D 12's now
            LOG_INFO("Direct3D 11: the effects are drawn as with Direct3D 12 (the remaster's - parallax, the world "
                     "in the water - with the strong set); DLSS, TAA and the world drawn larger (render_scale over "
                     "100) need Direct3D 12");
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
    // 3.0.4-beta.8 (the user's idea 7): under Wine (Linux, the Steam Deck) the 64-bit renderer's picture would have to
    // cross processes - the game's own process draws, unless [renderer] bridge_wine
    const platform::Facts& where = platformFacts();
    LOG_INFO("platform: %s", platform::describe(where).c_str());
    const bool bridgeWanted = config.bridge || config.dlss != "off" || config.ai.on();
    if (!remote && bridgeWanted && !platform::bridgeAllowed(where, config.bridgeWine))
        LOG_INFO("bridge: not under Wine - its picture would cross processes (no DirectComposition there); DLSS and "
                 "the HD textures need it ([renderer] bridge_wine = 1 to try)");
    // Part 3: the 64-bit renderer - DLSS and the AI textures need it.
    if (!remote && bridgeWanted && platform::bridgeAllowed(where, config.bridgeWine)) {
        gpu::BridgeOptions o;
        o.hostExe = gameDir() + "\\uaro-graphics\\uaro-render64.exe";
        o.gameDir = gameDir();
        o.present = config.bridgePresent;
        o.dpiScale = config.bridgeDpiScale;
        o.logLevel = int(config.logLevel);
        o.logSlot = std::max(1, wlog::slot());  // 3.0.3-beta.1: uaro-render64-<slot>.log beside this game's
        o.dlss = config.dlss;
        o.dlssScale = config.dlssScale;  // 2.4.2
        o.dlssWidth = config.dlssWidth;
        o.dlssHeight = config.dlssHeight;
        o.dlssModel = config.dlssModel;
        o.dlssJitterInverted = config.dlssJitterInverted;
        o.dlssMotionInverted = config.dlssMotionInverted;
        o.frameGeneration = config.dlss != "off" ? config.frameGeneration : 0;  // it uses DLSS's depth and motion
        o.reflex = config.reflex;  // 2.5.26
        o.shaderVariants = config.shaderVariants;  // 3.0.5-beta.3: Direct3D 12's states' own draw shaders
        o.ai = config.ai;
        o.gpu = config.gpu;
        std::string why;
        remote = r->createRemote(o, why);
        if (remote)
            LOG_INFO("bridge: drawing in uaro-render64.exe (64-bit), pictures: %s", o.present.c_str());
        else
            LOG_ERROR("bridge: %s; drawing in the game's process", why.c_str());
    }
    r->setShaderCache(ourFile(layout::File::Shaders32));  // 0.39.5; 2.8: uaro-graphics\cache
    if (!remote && !r->create(error)) {
        delete r;
        return false;
    }
    r->setWake(ownWake());  // 3.0.3-beta.9: the frame limit's wait ends when the window is switched to
    renderer = r;
    renderer->setUiSharp(config.uiFilter == UiFilter::Sharp);
    renderer->setStateCache(config.stateCache);  // 0.88
    if (!config.stateCache) LOG_INFO("Direct3D 11: every state set again each draw ([debug] state_cache = 0)");
    renderer->setMergeDraws(config.mergeDraws);  // 0.90
    renderer->setShaderVariants(config.shaderVariants);  // 2.5.16
    if (!config.shaderVariants)  // 3.0.5-beta.3: Direct3D 12's too
        LOG_INFO("%s: every draw with the one shader for every state ([debug] shader_variants = 0)",
                 d3d11 ? "Direct3D 11" : "Direct3D 12");
    if (!config.mergeDraws && !renderer->remote())  // 2.5: Direct3D 12 in this process gathers them too
        LOG_INFO("each of the game's draws by itself ([debug] merge_draws = 0)");
    renderer->preparePipelines(ourFile(layout::File::Pipelines));
    LOG_INFO("%s renderer on %s", d3d11 ? "Direct3D 11" : "Direct3D 12", renderer->adapterName().c_str());
    fpsShown_ = config.fpsCounter;  // 0.54
    fps_.setLabel(std::string(d3d11 ? "Direct3D 11" : "Direct3D 12") + (config.dlss != "off" ? " + DLSS" : ""));
    // 0.47: the AI textures with Direct3D 11 - the texture pack's pictures, in the game's process (2.5.12: on its own
    // thread, which started them itself).
    // 2.5.20: super-light from the start - no AI picture is drawn: none read either (back with the next start)
    if (d3d11 && config.ai.enabled && config.superLight)
        LOG_INFO("AI textures: not started - super-light ([quality] super_light)");
    const bool aiHere = d3d11 && config.ai.enabled && !config.superLight &&
                        (renderer->remote() || renderer->startAiTextures(gameDir(), config.ai));
    crisp::setWindowArt(config.windowArt);  // 0.41: before the first twin
    crisp::install(config.crispText);
    // AI textures (0.13): sprites through the game's sprite cache, cell by cell (sprite_hook.hpp); (0.14) the windows'
    // icons through crisp text's twins. 0.23: the torches' frames are known through it too - whenever the picture is
    // ours (not RTX Remix's), as the torches can be turned on in a running game.
    // 0.46.1: Direct3D 11 draws no torches; 0.47: its AI textures take the sprite frames when they are on.
    const AiSettings& ai = config.ai;
    const bool cells = ai.on() && (ai.sprites() || ai.effects || ai.icons);
    // 0.61: Direct3D 11 draws the characters' shadows and the torches' light now: they know the sprites by it too.
    const EffectSettings drawn = effects();
    const bool d3d11Hook = (aiHere && cells) || drawn.actorShadows || drawn.torch || drawn.glint ||
                           drawn.hitFlash;  // 0.95, 0.96: the sprites' kinds
    if (d3d11 ? d3d11Hook : ((cells && renderer->remote()) || !config.remix)) installSpriteHook();
    installEffectHook();  // 0.27: the level 99 aura softer (Device7::record)
    setFullColour(config.fullColour);  // 2.5.18: the textures in their pictures' own colours
    setFastTextures(config.fastTextures);  // 2.5.20
    setSpriteDyes(config.aiNow.dyes);      // 2.5.23
    if (!config.remix) installTextureHook();
    installMapStartHook();             // 3.0.2-beta.15: a map's loading known whatever its camera holds
    installPickHook(config.fastPick);  // 0.89: the cell under the mouse without testing the whole map
    installWorldMouse();               // 3.0.11-beta.24: the driver's parts are windows for the game's cursor
    installWindowsDraw();              // 3.0.11-beta.53: the replaced windows hidden before the game draws them
    buffcolumn::installBuffIcons();    // 3.0.11-beta.35: the game's status icons hidden for the buffs' column
    chatwin::install();                // 3.0.11-beta.41: the game's chat lines read for the new chat
    installFastFtol(config.fastFtol);  // 2.5.20: float to integer without switching the x87
    installChatFix();                  // 2.6.12: Enter in the chat box while a map loads (R-PVF8AV)
    installShadowFix();                // 3.0.4-beta.9: a shadow drawn while a map loads (R-VBDSCP)
    installBlindFix();                 // 2.6.18: blindness's darkness over the whole screen (R-MCVUC8)
    installHitHook();                  // 0.96: the hits' sounds (the flash itself is effects.hit_flash)
    renderer->setIconSink(&crisp::iconReady, nullptr);
    renderer->setGameDir(gameDir());  // 0.25: the maps' files (uaro-render64.exe has the folder already)
    crisp::setAiIcons(config.ai.on() && config.ai.icons && (renderer->remote() || aiHere) && config.crispText,
                      config.ai.uiPieces);
    return true;
}

void Context::close() {
    Profiler::get().stop();  // 0.87
    if (usage_.begun()) {  // 2.2.1: the last stretch of the statistics, the game ending
        std::string lines;
        usage_.close(lines);
        writeUsage(lines);
    }
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
    const EffectSettings fx = effects();
    // 1.8.4: every effect the renderer draws itself - the grass, the living world, the materials... need the camera
    // too (only the older ones were listed: alone, the newer ones went without it)
    const bool world = drawnByRenderer(fx);
    // 2.5: Direct3D 11 takes it too - its water (0.99, never given it until now), haze, sky and weather
    if (renderer->remix() || (enhanced_ && world)) {
        float view[12];
        float targetZ = 0;
        const bool known = readGameCamera(view);
        if (!known || !readCameraTargetDepth(targetZ)) targetZ = 0;
        renderer->setCamera(known, view, targetZ);
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

void Context::updateLight() {
    const ULONGLONG now = GetTickCount64();
    if (!config.batterySaver) {
        onBattery_ = false;
    } else if (!powerChecked_ || now - powerChecked_ >= 3000) {  // a cable pulled out is seen within 3 seconds
        powerChecked_ = now;
        SYSTEM_POWER_STATUS p{};
        onBattery_ = GetSystemPowerStatus(&p) && p.ACLineStatus == 0;
    }
    // 3.0.3-beta.3: several game windows - the ones not played in light, while another one is the main;
    // 3.0.3-beta.9: one asked to be shown next draws whole behind it, one wholly behind another one 2 frames a second
    const bool foreground = !window || GetForegroundWindow() == window;
    mainWindow_ = frontGame_.main(foreground) || windowSwitch_.summoned(foreground);
    const light::Role role = mainWindow_              ? light::Role::Main
                             : windowSwitch_.hidden() ? light::Role::Hidden
                                                      : light::Role::Other;
    const light::Overrides o = light::overridesOf(config, onBattery_, role);
    if (o == light_) return;
    if (o.other && (!light_.other || o.fpsMax != light_.fpsMax))
        LOG_INFO("windows: another game window is the main one - this one light: no effects, AI pictures or DLSS, "
                 "half the render scale, %d frames a second%s",
                 o.fpsMax, role == light::Role::Hidden ? " (wholly behind another game window)" : "");
    else if (!o.other && light_.other)
        LOG_INFO("windows: the main game window - the settings as the file has them");
    if (o.superLight != light_.superLight)
        LOG_INFO("super-light: %s", o.superLight ? "on - no effects, no AI pictures, the world at half size; Direct3D "
                                                   "11: its textures as their colour alone, 16-bit, no blended mip "
                                                   "levels"
                                                 : "off - the settings as the file has them");
    if (o.battery != light_.battery) {
        if (o.battery)
            LOG_INFO("battery saver: on the battery - %d frames at most, the effects at most the weak set's, the world "
                     "at 75 %% at most", o.fpsMax);
        else
            LOG_INFO("battery saver: %s - the settings as the file has them",
                     config.batterySaver ? "on the mains" : "off");
    }
    light_ = o;
    effectsMade_ = false;  // effects() made again
}

// 2.7.14: the cards' sparkles (Device7::record) - added light of their own kind: no soft particle (2.7.14)
void Context::drawSparkles() {
    for (const Sparkle& s : sparkles) {
        DrawCmd cmd;
        cmd.state = record.addState(s.setup);
        cmd.effect = uint8_t(effect_exe::Kind::Glint);
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        record.draw(cmd, s.v, glint::kSparkle, nullptr, 0);
    }
    sparkles.clear();
    if (orbSprites > 0 && orbLogs < 3) {  // 2.7.26: which of Ruwach's sprites the ball is on, the first frames
        ++orbLogs;
        const glint::OrbHeads::Head* h = orbHeads.heads().empty() ? nullptr : &orbHeads.heads()[0];
        LOG_INFO("orbs of light: %d of Ruwach's sprites, %d of them up in the air (the others its shadows); the head "
                 "%.0f pixels across, alpha %.2f, at %.0f,%.0f",
                 orbSprites, orbOwn, h ? h->size : 0.0f, h ? h->alpha : 0.0f, h ? h->x : 0.0f, h ? h->y : 0.0f);
    }
    orbSprites = orbOwn = 0;
    for (const glint::OrbHeads::Head& h : orbHeads.heads()) {  // 2.7.25
        glint::Vertex v[glint::kOrb];
        glint::orb(h.x, h.y, h.z, h.rhw, h.size, v);
        DrawCmd cmd;
        cmd.state = record.addState(orbSetup);
        cmd.effect = uint8_t(effect_exe::Kind::Glint);
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        record.draw(cmd, v, glint::kOrb, nullptr, 0);
    }
    orbHeads.clear();
}

void Context::endScene() {
    // 3.0.3-beta.12 (the user's R-KVTEMT: «всеравно курсор не видно»): the scene goes to the GPU here, before the
    // present - beta.9's and beta.11's panel, put in at the present, found no cursor in an empty record and covered it
    const bool drawn = record.takeWork() > 0;
    if (photo_) record.dropInterface();  // 3.0.4-beta.5: the photo mode - the world alone
    if (drawn && renderer && !photo_) buffoverlay::draw(record, *renderer);  // 3.0.4-beta.6: the buffs' times
    if (panelSlot_.endScene(drawn) && renderer && !photo_) {
        worlddim::draw(record);                 // 3.0.11-beta.49: the world dimmed under the windows (where it ends)
        lowhpedge::draw(record, *renderer);     // 3.0.11-beta.50: the low HP edge, there too
        castwin::draw(record, *renderer);       // 3.0.11-beta.48: the cast over the hero, under the panels
        minimapproto::draw(record, *renderer);  // 3.0.11-beta.1: the new minimap (prototype)
        buffcolumn::draw(record, *renderer);    // 3.0.11-beta.35: the buffs' column under it
        charpanel::draw(record, *renderer);     // 3.0.11-beta.36: the character block
        targetpanel::draw(record, *renderer);   // 3.0.11-beta.38: the target's card
        partywin::draw(record, *renderer);      // 3.0.11-beta.39: the party's panel
        hotbarwin::draw(record, *renderer);     // 3.0.11-beta.40: the hotbar
        chatwin::draw(record, *renderer);       // 3.0.11-beta.41: the chat
        itemswin::draw(record, *renderer);      // 3.0.11-beta.42: «Речі»
        statswin::draw(record, *renderer);      // 3.0.11-beta.42: «Характеристики»
        answercard::draw(record, *renderer);  // 3.0.9-beta.4: the developer's answer
        windowSwitch_.draw(record, *renderer);
        referencewin::draw(record, *renderer);  // 3.0.11-beta.7: «Довідник», over the rest
        record.takeWork();  // the driver's own draws are not the game's
    }
    flush();
}

// 3.0.3-beta.12 (R-KVTEMT: «звуки со второго окна слышно»): a game window behind another one - on its full screen the
// game no longer presents there, and what its presents did stopped with them: its sounds' fade (the background's
// volume, the other windows quiet) and its entry for the other windows' panels (its HP and SP)
void Context::behind() {
    Guard guard;
    if (!window || !renderer) return;
    if (!checkingWindows_ && GetTickCount64() - windowsAt_ >= 250) {
        windowsAt_ = GetTickCount64();
        checkingWindows_ = true;
        windowSwitch_.check(window, false, readOwnName(), config);
        checkingWindows_ = false;
    }
    mainWindow_ = frontGame_.main(false);
    soundFrame(false, mainWindow_ || config.windowOthersMusic, mainWindow_ || config.windowOthersSounds);
    notifier::frame(window, false, mainWindow_, config);  // 3.0.4-beta.3
    answercard::behind(window);                            // 3.0.9-beta.4: a new answer told from behind
}

void Context::present(const Rect& source, bool waitForVblank) {
    // 3.0.3-beta.8: whether the game drew this picture anew - the windows' panel goes over such a one alone: the
    // target keeps its pixels across a present, and over the same picture again the panel would darken itself.
    // 3.0.3-beta.12: put into the game's scene at its EndScene; here only into a picture drawn without one
    const bool drawnAnew = record.takeWork() > 0;
    drawSparkles();  // 2.7.14: a frame with no interface draw
    if (panelSlot_.present(drawnAnew) && renderer && !photo_) {
        worlddim::draw(record);                 // 3.0.11-beta.49
        lowhpedge::draw(record, *renderer);     // 3.0.11-beta.50
        castwin::draw(record, *renderer);       // 3.0.11-beta.48
        minimapproto::draw(record, *renderer);  // 3.0.11-beta.1
        buffcolumn::draw(record, *renderer);    // 3.0.11-beta.35
        charpanel::draw(record, *renderer);     // 3.0.11-beta.36
        targetpanel::draw(record, *renderer);   // 3.0.11-beta.38
        partywin::draw(record, *renderer);      // 3.0.11-beta.39
        hotbarwin::draw(record, *renderer);     // 3.0.11-beta.40
        chatwin::draw(record, *renderer);       // 3.0.11-beta.41
        itemswin::draw(record, *renderer);      // 3.0.11-beta.42
        statswin::draw(record, *renderer);
        answercard::draw(record, *renderer);  // 3.0.9-beta.4
        windowSwitch_.draw(record, *renderer);
        referencewin::draw(record, *renderer);  // 3.0.11-beta.7
    }
    record.takeWork();  // the driver's own draws are not the game's
    flush();
    effectFrame();  // 0.27: the effects' primitives of the next frame
    monsters.frame();  // 2.7.10: this frame's monsters, where the next hits look for theirs
    if (hitProbe.frames > 0 && --hitProbe.frames == 0)  // 2.7.11: the probe's end
        LOG_INFO("hit flash probe: %d quads about %.0f,%.0f", hitProbe.lines, hitProbe.x, hitProbe.y);
    if (!renderer) return;
    watchSettings();
    updateLight();  // 2.5.20
    if (window && IsIconic(window)) {  // minimised: nothing to show, and no vsync to hold the game's loop back
        noteWindow(true, false);
        // 2.5.20: 5 on the battery; 3.0.3-beta.9: another game window's 2 - woken at once when it is switched to
        const DWORD ms = light_.battery || light_.other ? DWORD(1000 / std::max(1, light_.minimisedFps)) : 15;
        if (HANDLE wake = ownWake())
            WaitForSingleObject(wake, ms);
        else
            Sleep(ms);
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
    // 3.0.3-beta.2: several game windows - the name in the title, each character's window at its place, four times
    // a second. Here, before the frame is made, and not within itself: moving the window sends WM_MOVE, and the
    // client may present again from its handler (3.0.3-beta.4).
    if (window && !checkingWindows_ && GetTickCount64() - windowsAt_ >= 250) {
        windowsAt_ = GetTickCount64();
        checkingWindows_ = true;
        const std::string name = readOwnName();
        severalWindows_.check(window, name, config);
        windowSwitch_.check(window, foreground, name, config);  // 3.0.3-beta.6: the keys that switch the windows
        checkingWindows_ = false;
    }
    // Pause or Scroll Lock in the game window: every improvement off - the game's size stretched like Windows does,
    // no interface filter, no sharpening - and on again, to compare without a restart.
    const bool keyDown = window && GetForegroundWindow() == window &&
                         (GetAsyncKeyState(VK_PAUSE) < 0 || GetAsyncKeyState(VK_SCROLL) < 0);
    if (compareKey_.pressed(keyDown)) {
        enhanced_ = !enhanced_;
        LOG_INFO("compare key: improvements %s", enhanced_ ? "on" : "off");
        // 2.15: said on the screen while they are off - a stray press looked like a broken game
        fps_.setNote(enhanced_ ? L"" : wideOf(settings::tr(startup().lang, settings::Text::CompareOffNote)));
    }
    // 0.54: Ctrl+Shift+F11 - the frame counter
    const bool fpsDown = window && GetForegroundWindow() == window && GetAsyncKeyState(VK_CONTROL) < 0 &&
                         GetAsyncKeyState(VK_SHIFT) < 0 && GetAsyncKeyState(VK_F11) < 0;
    if (fpsKey_.pressed(fpsDown)) {  // 2.2.1: written too - the console, the settings and the next start see it
        fpsShown_ = !fpsShown_;
        if (editIniFile(ourFile(layout::File::Ini), {{"frames", "fps_counter", fpsShown_ ? "1" : "0"}})) {
            config.fpsCounter = fpsShown_;  // what the file says now: not taken for a change of it
            LOG_INFO("frame counter: %s (Ctrl+Shift+F11)", fpsShown_ ? "on" : "off");
        }
    }
    // 3.0.4-beta.5 (the user's idea 5): Ctrl+Alt+F12 - the photo mode, the game's interface out of the picture
    const bool photoDown = window && GetForegroundWindow() == window && GetAsyncKeyState(VK_CONTROL) < 0 &&
                           GetAsyncKeyState(VK_MENU) < 0 && GetAsyncKeyState(VK_F12) < 0;
    if (photoKey_.pressed(photoDown)) {
        photo_ = !photo_;
        LOG_INFO("photo mode: %s (Ctrl+Alt+F12)", photo_ ? "on - the game's interface, the cursor and the panel hidden"
                                                         : "off");
    }
    keepCameraFree(config.cameraIndoor);  // 3.0.4-beta.5: building maps' camera as outdoors
    limitZoom(config.cameraZoomLimit);     // 3.0.4-beta.17: no further out than the character is drawn
    raiseWaterBoxes(enhanced_ && effects().ownGround);  // 3.0.4-beta.20: the game's water to its far plane
    // 2.3: Ctrl+Shift+F9 - uaRO Studio's X-ray: the AI pictures' textures tinted by how they are shown; written too,
    // as Ctrl+Shift+F11 is ([debug] xray, the console's switch)
    const bool xrayDown = window && GetForegroundWindow() == window && GetAsyncKeyState(VK_CONTROL) < 0 &&
                          GetAsyncKeyState(VK_SHIFT) < 0 && GetAsyncKeyState(VK_F9) < 0;
    if (xrayKey_.pressed(xrayDown && config.developer)) {  // 2.15: uaRO Studio's keys - the Developer mode's
        const bool on = !config.xray;
        if (editIniFile(ourFile(layout::File::Ini), {{"debug", "xray", on ? "1" : "0"}})) {
            config.xray = on;
            LOG_INFO("X-ray: %s (Ctrl+Shift+F9)", on ? "on" : "off");
        }
    }
    // 2.3: Ctrl+Shift+F8 - the texture under the game's cursor into uaRO Studio; Ctrl+Shift+F7 - its picture off in
    // the game, and on again (A/B of one texture)
    const bool ctrlShift = window && GetForegroundWindow() == window && GetAsyncKeyState(VK_CONTROL) < 0 &&
                           GetAsyncKeyState(VK_SHIFT) < 0 && config.developer;  // 2.15: the Developer mode's
    uint8_t once = 0;
    int cursorX = 0, cursorY = 0;
    if (pickKey_.pressed(ctrlShift && GetAsyncKeyState(VK_F8) < 0)) {
        POINT p{};
        RECT r{};
        if (readGameCursor(cursorX, cursorY)) {  // the game's own (DirectInput's), in its pixels
            once |= 2;
        } else if (GetCursorPos(&p) && ScreenToClient(window, &p) && GetClientRect(window, &r) && r.right > 0 &&
                   r.bottom > 0) {  // another build: Windows' cursor, the window's pixels to the game's
            cursorX = int(int64_t(p.x) * shadow.width() / r.right);
            cursorY = int(int64_t(p.y) * shadow.height() / r.bottom);
            once |= 2;
        }
    }
    if (abKey_.pressed(ctrlShift && GetAsyncKeyState(VK_F7) < 0)) once |= 4;
    // Pause: the game's own everywhere, no tint; 2.7.8: bits 3-4 the ground seams' debug view ([debug] seams_view)
    const uint8_t studio = enhanced_ ? uint8_t((config.xray ? 1 : 0) | (std::clamp(config.seamsView, 0, 2) << 3)) : 0;
    if (studio != studioSent_ || once) {
        renderer->setStudio(uint8_t(studio | once), cursorX, cursorY);
        studioSent_ = studio;
    }
    // 0.54: Ctrl+Shift+F12 - the picture as shown (4K, not the game's 1920x1080) into the game's ScreenShot folder
    const bool shotDown = window && GetForegroundWindow() == window && GetAsyncKeyState(VK_CONTROL) < 0 &&
                          GetAsyncKeyState(VK_SHIFT) < 0 && GetAsyncKeyState(VK_F12) < 0;
    // 2.13: Ctrl+Shift+F10 - «Звернення»: the same shot, then the settings app's «Нове звернення» with it and the map
    const bool reportDown = window && GetForegroundWindow() == window && GetAsyncKeyState(VK_CONTROL) < 0 &&
                            GetAsyncKeyState(VK_SHIFT) < 0 && GetAsyncKeyState(VK_F10) < 0;
    const bool shotNow = shotKey_.pressed(shotDown);
    // 3.0.9-beta.4: while the answer card shows, the same key opens the answer - no «Нове звернення» then
    const bool reportNow = reportKey_.pressed(reportDown) && !answercard::openByKey();
    std::string screenshot;
    if (shotNow || reportNow) {
        const std::string folder = gameDir() + "\\ScreenShot";
        CreateDirectoryA(folder.c_str(), nullptr);
        SYSTEMTIME t{};
        GetLocalTime(&t);
        char name[64];
        std::snprintf(name, sizeof name, "\\uaro-%04u%02u%02u-%02u%02u%02u.png", t.wYear, t.wMonth, t.wDay, t.wHour,
                      t.wMinute, t.wSecond);
        screenshot = folder + name;
        // 3.0.4-beta.25 (R-GXHFX8 «всеравно корабль разрезаный получается»): the game's camera with the shot - its
        // view (world -> view, rows) and the game's size: the picture made again offline exactly
        float view[12];
        if (readGameCamera(view))
            LOG_INFO("camera: the game's view with the shot %s - %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.4f "
                     "%.4f %.4f; the game %dx%d", name + 1, view[0], view[1], view[2], view[3], view[4], view[5],
                     view[6], view[7], view[8], view[9], view[10], view[11], screenWidth, screenHeight);
        if (reportNow) startReport(screenshot, readGameMap());
    }
    // Stage 3: the window's real pixels and the scale for them. The first present switches from the game's size:
    // the start-up calibration has run by then (spec §4.3).
    const SIZE out = window ? clientPixels(window, config.highResolution) : SIZE{shadow.width(), shadow.height()};
    // Part 3: DLSS draws the world at its own size and shows it at the picture's - no supersampling on top of it
    // (DLAA at 150% would draw 2.25 times the pixels for nothing).
    const bool dlss = renderer->upscales() && config.dlss != "off" && !light_.noUpscaler;  // 3.0.3-beta.3
    // 0.62: [display] ui_native - supersampling the world alone (Direct3D 12 in this process, no DLSS): the target the
    // picture's size, the world render_scale times it, made smaller into it; the interface at the picture's pixels.
    const bool nativeWanted = !dlss && config.uiNative && config.highResolution && config.renderScale > 100 &&
                              !light_.noSupersampling && renderer->canSupersample();
    const bool uiNative =
        nativeWanted && renderer->setSupersample(enhanced_ ? config.renderScale / 100.0 : 1.0) && enhanced_;
    if (!nativeWanted) renderer->setSupersample(1.0);  // turned off in a running game
    // 0.75: [quality] taa - the renderer's own, where neither DLSS nor ui_native is used
    const bool taaWanted =
        !dlss && !nativeWanted && config.taa && !light_.noSupersampling && renderer->canSupersample();
    const bool taa = taaWanted && renderer->setTaa(enhanced_) && enhanced_;
    if (!taaWanted) renderer->setTaa(false);
    const int renderScale = dlss || uiNative || taa ? 100 : std::min(config.renderScale, light_.renderScaleMax);
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
    const int samples = enhanced_ && !upscale && config.msaa > 1 && !light_.noSupersampling ? config.msaa : 1;
    if (plan.targetWidth != scaledWidth_ || plan.targetHeight != scaledHeight_ || samples != scaledSamples_ ||
        upscale != scaledUpscale_) {
        scaledWidth_ = plan.targetWidth;  // also when it fails: no new attempt every frame
        scaledHeight_ = plan.targetHeight;
        scaledSamples_ = samples;
        scaledUpscale_ = upscale;
        scaleMade_ = renderer->setScale(plan.k, plan.targetWidth, plan.targetHeight, samples, upscale);
    }
    // 3.0.3-beta.8: the windows' panel at this frame's scale - drawn into the next present's record
    if (window)
        windowSwitch_.frame(window, mainWindow_, config, scaleMade_ ? plan.k : 1.0,
                            scaleMade_ ? plan.targetWidth : shadow.width(),
                            scaleMade_ ? plan.targetHeight : shadow.height(), gameFullscreen);
    // 3.0.4-beta.6: the buffs' times on the game's icons - none while the buffs' column hides them (3.0.11-beta.35)
    buffoverlay::frame(config.buffTimers && !buffcolumn::hidingGameIcons(), float(scaleMade_ ? plan.k : 1.0));
    // 3.0.9-beta.4: the developer's answer - its card at this frame's scale, drawn into the next present's record
    answercard::frame(window, scaleMade_ ? plan.k : 1.0, scaleMade_ ? plan.targetWidth : shadow.width(),
                      scaleMade_ ? plan.targetHeight : shadow.height());
    // 3.0.11-beta.54: the new interface's parts - at the screen's own resolution when the render scale is a whole r
    // (their pixels r x r of the target: R-PVZH5M's 4K at 200 % painted them 4 times over) and at their size (auto:
    // smaller on a small game, R-JK5455); their k, width and height these
    const int stretch =
        scaleMade_ && plan.r > 1.5 && std::fabs(plan.r - std::round(plan.r)) < 1e-3 ? int(std::lround(plan.r)) : 1;
    setPartStretch(stretch);
    const double pk = (scaleMade_ ? plan.k : 1.0) / stretch;
    const int pw = (scaleMade_ ? plan.targetWidth : shadow.width()) / stretch;
    const int ph = (scaleMade_ ? plan.targetHeight : shadow.height()) / stretch;
    uistyle::setSize(config.uiSize > 0 ? float(config.uiSize) / 100.0f
                                       : uistyle::autoSize(shadow.width(), shadow.height(), scaleMade_ ? plan.s : 1.0));
    // 3.0.11-beta.1: [interface] minimap - the new interface's minimap prototype in the game's own one's corner
    minimapproto::frame(window,
                        {config.newMinimap, config.minimapNpcs, config.minimapWarps, config.minimapMobs,
                         config.minimapWay},
                        pk, pw, ph);
    // 3.0.11-beta.35: [interface] buffs - the buffs' column under the new minimap, the game's icons hidden
    buffcolumn::frame(window, config.buffs, pk, pw, ph);
    motion::setOn(config.animations);  // 3.0.11-beta.46: [interface] animations - the new interface's motion
    uistyle::set(config.hudOpacity, config.windowOpacity);  // 3.0.11-beta.47: its grounds
    // 3.0.11-beta.36: [interface] character - the character block at the top left, the game's Basic Info hidden
    charpanel::frame(window, config.character, pk, pw, ph);
    // 3.0.11-beta.38: [interface] target - the target's card at the top middle
    targetpanel::frame(window, config.targetCard, pk, pw, ph);
    // 3.0.11-beta.39: [interface] party - the party's panel under the character block
    partywin::frame(window, config.partyPanel, gameCodePage(config), pk, pw, ph);
    // 3.0.11-beta.41: [interface] chat - the chat at the bottom left, the game's own out of sight (before the hotbar:
    // it keeps clear of it)
    chatwin::frame(window, config.chat, gameCodePage(config), pk, pw, ph);
    // 3.0.11-beta.49: [interface] dim_world - the world dimmed while the new windows are open
    worlddim::frame(config.dimWorld, pk, pw, ph);
    // 3.0.11-beta.50: [interface] low_hp_edge - the screen's edges red at low HP (0: none)
    lowhpedge::frame(config.lowHpEdge, pk, pw, ph);
    // 3.0.11-beta.48: [interface] cast - the character's cast over the hero, the game's own gauge hidden
    castwin::frame(window, config.castLabel, gameCodePage(config), pk, pw, ph);
    // 3.0.11-beta.40: [interface] hotbar - the hotkeys' bar at the bottom middle, the game's own hidden
    hotbarwin::frame(window, config.hotbar, gameCodePage(config), pk, pw, ph);
    // 3.0.11-beta.42: [interface] items and status - «Речі» and «Характеристики» while the game's own are open
    itemswin::frame(window, config.itemsWindow, gameCodePage(config), pk, pw, ph);
    statswin::frame(window, config.statusWindow, pk, pw, ph);
    // 3.0.11-beta.7: [interface] reference - «Довідник» (Ctrl+F)
    referencewin::frame(window, config.referenceWindow, pk, pw, ph);
    // 2.5.16: [quality] world_scale - Direct3D 11 draws the world smaller and makes it larger before the interface;
    // Pause draws it whole (the game's own picture to compare with)
    renderer->setWorldScale(enhanced_ ? std::min(config.worldScale, light_.worldScaleMax) / 100.0 : 1.0);
    renderer->setLight(enhanced_ ? light_.flags : uint8_t(0));  // 2.5.20: Pause shows the game's own picture
    // 3.0.5-beta.2: [debug] profile - the frame's spans too (frame_profile); 3.0.5-beta.6: on while the test runs
    renderer->setProfile(config.profile || test_.running());
    if (upscale && (dlss || taa)) {  // the camera of the frame being shown: the motion of its pixels
        float view[12];
        float targetZ = 0;
        const bool known = readGameCamera(view);
        if (!known || !readCameraTargetDepth(targetZ)) targetZ = 0;
        renderer->setCamera(known, view, targetZ);
        if (known != cameraKnown_) {
            cameraKnown_ = known;
            LOG_INFO("%s: the game's camera %s", taa ? "TAA" : "DLSS",
                     known ? "found" : "not in view (no world shown, or another exe)");
        }
    }
    // AI textures: the pack's index notes the maps textures are seen on (making them); 0.25: the map's file - its lamps
    // and the torches the game does not show.
    if ((config.ai.on() && renderer->remote()) || !config.remix) {
        const std::string map = readGameMap();
        if (map != map_) {
            map_ = map;
            renderer->setMap(map);
            if (!map.empty()) LOG_INFO("map: %s", map.c_str());
        }
    }
    if (knownGameBuild()) {  // 3.0.2-beta.12: the world by the game's camera, read for it alone without DLSS
        float seen[12];
        watchMapLoad(readGameMap(), readGameCamera(seen), !waitForVblank);
    }
    // 2.2.1: the AI pictures' kinds as the file says now (a kind off hidden at once; 2.2.2: each kind of sprites
    // too); Pause: none
    const bool aiShown = enhanced_ && !light_.aiHidden;  // 2.5.20: super-light draws none of them
    renderer->setDrawQuality(enhanced_ && !light_.noAnisotropy ? config.anisotropy : 0,
                             enhanced_ && config.alphaToCoverage && !light_.noSupersampling,
                             aiShown ? aiKindsShown(config.aiNow) : uint8_t(0),
                             aiShown ? aiSpritesShown(config.aiNow) : uint8_t(0));
    testFrame(foreground);  // 3.0.5-beta.6: «Тест графіки» - its set is effects() below
    keepFrameRate(config, window);
    // 0.87: [debug] profile; 3.0.5-beta.10: and while «Тест графіки» runs - where the game's thread goes on each set
    // (R-5A9BTU: Direct3D 11's «Високі» 63 ms of the processor a frame, nothing sampled with the profile off)
    Profiler::get().run(GetCurrentThreadId(), config.profile || test_.running());
    // 2.2: the set and the groups made into the switches; 0.63: how much night now, by the clock; 0.74: the minute
    EffectSettings fx = effects();
    {  // 2.6.11: the minute for all that goes by the hour - the computer's clock or the player's chosen moment
        SYSTEMTIME t{};
        GetLocalTime(&t);
        // 2.6.19: «От сервера» - the server's night made a minute, the day going down or coming up a minute long
        const int server = fx.clock == daylight::kClockServer
                               ? serverClock_.minute(serverNightNow(), double(GetTickCount64()) / 1000.0)
                               : -1;
        applyClock(fx, t.wHour * 60 + t.wMinute, server);
    }
    if (!test_.running())  // 3.0.5-beta.6: the test measures the sets as they are
        for (const std::string& k : keepOff_) effect_costs::drop(fx, k);  // 0.76
    // 1.9.3: Tab held in the console - the chosen effect off while it is held (the file as it is, that key 0)
    const std::string& comparing = console_.comparing();
    if (comparing != consoleOff_) {
        consoleOff_ = comparing;
        std::string text;
        readTextFile(ourFile(layout::File::Ini), text);
        if (!comparing.empty()) {  // 2.2: after the set and the groups - the switch as it comes out, then off
            EffectSettings file = parseConfig(text).effects;
            for (int g = 0; g < kEffectGroups; ++g)  // a group held: all of it off
                if (comparing == std::string("effects.group_") + effect_groups::groupWord(g)) file.groups[g] = false;
            compareFx_ = effect_groups::effective(file, cardLevel());
            if (const effect_groups::Member* fxm = effect_groups::memberOf(comparing)) compareFx_.*fxm->on = false;
        }
    }
    if (!consoleOff_.empty()) {
        const int night = fx.nightNow, day = fx.dayNow;
        fx = compareFx_;
        fx.nightNow = night;
        fx.dayNow = day;
    }
    renderer->setEffects(enhanced_ ? fx : effectsOff());  // Pause: the game's own picture
    serverNightFrame(enhanced_ && fx.noServerNight);      // 2.6.13: the server's night by the clock and «Ночь»
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
    o.fpsLimit = light::fpsLimit(config.fpsLimit, light_);  // 2.5.20: the battery's
    o.pacerSleeps = light_.pacerSleeps;
    const int idle = !foreground ? 15 : map_.empty() ? 30 : 0;
    if (config.savePower && idle && (o.fpsLimit <= 0 || o.fpsLimit > idle)) o.fpsLimit = idle;
    o.sharpen = enhanced_ && config.sharpen > 0 && !light_.noSharpen ? float(config.sharpen - 1) / 99.0f
                                                                     : -1.0f;  // 1-100 -> CAS 0..1
    o.foreground = foreground;
    o.vsyncSetting = config.vsync;
    o.screenshot = screenshot;
    o.hdr = config.hdr && enhanced_;  // 0.65: Pause shows the SDR picture
    // 0.91: quieter in the background; the crowd's limiter thinned out. 3.0.3-beta.3: the music of the main game
    // window alone, unless [windows] others_music
    soundFrame(foreground, mainWindow_ || config.windowOthersMusic, mainWindow_ || config.windowOthersSounds);
    notifier::frame(window, foreground, mainWindow_, config);  // 3.0.4-beta.3: whispers and the like from behind
    // 3.0.2-beta.13 (beta.12's measure: a map's 102 loading pictures waited 1.68 s for their presents, 60 a second):
    // the game's loading picture while a map loads - no frame limit, its present not waited for, and through
    // uaro-render64.exe at most 10 a second (the game draws it anew for each per cent of the map's models)
    bool shown = true;
    if (!waitForVblank && (mapStarting() || mapLoad_.loading())) {  // 3.0.2-beta.15: inside the game's map start
        o.fpsLimit = 0;
        o.loading = true;
        shown = !renderer->remote() || loadShownMs_ <= 0.0 || frameMs_ - loadShownMs_ >= 100.0;
        if (shown) {
            loadShownMs_ = frameMs_;
            ++loadShown_;
        }
    }
    if (shown) renderer->present(window, out.cx, out.cy, source, o);
    windowSwitch_.presented(light_.other);  // 3.0.3-beta.9: asked to be shown next - its whole picture answers
    if (!firstPicture_) {  // 2.12: the start went well (core/safe_mode)
        firstPicture_ = true;
        LOG_INFO("%s", safe::kFirstPicture);
    }
    GpuTiming gpuNow;  // 2.2.1: the card's work a frame - the counter's and the console's
    const bool timed = renderer->gpuTiming(gpuNow);
    rendererLoading_ = timed && gpuNow.loading;  // 3.0.6-beta.3: «Тест графіки» measures a set once it is done
    fps_.setGpu(timed ? &gpuNow : nullptr);
    fps_.present(window, fpsShown_ && !photo_);  // 3.0.4-beta.5: not in the photo mode's picture
    if (test_.running()) {  // 3.0.5-beta.6: the card's work of the set shown; the statistics leave the test out
        if (timed && gpuNow.serial != testGpuSerial_) {
            testGpuSerial_ = gpuNow.serial;
            test_.gpu(testClockS(), gpuNow.gpuMs, gpuNow.effects);
        }
    } else {
        noteUsage(foreground, out.cx, out.cy, timed ? &gpuNow : nullptr);  // 2.2.1
    }
    if (console_.present(window, foreground, ourFile(layout::File::Ini), config, map_,
                         timed ? &gpuNow : nullptr)) {  // 1.9.3; 2.6.22: the map for «Только на этой карте»
        settingsChecked_ = 0;  // read at once, at the next present
        settingsKnown_ = false;
    }
    if (console_.takeTestAsk()) testStart();  // 3.0.5-beta.6
    ++stats_.frames;
    logStats(false);
}

namespace {

// 3.0.2-beta.12: our cps.dll's count of what the game has inflated (files, bytes, microseconds; each wraps at 2^32);
// false with the game's own
bool inflatedSoFar(uint32_t out[3]) {
    using Fn = void(__cdecl*)(unsigned int out[3]);
    static Fn fn = nullptr;
    static bool looked = false;
    if (!looked) {
        looked = true;
        if (HMODULE cps = GetModuleHandleA("cps.dll"))
            fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(cps, "uagfx_inflated")));
    }
    unsigned int now[3] = {};
    if (!fn) return false;
    fn(now);
    for (int i = 0; i < 3; ++i) out[i] = now[i];
    return true;
}

}  // namespace

// 3.0.2-beta.12 (the client build session: prt_fild08 loads in 3.7 s, cps.dll's inflating 6 times as fast changed
// nothing): each map's loading measured by itself - from its loading picture to its world (core/map_load.hpp), the
// game's thread sampled meanwhile (Profiler), with the frames shown, the bridge's times and cps.dll's inflating over
// it; one block in the log at its end.
void Context::watchMapLoad(const std::string& map, bool world, bool picture) {
    LARGE_INTEGER now, f;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&f);
    frameMs_ = double(now.QuadPart) * 1000.0 / double(f.QuadPart);
    using Event = map_load::Watch::Event;
    const Event event = mapLoad_.frame(frameMs_, map, world, picture, mapStarting());
    if (event == Event::Began) {
        loadPictures_ = loadShown_ = loadGameFrames_ = 0;
        loadShownMs_ = 0.0;
        renderer->bridgeTimes(loadSent_, loadWaited_);
        loadInflatedKnown_ = inflatedSoFar(loadInflated_);
        pickCounts(loadPicks_, loadChecked_, loadTheirsMs_);
        Profiler::get().open(GetCurrentThreadId());
    }
    if ((event == Event::Began || event == Event::None) && mapLoad_.loading())
        ++(picture ? loadPictures_ : loadGameFrames_);  // 3.0.2-beta.13: the game's loading picture shows at once
    switch (event) {
    case Event::Began:
    case Event::None:
        break;
    case Event::Ended: {
        const std::string prefix = "map " + mapLoad_.map();
        char head[240], before[64] = "";
        if (mapLoad_.beforeMs() >= 0)
            std::snprintf(before, sizeof before, " (the old map's last frame %.2f s before it)",
                          mapLoad_.beforeMs() / 1000.0);
        std::snprintf(head, sizeof head,
                      "loaded in %.2f s from its loading picture to its world%s; %u loading pictures (%u shown) and "
                      "%u game frames meanwhile",
                      mapLoad_.loadMs() / 1000.0, before, loadPictures_, loadShown_, loadGameFrames_);
        std::vector<std::string> more;
        char line[240];
        if (renderer->remote()) {
            double sent = 0.0, waited = 0.0;
            renderer->bridgeTimes(sent, waited);
            std::snprintf(line, sizeof line,
                          "%s: the bridge - %.2f s the frames' draws written and sent, %.2f s waiting for the 64-bit "
                          "renderer's presents",
                          prefix.c_str(), (sent - loadSent_) / 1000.0, (waited - loadWaited_) / 1000.0);
            more.push_back(line);
        }
        uint32_t inflated[3] = {};
        if (loadInflatedKnown_ && inflatedSoFar(inflated)) {
            std::snprintf(line, sizeof line, "%s: cps.dll inflated %u files, %.1f MB in %.2f s", prefix.c_str(),
                          inflated[0] - loadInflated_[0], double(inflated[1] - loadInflated_[1]) / 1048576.0,
                          double(inflated[2] - loadInflated_[2]) / 1e6);
            more.push_back(line);
        }
        unsigned picks = 0, checked = 0;
        double theirsMs = 0.0;
        pickCounts(picks, checked, theirsMs);
        if (picks != loadPicks_) {  // 3.0.2-beta.13
            std::snprintf(line, sizeof line,
                          "%s: the cell under the mouse - %u picks, %u of them checked against the game's own (%.2f s)",
                          prefix.c_str(), picks - loadPicks_, checked - loadChecked_,
                          (theirsMs - loadTheirsMs_) / 1000.0);
            more.push_back(line);
        }
        Profiler::get().close(prefix, head, more);
        break;
    }
    case Event::Dropped:
        Profiler::get().drop();
        if (*mapLoad_.droppedWhy())  // 3.0.2-beta.15: a map's start not measured says why
            LOG_INFO("map %s: its loading not measured - %s", mapLoad_.map().c_str(), mapLoad_.droppedWhy());
        break;
    }
}

// 0.76: every half minute the effects' costs into uaro-graphics\effect-costs.txt (the settings app shows them);
// [frames] keep_fps - the frames counted in five-second windows while the game is in front with every improvement on;
// two slow windows in a row (under 90 % of it) turn the costliest effect off for the rest of the game.
void Context::keepFrameRate(const Config& config, HWND window) {
    const uint64_t now = GetTickCount64();
    if (keepNoteUntil_ && now >= keepNoteUntil_) {  // 3.0.4-beta.11: its line said long enough
        keepNoteUntil_ = 0;
        if (enhanced_) fps_.setNote(L"");
    }
    effect_costs::Costs costs;
    const bool measured = renderer && renderer->effectCosts(costs);
    if (measured && now - costsWritten_ >= 30000) {
        costsWritten_ = now;  // whole: the settings app may read it this moment
        writeFileAtomic(gameDir() + "\\uaro-graphics\\effect-costs.txt", effect_costs::format(costs));
    }
    if (config.keepFps <= 0) {
        keepOff_.clear();  // turned off: the effects come back (3.0.4-beta.35: the windows go on for the card's judge)
        keepSlow_ = 0;
    }
    // 3.0.5-beta.6: nor one of «Тест графіки» - its sets change every 15 s, and it turns nothing off
    if (!enhanced_ || !window || GetForegroundWindow() != window || IsIconic(window) || test_.running()) {
        keepSince_ = 0;  // a frame rate in the background says nothing
        return;
    }
    float seen[12];  // 3.0.4-beta.35: a map shown all the window long - the login's screens draw next to nothing
    const bool onMap = !map_.empty() && readGameCamera(seen);
    if (!keepSince_) {
        keepSince_ = now;
        keepFrames_ = 0;
        keepOnMap_ = onMap;
        return;
    }
    ++keepFrames_;
    keepOnMap_ = keepOnMap_ && onMap;
    if (now - keepSince_ < 5000) return;
    const double fps = double(keepFrames_) * 1000.0 / double(now - keepSince_);
    const bool windowOnMap = keepOnMap_;
    keepSince_ = now;
    keepFrames_ = 0;
    keepOnMap_ = onMap;
    GpuTiming gpu;  // 2.2.2: the card's own work - a game slow for the processor's part keeps its effects
    const double gpuMs = renderer && renderer->gpuTiming(gpu) ? gpu.gpuMs : -1.0;
    if (windowOnMap) {
        judgeCard(config, fps, gpuMs);
        noteFirstGame(config, fps);  // 3.0.11-beta.61
    }
    if (config.keepFps <= 0) return;
    keepSlow_ = effect_costs::slowForCard(fps, config.keepFps, gpuMs) ? keepSlow_ + 1 : 0;
    if (keepSlow_ < 2) return;
    keepSlow_ = 0;
    EffectSettings fx = effects();
    for (const std::string& k : keepOff_) effect_costs::drop(fx, k);
    std::set<std::string> passed = keepOff_;  // 3.0.4-beta.11: and what the player turned on in this game
    passed.insert(keepByHand_.begin(), keepByHand_.end());
    // 3.0.5-beta.3: how far the card is over the frame keep_fps wants - the thinned grass goes first when the others
    // cannot make that up (R-UQKHFX)
    const double overMs = gpuMs >= 0.0 ? gpuMs - 0.85 * 1000.0 / double(config.keepFps) : -1.0;
    const std::string key = effect_costs::pickToDrop(fx, measured ? costs : effect_costs::Costs{}, passed, overMs);
    if (key.empty()) return;
    const double othersMs = key == "grass" && measured ? effect_costs::othersCost(fx, costs, passed) : 0.0;
    keepOff_.insert(key);
    effectsMade_ = false;  // 3.0.4-beta.35: the thinned grass is in the effects made
    const bool thin = key == "grass_thin";  // 3.0.4-beta.35: the grass thinned, not off
    const double ms = effect_costs::costOf(costs, thin ? "grass" : key);
    char cost[48] = "", card[48] = "";
    if (ms >= 0) std::snprintf(cost, sizeof cost, " (it cost %.2f ms)", ms);
    if (gpuMs >= 0) std::snprintf(card, sizeof card, ", the card %.1f ms a frame", gpuMs);
    if (thin)
        LOG_INFO("keep_fps: %.0f fps, under %d%s - the grass thinned for this game (its roots on every second row)%s",
                 fps, config.keepFps, card, cost);
    else if (othersMs > 0.0)  // 3.0.5-beta.3: the grass before the others - why
        LOG_INFO("keep_fps: %.0f fps, under %d%s - effects.grass off for this game%s, before the others: they cost "
                 "%.2f ms together, the card %.2f ms over", fps, config.keepFps, card, cost, othersMs, overMs);
    else
        LOG_INFO("keep_fps: %.0f fps, under %d%s - effects.%s off for this game%s", fps, config.keepFps, card,
                 key.c_str(), cost);
    // 3.0.4-beta.11: said on the screen too - the grass gone with nothing said looked like a fault (R-EQDYNE)
    const settings::IniSetting* s = settings::findSetting("effects." + key);
    const settings::Lang lang = startup().lang;
    const std::string name = s ? settings::tr(lang, s->label) : key;
    const std::string fpsNow = std::to_string(int(fps + 0.5)), target = std::to_string(config.keepFps);
    const settings::Text note = thin ? settings::Text::KeepFpsThinNote : settings::Text::KeepFpsOffNote;
    sayOnScreen(wideOf(thin ? settings::fmt(settings::tr(lang, note), {fpsNow, target})
                            : settings::fmt(settings::tr(lang, note), {name, fpsNow, target})));
}

// 3.0.4-beta.35: a line on the screen for 10 s (the frame counter's)
void Context::sayOnScreen(const std::wstring& note) {
    keepNote_ = note;
    keepNoteUntil_ = GetTickCount64() + 10000;
    if (enhanced_) fps_.setNote(keepNote_);
}

card_speed::Judge& Context::judge() const {
    if (!judgeMade_) {
        judge_ = card_speed::Judge(startup().cardLevel, startup().cardSettled);
        judgeMade_ = true;
    }
    return judge_;
}

// 3.0.4-beta.35 (the user: «у людей отключается трава на слабых видеокартах»): a window on a map for the card's judge
// (core/card_speed.hpp) - the automatic set alone has a card's level to move; its verdict written for the next games
// and the settings app (uaro-graphics\card-level.txt). 3.0.11-beta.58 (the autopilot): every card, the whole ladder
void Context::judgeCard(const Config& config, double fps, double gpuMs) {
    if (config.effects.preset != EffectPreset::Auto) return;
    const int target = config.fpsLimit > 0 ? config.fpsLimit : 60;
    const int before = judge().level();
    const card_speed::Step step = judge().window(fps, target, gpuMs, config.keepFps);
    if (step == card_speed::Step::None) return;
    effectsMade_ = false;
    keepSlow_ = 0;  // 3.0.11-beta.58: the set moved this window - keep_fps counts again from the next one
    const Startup& s = startup();
    const settings::Lang lang = s.lang;
    const int now = judge().level();
    const auto setName = [lang](int level) { return settings::tr(lang, settings::effectsSetText(level)); };
    switch (step) {
        case card_speed::Step::Raise:  // not written: a game ended on trial starts the next one on the set before
            LOG_INFO("card level: %s keeps up - its own work %.1f ms a frame of %.1f, %.0f fps, for a minute on the "
                     "maps: the set %d on trial", s.adapter.c_str(), gpuMs, 1000.0 / double(target), fps, now);
            sayOnScreen(wideOf(settings::fmt(settings::tr(lang, settings::Text::CardRaisedNote), {setName(now)})));
            return;
        case card_speed::Step::Keep:
            LOG_INFO("card level: %s - the set %d kept (three minutes on it without the card holding the game back)%s",
                     s.adapter.c_str(), now, judge().settled() ? ", judged" : "");
            break;
        case card_speed::Step::Back:
            LOG_INFO("card level: %s - %.0f fps, under %d with the card %.1f ms a frame: the set %d too much, back to "
                     "%d, judged", s.adapter.c_str(), fps, config.keepFps > 0 ? config.keepFps : target, gpuMs, before,
                     now);
            sayOnScreen(wideOf(settings::fmt(settings::tr(lang, settings::Text::CardLoweredNote),
                                             {setName(before), setName(now)})));
            break;
        case card_speed::Step::Lower:
            LOG_INFO("card level: %s - holding the game back half a minute (%.0f fps, the card %.1f ms a frame): the "
                     "set %d -> %d, judged", s.adapter.c_str(), fps, gpuMs, before, now);
            sayOnScreen(wideOf(settings::fmt(settings::tr(lang, settings::Text::CardSlowNote),
                                             {setName(before), setName(now)})));
            break;
        case card_speed::Step::None: return;
    }
    writeFileAtomic(gameDir() + "\\uaro-graphics\\card-level.txt",
                    card_speed::format({now, judge().settled(), s.adapter}));
}

// 3.0.11-beta.61 (the autopilot): the first game's first minute on the maps - its windows' median frames and the
// effects set then, into the outcome written at its start (an install's first game only: no file, nothing)
void Context::noteFirstGame(const Config& config, double fps) {
    if (firstGameNoted_) return;
    firstMapFps_.push_back(fps);
    if (firstMapFps_.size() < size_t(card_speed::kRaiseWindows)) return;
    firstGameNoted_ = true;
    const std::string path = layout::join(gameDir(), layout::kFirstGame);
    std::string text;
    firstgame::Outcome o;
    if (!readTextFile(path, text) || !firstgame::parse(text, o) || o.map || firstgame::complete(o)) return;
    std::vector<double> v = firstMapFps_;
    std::nth_element(v.begin(), v.begin() + std::ptrdiff_t(v.size() / 2), v.end());
    o.map = true;
    o.fps = int(v[v.size() / 2] + 0.5);
    o.level = effect_groups::setLevel(config.effects.preset, cardLevel());
    writeFileAtomic(path, firstgame::format(o));
    LOG_INFO("first game: a minute on the maps - %d frames a second (the median), the effects set %d", o.fps, o.level);
}

void Context::watchSettings() {
    const uint64_t now = GetTickCount64();
    if (settingsChecked_ && now - settingsChecked_ < 1000) return;
    settingsChecked_ = now;
    const std::string path = ourFile(layout::File::Ini);
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
    settingsChanged();
    // 3.0.4-beta.11: an effect the player turns on now is theirs for this game - keep_fps leaves it (and gives back
    // one it had turned off)
    for (const std::string& k : effect_costs::turnedOn(was.effects, config.effects)) {
        keepByHand_.insert(k);
        if (keepOff_.erase(k)) LOG_INFO("keep_fps: effects.%s turned on again by the player - back on", k.c_str());
    }
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
    if (config.fullColour != was.fullColour) setFullColour(config.fullColour);  // 2.5.18: the textures loaded next
    if (config.aiNow.dyes != was.aiNow.dyes) setSpriteDyes(config.aiNow.dyes);  // 2.5.23: the frames written next
    if (config.logLevel != was.logLevel) wlog::setLevel(config.logLevel);
    if (config.fpsCounter != was.fpsCounter) fpsShown_ = config.fpsCounter;  // 2.2.1: at once
    if (config.sound.output != was.sound.output) soundOutput(config.sound.output);  // 3.0.4-beta.1
    if (config.sound.ambience != was.sound.ambience || config.sound.ambienceVolume != was.sound.ambienceVolume)
        audio::configureAmbience(config.sound.ambience, config.sound.ambienceVolume);  // 3.0.4-beta.7
    if (config.statsSend && usage_.begun()) {  // 2.2.1: what was changed in the game - what was liked, or not
        std::string lines;  // 2.2.2: the effects by what is drawn (a set made the player's own changes nothing)
        for (const std::string& a : usage::changes(was.effects, config.effects, cardLevel(), u.applied))
            usage_.change(a, lines);
        writeUsage(lines);
    } else if (!config.statsSend && was.statsSend && usage_.begun()) {
        std::string lines;
        usage_.close(lines);  // the leave taken back: the stretch so far, then nothing more
        writeUsage(lines);
    }
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
    if (stats_.cpuMs < 0.0) stats_.cpuMs = threadCpuMs();
    const DWORD ms = now - stats_.since;
    if (!final && ms < 30000) return;
    // 2.5.15: how much of a core the game's thread had - its own work and ours in it, not its waits
    char busy[96] = "";
    if (ms && stats_.frames)
        std::snprintf(busy, sizeof busy, "; the game's thread busy %.0f %% of a core (%.1f ms a frame)",
                      (threadCpuMs() - stats_.cpuMs) * 100.0 / ms, (threadCpuMs() - stats_.cpuMs) / stats_.frames);
    char full[96] = "";  // 2.5.18
    if (fullColourTextures())
        std::snprintf(full, sizeof full, "; %u textures written in full colour so far (%u MB waiting)",
                      fullColourTextures(), unsigned(TextureLevels::fullBytes() >> 20));
    if (stats_.frames || stats_.readBacks)
        LOG_INFO("%u frames in %.1f s (%.1f fps), %u reads of the frame from the GPU%s%s%s", stats_.frames,
                 ms / 1000.0, ms ? stats_.frames * 1000.0 / ms : 0.0, stats_.readBacks, crisp::stats().c_str(), busy,
                 full);
    // 3.0.11-beta.55 (R-9PP79T): the new interface's pictures painted meanwhile, and what they cost
    if (const std::string paints = uicost::take(); !paints.empty())
        LOG_INFO("interface: pictures painted in %.0f s - %s", ms / 1000.0, paints.c_str());
    stats_ = Stats{};
    stats_.since = now;
    stats_.cpuMs = threadCpuMs();
}

}  // namespace uagfx::win
