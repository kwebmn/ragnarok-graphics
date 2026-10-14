// uaro-render64.exe (part 3): draws for the 32-bit driver in uaRO.exe with Direct3D 12 in a 64-bit process, so that
// what exists only for 64-bit programs - DLSS, frame generation - can come next. The driver starts it with
//   uaro-render64.exe --channel <shared block> --parent <game pid> --dir "<game folder>" --log <level>
//                     --present auto|window|composition
// and it ends with the game.
#include <windows.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "core/bridge_protocol.hpp"
#include "core/bridge_wire.hpp"
#include "core/version.hpp"
#include "host/streamline.hpp"
#include "win/bridge/channel.hpp"
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
        else continue;
        ++i;
    }
    return a;
}

win::bridge::Channel* g_channel = nullptr;

// Every log line also goes to the driver, which writes it into uaro-graphics.log with "render64:" in front.
void forward(LogLevel level, const char* text) {
    if (!g_channel) return;
    std::vector<uint8_t> out;
    WireWriter w(out);
    w.u8(uint8_t(level));
    w.str(text);
    g_channel->send(Msg::Log, out);
}

class Host {
public:
    int run(const Args& a);

private:
    void dispatch(Msg type, const uint8_t* data, uint32_t size);
    void present(WireReader& rd);

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
};

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
    // Part 3: Streamline starts before the first Direct3D call of the process.
    bool dlss = false;
    if (a.dlss != "off") {
        std::string why;
        dlss = dlss_.init(a.dir + "\\uaro-graphics\\dlss", a.dir, a.dlss, why);
        dlss_.setSigns(a.jitterInverted ? -1.0f : 1.0f, a.motionInverted ? -1.0f : 1.0f);
        if (!dlss) LOG_ERROR("DLSS: %s; drawing without it", why.c_str());
    }
    out_.clear();
    WireWriter hello(out_);
    if (!renderer_.create(error)) {
        LOG_ERROR("Direct3D 12: %s", error.c_str());
        hello.u8(0);
        hello.str(error);
        channel_.send(Msg::Hello, out_);
        return 3;
    }
    LOG_INFO("uaro-render64 %s (64-bit) draws on %s", kVersion, renderer_.adapterName().c_str());
    if (dlss) {
        std::string why;
        if (dlss_.attach(renderer_.device(), why)) {
            renderer_.setUpscaler(&dlss_);
            LOG_INFO("%s ready (NVIDIA DLSS through Streamline)", dlss_.name());
        } else {
            LOG_ERROR("DLSS: %s; drawing without it", why.c_str());
        }
    }
    hello.u8(1);
    hello.str(renderer_.adapterName());
    channel_.send(Msg::Hello, out_);
    for (;;) {
        Msg type;
        const uint8_t* data = nullptr;
        uint32_t size = 0;
        if (!channel_.receive(type, data, size, INFINITE)) break;  // the game has gone
        if (type == Msg::Quit) break;
        dispatch(type, data, size);
        if (renderer_.lost() && !lostSent_) {
            lostSent_ = true;
            out_.clear();
            WireWriter(out_).str("the GPU device was removed");
            channel_.send(Msg::Lost, out_);
        }
    }
    renderer_.finish();  // nothing of DLSS's still on the GPU when Streamline shuts down
    wlog::setSink(nullptr);
    g_channel = nullptr;
    if (parent) CloseHandle(parent);
    return 0;
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
            renderer_.setDrawQuality(aniso, coverage);
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
            if (rd.ok()) renderer_.setCamera(valid, view);
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
    const Args a = parseArgs();
    wlog::open(a.dir + "\\uaro-render64.log", LogLevel(a.log < 0 ? 0 : a.log > 2 ? 2 : a.log));
    if (a.channel.empty() || !a.parent) {
        LOG_ERROR("uaro-render64.exe is started by the uaRO Graphics driver in the game, not by hand");
        return 1;
    }
    Host host;
    return host.run(a);
}
