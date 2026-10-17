// uaro-render64.exe (part 3): draws for the 32-bit driver in uaRO.exe with Direct3D 12 in a 64-bit process, so that
// what exists only for 64-bit programs - DLSS, frame generation - can come next. The driver starts it with
//   uaro-render64.exe --channel <shared block> --parent <game pid> --dir "<game folder>" --log <level>
//                     --present auto|window|composition
// and it ends with the game. 0.43: the settings app starts it to make the texture pack (host/pack_builder.hpp):
//   uaro-render64.exe --pack check|build --dir "<game folder>" --ai <[ai] settings> --kinds <what>
//                     [--quality 90] [--fresh 1] [--out "<pack>"] [--status "<file>"]
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/bridge_protocol.hpp"
#include "core/bridge_wire.hpp"
#include "core/version.hpp"
#include "host/pack_builder.hpp"
#include "host/streamline.hpp"
#include "win/bridge/channel.hpp"
#include "win/gpu/adapters.hpp"
#include "win/gpu/renderer.hpp"
#include "win/log.hpp"

using namespace uagfx;
using bridge::Msg;
using bridge::WireReader;
using bridge::WireWriter;

namespace {

struct Args {
    std::string channel;
    DWORD parent = 0;
    std::string dir = ".";
    int log = 1;
    std::string present = "auto";
    std::string dlss = "off";
    bool jitterInverted = false;
    bool motionInverted = false;
    int generation = 0;
    AiSettings ai;  // [ai]
    std::string gpu;  // 0.50.2: [renderer] gpu ("" the fast card)
    host::PackArgs pack;  // 0.43: --pack
};

// The driver's command line: words, "quoted words".
Args parseArgs() {
    std::vector<std::string> words;
    const char* p = GetCommandLineA();
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        std::string w;
        if (*p == '"') {
            for (++p; *p && *p != '"'; ++p) w += *p;
            if (*p) ++p;
        } else {
            for (; *p && *p != ' '; ++p) w += *p;
        }
        words.push_back(w);
    }
    Args a;
    for (size_t i = 1; i + 1 < words.size(); ++i) {
        const std::string& k = words[i];
        const std::string& v = words[i + 1];
        if (k == "--channel") a.channel = v;
        else if (k == "--parent") a.parent = DWORD(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--dir") a.dir = v;
        else if (k == "--log") a.log = std::atoi(v.c_str());
        else if (k == "--present") a.present = v;
        else if (k == "--dlss") a.dlss = v;
        else if (k == "--dlss-jitter") a.jitterInverted = v == "inverted";
        else if (k == "--dlss-motion") a.motionInverted = v == "inverted";
        else if (k == "--fg") a.generation = std::atoi(v.c_str());
        else if (k == "--ai") a.ai = parseAiArgument(v);
        else if (k == "--gpu") a.gpu = v == "auto" ? "" : v;
        else if (k == "--pack") a.pack.mode = v;
        else if (k == "--kinds") a.pack.choice = pack::parseChoice(v);
        else if (k == "--quality") a.pack.quality = std::clamp(std::atoi(v.c_str()), 1, 100);
        else if (k == "--fresh") a.pack.fresh = v == "1";
        else if (k == "--redo") {  // 1.6
            for (size_t from = 0; from <= v.size();) {
                size_t comma = v.find(',', from);
                if (comma == std::string::npos) comma = v.size();
                ai::Group g;
                if (ai::parseGroup(v.substr(from, comma - from), g)) a.pack.redo[size_t(g)] = true;
                from = comma + 1;
            }
        }
        else if (k == "--out") a.pack.out = v;
        else if (k == "--status") a.pack.status = v;
        else continue;
        ++i;
    }
    return a;
}

std::atomic<win::bridge::Channel*> g_channel{nullptr};

// Every log line also goes to the driver, which writes it into uaro-graphics.log with "render64:" in front - when
// there is room: a line never waits (it may come from Streamline's own thread while the main one waits to send).
void forward(LogLevel level, const char* text) {
    win::bridge::Channel* channel = g_channel.load();  // once: it goes at shutdown (0.10 review 10)
    if (!channel) return;
    std::vector<uint8_t> out;
    WireWriter w(out);
    w.u8(uint8_t(level));
    w.str(text);
    channel->trySend(Msg::Log, out);
}

// Part 3: DLSS without frame generation starts on a thread of its own once the device exists, so that the game
// shows its picture at once: NVIDIA's start takes some five seconds (0.10 logs), and the game's window stayed
// black all that time.
struct LateDlss {
    host::Dlss* dlss = nullptr;
    ID3D12Device* device = nullptr;
    std::string folder, logs, mode, why;
    std::atomic<int> state{0};  // 0 starting, 1 ready, 2 not there
    ULONGLONG started = 0;
    HANDLE thread = nullptr;
    bool done = false;  // the main thread has acted on the state
};

DWORD WINAPI startDlssLate(void* context) {
    auto* late = static_cast<LateDlss*>(context);
    std::string why;
    const bool ok = late->dlss->init(late->folder, late->logs, late->mode, 0, true, why) &&
                    late->dlss->attach(late->device, why);
    late->why = why;
    late->state.store(ok ? 1 : 2, std::memory_order_release);
    return 0;
}

class Host {
public:
    int run(const Args& a);

private:
    void dispatch(Msg type, const uint8_t* data, uint32_t size);
    void present(WireReader& rd);
    // AI icons (0.14): one made larger goes back to the driver.
    static void iconDone(void* self, uint64_t key, int width, int height, const uint32_t* argb);
    // 0.40: what the start does, to the driver: it shows it over the game's window until the Hello.
    static void startProgress(void* self, const start::Progress& p);
    void checkLateDlss();  // DLSS started in the background: on, or the reason it is not

    win::bridge::Channel channel_;
    gpu::Renderer renderer_;
    host::Dlss dlss_;  // after the renderer: Streamline shuts down before the device goes
    std::string presentMode_;
    bool surface_ = false;      // the driver gave a composition surface
    bool surfaceAsked_ = false;
    bool lostSent_ = false;
    int gameWidth_ = 0;
    int gameHeight_ = 0;
    FrameRecord record_;
    std::vector<uint32_t> pixels_;
    std::vector<uint8_t> out_;
    std::vector<uint8_t> iconOut_;  // its own: an icon comes back in the middle of other work
    std::vector<uint8_t> startOut_;  // 0.40
    LateDlss late_;
};

void Host::startProgress(void* self, const start::Progress& p) {
    Host& h = *static_cast<Host*>(self);
    h.startOut_.clear();
    WireWriter w(h.startOut_);
    w.u8(uint8_t(p.stage));
    w.i32(p.done);
    w.i32(p.total);
    h.channel_.send(Msg::StartProgress, h.startOut_);
}

void Host::iconDone(void* self, uint64_t key, int width, int height, const uint32_t* argb) {
    Host& h = *static_cast<Host*>(self);
    h.iconOut_.clear();
    WireWriter w(h.iconOut_);
    w.u64(key);
    w.i32(width);
    w.i32(height);
    if (width > 0 && height > 0 && argb) w.bytes(argb, size_t(width) * size_t(height) * 4);  // 0x0: there is none
    h.channel_.send(Msg::IconDone, h.iconOut_);
}

int Host::run(const Args& a) {
    presentMode_ = a.present;
    std::string error;
    if (!channel_.open(a.channel, error)) {
        LOG_ERROR("bridge: %s", error.c_str());
        return 2;
    }
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, a.parent);
    channel_.setPeer(parent);
    g_channel = &channel_;
    wlog::setSink(forward);
    // Part 3: DLSS. Frame generation takes over the swap chain, so with it Streamline starts before the first
    // Direct3D call of the process and Direct3D 12 and DXGI come through its interposer. Without it Streamline starts
    // in the background once the device exists (LateDlss) and takes over only the DXGI factory - still seeing every
    // present (0.10 log: "presentCommon() was not observed": its bookkeeping and clean-up run there).
    const std::string folder = a.dir + "\\uaro-graphics\\dlss";
    const bool wantDlss = a.dlss != "off";
    const bool early = wantDlss && a.generation >= 2;
    bool dlss = false;
    struct Attach {
        host::Dlss* dlss;
        bool ok;
        std::string why;
    } attach{&dlss_, false, "the device was not made"};
    dlss_.setSigns(a.jitterInverted ? -1.0f : 1.0f, a.motionInverted ? -1.0f : 1.0f);
    if (early) {
        startProgress(this, start::Progress{start::Stage::Dlss, 0, 0});  // 0.40
        std::string why;
        dlss = dlss_.init(folder, a.dir, a.dlss, a.generation, false, why);
        if (dlss) {
            renderer_.setApi(dlss_.module());
            renderer_.setOnDevice(
                [](void* context, ID3D12Device* device) {
                    auto* at = static_cast<Attach*>(context);
                    at->ok = at->dlss->attach(device, at->why);
                },
                &attach);
        }
        if (!dlss) LOG_ERROR("DLSS: %s; drawing without it", why.c_str());
    }
    out_.clear();
    WireWriter hello(out_);
    renderer_.setShaderCache(a.dir + "\\uaro-graphics\\shaders-64.bin");  // 0.39.5
    renderer_.setStartSink(&Host::startProgress, this);                         // 0.40
    if (!renderer_.create(error)) {
        LOG_ERROR("Direct3D 12: %s", error.c_str());
        hello.u8(0);
        hello.str(error);
        channel_.send(Msg::Hello, out_);
        return 3;
    }
    LOG_INFO("uaro-render64 %s (64-bit) draws on %s", kVersion, renderer_.adapterName().c_str());
    renderer_.setIconSink(&Host::iconDone, this);
    if (a.ai.on()) {
        startProgress(this, start::Progress{start::Stage::AiTextures, 0, 0});  // 0.40
        renderer_.startAiTextures(a.dir, a.ai);  // its own log line, on or why not
    }
    renderer_.setGameDir(a.dir);  // 0.25: the maps' files and the axes file
    bool upscales = false;
    if (dlss) {  // Streamline got the device the moment it was made (setOnDevice)
        if (attach.ok) {
            renderer_.setUpscaler(&dlss_);
            upscales = true;
            LOG_INFO("%s ready (NVIDIA DLSS through Streamline)", dlss_.name());
        } else {
            LOG_ERROR("DLSS: %s; drawing without it", attach.why.c_str());
        }
    }
    hello.u8(1);
    hello.str(renderer_.adapterName());
    hello.u8(upscales ? 1 : 0);  // the driver keeps MSAA and render_scale when DLSS is not there (0.8 review M4)
    channel_.send(Msg::Hello, out_);
    if (wantDlss && !early) {
        late_.dlss = &dlss_;
        late_.device = renderer_.device();
        late_.folder = folder;
        late_.logs = a.dir;
        late_.mode = a.dlss;
        late_.started = GetTickCount64();
        late_.thread = CreateThread(nullptr, 0, startDlssLate, &late_, 0, nullptr);
        if (!late_.thread)
            LOG_ERROR("DLSS: its start could not be run (error %lu); drawing without it", GetLastError());
    }
    for (;;) {
        Msg type;
        const uint8_t* data = nullptr;
        uint32_t size = 0;
        // The background start is looked at between frames: after a present, or while nothing comes.
        const DWORD wait = late_.thread && !late_.done ? 100 : INFINITE;
        if (!channel_.receive(type, data, size, wait)) {
            if (wait == INFINITE || channel_.peerGone()) break;  // the game has gone
            checkLateDlss();
            continue;
        }
        if (type == Msg::Quit) break;
        dispatch(type, data, size);
        if (type == Msg::Present) checkLateDlss();
        if (renderer_.lost() && !lostSent_) {
            lostSent_ = true;
            out_.clear();
            WireWriter(out_).str("the GPU device was removed");
            channel_.send(Msg::Lost, out_);
        }
    }
    if (late_.thread) {  // a start still running when the game closes: nothing is taken down under it
        if (WaitForSingleObject(late_.thread, 10000) != WAIT_OBJECT_0) ExitProcess(0);
        CloseHandle(late_.thread);
    }
    renderer_.finish();  // nothing of DLSS's still on the GPU when Streamline shuts down
    wlog::setSink(nullptr);
    g_channel = nullptr;
    if (parent) CloseHandle(parent);
    return 0;
}

void Host::checkLateDlss() {
    if (!late_.thread || late_.done) return;
    const int state = late_.state.load(std::memory_order_acquire);
    if (state == 0) return;
    late_.done = true;
    if (state == 2) {
        LOG_ERROR("DLSS: %s; drawing without it", late_.why.c_str());
        return;
    }
    if (!renderer_.upgradeFactory(&host::Dlss::upgrade)) {
        LOG_ERROR("DLSS: Streamline could not take over the DXGI factory; drawing without it");
        return;
    }
    renderer_.setUpscaler(&dlss_);
    channel_.send(Msg::UpscalerReady);  // the driver turns it on at its next present
    LOG_INFO("%s ready (NVIDIA DLSS through Streamline, started alongside the game in %.1f s)", dlss_.name(),
             double(GetTickCount64() - late_.started) / 1000.0);
}

void Host::dispatch(Msg type, const uint8_t* data, uint32_t size) {
    WireReader rd(data, size);
    out_.clear();
    WireWriter w(out_);
    switch (type) {
        case Msg::SetUiSharp: renderer_.setUiSharp(rd.u8() != 0); break;
        case Msg::PreparePipelines: renderer_.preparePipelines(rd.str()); break;
        case Msg::SetTarget: {
            gameWidth_ = rd.i32();
            gameHeight_ = rd.i32();
            w.u8(rd.ok() && renderer_.setTarget(gameWidth_, gameHeight_) ? 1 : 0);
            channel_.send(Msg::TargetSet, out_);
            break;
        }
        case Msg::SetScale: {
            const double k = rd.f64();
            const int width = rd.i32();
            const int height = rd.i32();
            const int samples = rd.i32();
            const bool upscale = rd.u8() != 0;
            w.u8(rd.ok() && renderer_.setScale(k, width, height, samples, upscale) ? 1 : 0);
            w.i32(renderer_.samples());
            channel_.send(Msg::ScaleSet, out_);
            break;
        }
        case Msg::SetDrawQuality: {
            const int aniso = rd.i32();
            const bool coverage = rd.u8() != 0;
            const bool aiShown = rd.u8() != 0;
            renderer_.setDrawQuality(aniso, coverage, aiShown);
            break;
        }
        case Msg::SetEffects: {
            EffectSettings e;
            e.glow = rd.u8() != 0;
            e.glowStrength = rd.i32();
            e.glowSize = rd.i32();
            e.glowLight = rd.i32();
            e.ao = rd.u8() != 0;
            e.aoStrength = rd.i32();
            e.aoSize = rd.i32();
            e.water = rd.u8() != 0;
            e.waterWaves = rd.i32();
            e.waterReflections = rd.i32();
            e.waterClarity = rd.i32();
            e.grade = rd.u8() != 0;
            e.gradeSaturation = rd.i32();
            e.gradeContrast = rd.i32();
            e.gradeBrightness = rd.i32();
            e.gradeWarmth = rd.i32();
            e.gradeVignette = rd.i32();
            e.dof = rd.u8() != 0;
            e.dofStrength = rd.i32();
            e.dofRange = rd.i32();
            e.smoothLight = rd.u8() != 0;
            e.torch = rd.u8() != 0;
            e.torchGlow = rd.i32();
            e.torchLight = rd.i32();
            e.torchFlicker = rd.i32();
            e.torchFlame = rd.u8() != 0;
            e.torchBrightness = rd.i32();
            e.smoke = rd.u8() != 0;
            e.smokeDensity = rd.i32();
            e.lamps = rd.u8() != 0;
            e.lampsLight = rd.i32();
            e.fireLight = rd.u8() != 0;
            e.fireLightStrength = rd.i32();
            e.fireflies = rd.u8() != 0;
            e.firefliesBrightness = rd.i32();
            e.mapGlows = rd.u8() != 0;
            e.mapGlowsBrightness = rd.i32();
            e.rays = rd.u8() != 0;
            e.raysBrightness = rd.i32();
            e.lava = rd.u8() != 0;
            e.lavaBrightness = rd.i32();
            e.actorShadows = rd.u8() != 0;
            e.actorShadowsStrength = rd.i32();
            e.clouds = rd.u8() != 0;
            e.cloudsStrength = rd.i32();
            e.haze = rd.u8() != 0;
            e.hazeStrength = rd.i32();
            e.heat = rd.u8() != 0;
            e.heatStrength = rd.i32();
            e.skillColours = rd.u8() != 0;
            e.portals = rd.u8() != 0;
            e.portalsStrength = rd.i32();
            e.weather = rd.u8() != 0;
            e.weatherAmount = rd.i32();
            e.night = rd.u8() != 0;  // 0.63
            e.nightStrength = rd.i32();
            e.nightClock = rd.u8() != 0;
            e.nightNow = rd.i32();
            e.foliage = rd.u8() != 0;  // 0.64
            e.foliageStrength = rd.i32();
            e.softParticles = rd.u8() != 0;  // 0.67
            e.rimLight = rd.u8() != 0;  // 0.68
            e.rimStrength = rd.i32();
            e.sky = rd.u8() != 0;  // 0.69
            e.skyClouds = rd.i32();
            e.skyStrength = rd.i32();
            e.motes = rd.u8() != 0;  // 0.70
            e.motesAmount = rd.i32();
            e.relief = rd.u8() != 0;  // 0.71
            e.reliefStrength = rd.i32();
            e.floorReflections = rd.u8() != 0;  // 0.72
            e.floorReflectStrength = rd.i32();
            e.shafts = rd.u8() != 0;  // 0.73
            e.shaftsStrength = rd.i32();
            e.dayCycle = rd.u8() != 0;  // 0.74
            e.dayNow = rd.i32();
            e.sunShadows = rd.u8() != 0;  // 0.77
            e.sunShadowsStrength = rd.i32();
            e.bloom = rd.u8() != 0;  // 0.78
            e.bloomStrength = rd.i32();
            e.filmic = rd.u8() != 0;
            e.regionLooks = rd.u8() != 0;  // 0.79
            e.regionLooksStrength = rd.i32();
            e.footprints = rd.u8() != 0;  // 0.80
            e.storms = rd.u8() != 0;
            e.grass = rd.u8() != 0;  // 0.81
            e.grassAmount = rd.i32();
            e.skillSparks = rd.u8() != 0;  // 0.82
            e.skillSparksAmount = rd.i32();
            e.filmGrain = rd.u8() != 0;  // 0.83
            e.filmGrainAmount = rd.i32();
            e.mapFade = rd.u8() != 0;
            e.roundLight = rd.u8() != 0;  // 0.84
            e.appear = rd.u8() != 0;  // 0.93
            e.ripples = rd.u8() != 0;  // 0.94
            e.glint = rd.u8() != 0;  // 0.95
            e.hitFlash = rd.u8() != 0;  // 0.96
            e.wetGround = rd.u8() != 0;  // 0.97
            e.flare = rd.u8() != 0;  // 0.98
            e.treeVolume = rd.u8() != 0;  // 1.7
            e.leafLight = rd.u8() != 0;
            e.treeTints = rd.u8() != 0;
            e.gusts = rd.u8() != 0;
            e.dapple = rd.u8() != 0;
            e.flowers = rd.u8() != 0;  // 1.7.1
            e.dew = rd.u8() != 0;
            e.morningFog = rd.u8() != 0;
            e.sandDrift = rd.u8() != 0;
            e.snowSparkle = rd.u8() != 0;
            e.wetWalls = rd.u8() != 0;
            e.shoreFoam = rd.u8() != 0;  // 1.7.2
            e.fish = rd.u8() != 0;
            e.swamp = rd.u8() != 0;
            e.butterflies = rd.u8() != 0;  // 1.7.3
            e.birds = rd.u8() != 0;
            e.leafFall = rd.u8() != 0;
            e.embers = rd.u8() != 0;
            e.breath = rd.u8() != 0;
            e.splashes = rd.u8() != 0;
            e.caveDrips = rd.u8() != 0;
            e.underwater = rd.u8() != 0;
            e.materials = rd.u8() != 0;  // 1.8
            e.materialsStrength = rd.i32();
            e.tier = rd.i32();
            e.groundSeams = rd.u8() != 0;  // 1.9
            if (rd.ok()) renderer_.setEffects(e);
            break;
        }
        case Msg::CreateTexture: {
            const uint32_t id = rd.u32();
            const int width = rd.i32();
            const int height = rd.i32();
            const int levels = rd.i32();
            if (rd.ok() && !renderer_.createTextureAs(id, width, height, levels))
                LOG_ERROR("texture %u (%dx%d, %d levels) not made", id, width, height, levels);
            break;
        }
        case Msg::ReleaseTexture: renderer_.releaseTexture(rd.u32()); break;
        case Msg::ObjectTexture: {
            const uint32_t id = rd.u32();
            const uint8_t kind = rd.u8();
            if (rd.ok()) renderer_.noteObjectTexture(id, kind);
            break;
        }
        case Msg::Execute: {
            if (bridge::readRecord(rd, record_)) {
                renderer_.execute(record_);
            } else {
                static bool logged = false;
                if (!logged) LOG_ERROR("bridge: a damaged frame record was skipped");
                logged = true;
            }
            break;
        }
        case Msg::ReadTarget: {
            const bool ok = renderer_.readTarget(pixels_) && pixels_.size() == size_t(gameWidth_) * gameHeight_;
            w.u8(ok ? 1 : 0);
            w.i32(gameWidth_);
            w.i32(gameHeight_);
            if (ok) w.bytes(pixels_.data(), pixels_.size() * 4);
            channel_.send(Msg::TargetPixels, out_);
            break;
        }
        case Msg::Present: present(rd); break;
        case Msg::Camera: {
            const bool valid = rd.u8() != 0;
            float view[12];
            for (float& v : view) v = rd.f32();
            const float targetZ = rd.left() >= 4 ? rd.f32() : 0.0f;  // 1.4.2: the character's depth
            if (rd.ok()) renderer_.setCamera(valid, view, targetZ);
            break;
        }
        case Msg::IconWanted: {
            const uint64_t key = rd.u64();
            const int w = rd.i32(), h = rd.i32();
            if (!rd.ok() || w <= 0 || h <= 0 || w > 512 || h > 512) break;
            const uint8_t* bytes = rd.take(size_t(w) * size_t(h) * 4);
            if (!bytes) break;
            std::vector<uint32_t> argb(size_t(w) * size_t(h));
            std::memcpy(argb.data(), bytes, argb.size() * 4);
            renderer_.requestIcon(key, w, h, argb.data());
            break;
        }
        case Msg::Map: {
            const std::string map = rd.str();
            if (rd.ok()) renderer_.setMap(map);
            break;
        }
        case Msg::SetSurface: {
            const uint64_t handle = rd.u64();
            if (!rd.ok()) break;
            renderer_.setPresentSurface(HANDLE(uintptr_t(handle)));
            surface_ = true;
            LOG_INFO("bridge: drawing into the game window's composition surface");
            break;
        }
        default: break;
    }
}

void Host::present(WireReader& rd) {
    const HWND window = HWND(uintptr_t(rd.u64()));
    const int width = rd.i32();
    const int height = rd.i32();
    const Rect source = rd.rect();
    gpu::PresentOptions o;
    o.vsync = rd.u8() != 0;
    o.allowTearing = rd.u8() != 0;
    o.fpsLimit = rd.i32();
    o.sharpen = rd.f32();
    o.foreground = rd.u8() != 0;
    o.screenshot = rd.str();  // 0.54
    o.hdr = rd.u8() != 0;     // 0.65
    if (rd.ok() && (surface_ || presentMode_ != "composition")) {
        renderer_.present(window, width, height, source, o);
        if (!surface_ && renderer_.presentRefused() && !surfaceAsked_ && presentMode_ != "window") {
            surfaceAsked_ = true;
            LOG_INFO("bridge: the game's window takes no swap chain from this process; "
                     "asking for a composition surface");
            channel_.send(Msg::NeedSurface);
        }
    } else {
        Sleep(1);  // no way to show it yet: the surface is on its way
    }
    channel_.notifyPresented();
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Args a = parseArgs();
    gpu::setChosenAdapter(a.gpu);  // 0.50.2: the game's card, and the texture pack's
    if (!a.pack.mode.empty()) {  // 0.43: the texture pack, with a log of its own
        wlog::open(a.dir + "\\uaro-pack.log", LogLevel(a.log < 0 ? 0 : a.log > 2 ? 2 : a.log));
        a.pack.dir = a.dir;
        a.pack.ai = a.ai;
        a.pack.choice.minSide = std::min(a.ai.minSize, a.ai.maxSize);
        a.pack.choice.maxSide = std::max(a.ai.minSize, a.ai.maxSize);
        return host::runPack(a.pack);
    }
    wlog::open(a.dir + "\\uaro-render64.log", LogLevel(a.log < 0 ? 0 : a.log > 2 ? 2 : a.log));
    if (a.channel.empty() || !a.parent) {
        LOG_ERROR("uaro-render64.exe is started by the uaRO Graphics driver in the game, not by hand");
        return 1;
    }
    Host host;
    return host.run(a);
}
