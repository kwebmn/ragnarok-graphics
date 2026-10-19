// uaro-render64.exe (part 3): draws for the 32-bit driver in uaRO.exe with Direct3D 12 in a 64-bit process, so that
// what exists only for 64-bit programs - DLSS, frame generation - can come next. The driver starts it with
//   uaro-render64.exe --channel <shared block> --parent <game pid> --dir "<game folder>" --log <level>
//                     --present auto|window|composition
// and it ends with the game. 0.43: the settings app starts it to make the texture pack (host/pack_builder.hpp):
//   uaro-render64.exe --pack build|drop --dir "<game folder>" --ai <[ai] settings> --kinds <what>
//                     [--quality 90] [--fresh 1] [--out "<pack>"] [--status "<file>"]
//                     [--map <map> --map-kinds ground,models --remake 1] (2.2.5: one map's textures)
//                     [--colours 8,77] (2.2.5: the most hair and clothes colours made; -1 every one)
// 2.3: uaRO Studio starts a copy of it as its engine (host/studio_engine.hpp):
//   uaro-render64.exe --studio <pipe> --dir "<game folder>"
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
#include "host/studio_engine.hpp"
#include "host/streamline.hpp"
#include "win/bridge/channel.hpp"
#include "win/bridge/serve.hpp"
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
    int dlssScale = 75;  // 2.4.2: dlss = custom - its share of the screen, or its size (0: by the share)
    int dlssWidth = 0, dlssHeight = 0;
    std::string dlssModel = "auto";
    bool jitterInverted = false;
    bool motionInverted = false;
    int generation = 0;
    bool reflex = true;  // 2.5.26: [frames] reflex
    AiSettings ai;  // [ai]
    std::string gpu;  // 0.50.2: [renderer] gpu ("" the fast card)
    host::PackArgs pack;  // 0.43: --pack
    std::string studio;   // 2.3: --studio <pipe>, uaRO Studio's engine
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
    int colours[2] = {-2, -2};  // 2.2.5: none given
    for (size_t i = 1; i + 1 < words.size(); ++i) {
        const std::string& k = words[i];
        const std::string& v = words[i + 1];
        if (k == "--channel") a.channel = v;
        else if (k == "--parent") a.parent = DWORD(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "--dir") a.dir = v;
        else if (k == "--log") a.log = std::atoi(v.c_str());
        else if (k == "--present") a.present = v;
        else if (k == "--dlss") a.dlss = v;
        else if (k == "--dlss-scale") a.dlssScale = std::clamp(std::atoi(v.c_str()), 33, 100);
        else if (k == "--dlss-size") {
            const size_t x = v.find('x');
            a.dlssWidth = x == std::string::npos ? 0 : std::max(0, std::atoi(v.substr(0, x).c_str()));
            a.dlssHeight = x == std::string::npos ? 0 : std::max(0, std::atoi(v.substr(x + 1).c_str()));
        } else if (k == "--dlss-model") a.dlssModel = v;
        else if (k == "--dlss-jitter") a.jitterInverted = v == "inverted";
        else if (k == "--dlss-motion") a.motionInverted = v == "inverted";
        else if (k == "--fg") a.generation = std::atoi(v.c_str());
        else if (k == "--reflex") a.reflex = v != "0";
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
                ai::Family f;  // 2.5.28: a family's word (of before) - its groups
                const std::string word = v.substr(from, comma - from);
                if (ai::parseGroup(word, g)) {
                    a.pack.redo[size_t(g)] = true;
                } else if (ai::parseFamily(word, f)) {
                    for (int i = 0; i < ai::kGroups; ++i)
                        if (ai::familyOf(ai::Group(i)) == f) a.pack.redo[size_t(i)] = true;
                }
                from = comma + 1;
            }
        }
        else if (k == "--map") a.pack.map = v;  // 2.2.5: one map's textures
        else if (k == "--map-kinds") {
            a.pack.mapGround = v.find("ground") != std::string::npos;
            a.pack.mapModels = v.find("models") != std::string::npos;
        }
        else if (k == "--remake") a.pack.remake = v == "1";
        else if (k == "--colours") {  // 2.2.5: the most hair and clothes colours, "20,77" (applied after --kinds)
            const size_t comma = v.find(',');
            colours[0] = std::atoi(v.c_str());
            colours[1] = comma == std::string::npos ? colours[0] : std::atoi(v.c_str() + comma + 1);
        }
        else if (k == "--studio") a.studio = v;  // 2.3
        else if (k == "--out") a.pack.out = v;
        else if (k == "--status") a.pack.status = v;
        else continue;
        ++i;
    }
    if (colours[0] >= -1) a.pack.choice.maxHairColour = colours[0];
    if (colours[1] >= -1) a.pack.choice.maxClothColour = colours[1];
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
    void present(const uint8_t* data, uint32_t size);
    // AI icons (0.14): one made larger goes back to the driver.
    static void iconDone(void* self, uint64_t key, int width, int height, const uint32_t* argb);
    // 0.40: what the start does, to the driver: it shows it over the game's window until the Hello.
    static void startProgress(void* self, const start::Progress& p);
    void checkLateDlss();  // DLSS started in the background: on, or the reason it is not

    win::bridge::Channel channel_;
    gpu::Renderer renderer_;
    win::bridge::Server server_{renderer_, channel_};  // 2.5.12: the messages shared with Direct3D 11's thread
    host::Dlss dlss_;  // after the renderer: Streamline shuts down before the device goes
    std::string presentMode_;
    bool surface_ = false;      // the driver gave a composition surface
    bool surfaceAsked_ = false;
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
    dlss_.setOwn(a.dlssScale, a.dlssWidth, a.dlssHeight, a.dlssModel);  // 2.4.2
    dlss_.setReflex(a.reflex);                                          // 2.5.26
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
        server_.sendLost();
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
    if (type == Msg::Present) return present(data, size);
    if (type == Msg::SetSurface) {
        WireReader rd(data, size);
        const uint64_t handle = rd.u64();
        if (!rd.ok()) return;
        renderer_.setPresentSurface(HANDLE(uintptr_t(handle)));
        surface_ = true;
        LOG_INFO("bridge: drawing into the game window's composition surface");
        return;
    }
    server_.dispatch(type, data, size);  // 2.5.12: the rest, as Direct3D 11's own thread has them too
}

void Host::present(const uint8_t* data, uint32_t size) {
    win::bridge::Server::Present p;
    if (win::bridge::Server::readPresent(data, size, p) && (surface_ || presentMode_ != "composition")) {
        renderer_.present(p.window, p.width, p.height, p.source, p.options);
        server_.sendTiming();
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
    if (!a.studio.empty()) {  // 2.3: uaRO Studio's engine, a log of its own
        wlog::open(a.dir + "\\uaro-studio-engine.log", LogLevel(a.log < 0 ? 0 : a.log > 2 ? 2 : a.log));
        return host::runStudio(a.dir, a.studio);
    }
    wlog::open(a.dir + "\\uaro-render64.log", LogLevel(a.log < 0 ? 0 : a.log > 2 ? 2 : a.log));
    if (a.channel.empty() || !a.parent) {
        LOG_ERROR("uaro-render64.exe is started by the uaRO Graphics driver in the game, not by hand");
        return 1;
    }
    Host host;
    return host.run(a);
}
