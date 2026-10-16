#include "win/gpu/remote.hpp"

#include <algorithm>
#include <cstring>

#include "core/bridge_wire.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

using bridge::Msg;
using bridge::WireReader;
using bridge::WireWriter;

namespace {

// Each ring's header comes off its area, and a ring's room is a power of two (0.8 review M3).
constexpr uint32_t kRingHeader = uint32_t(sizeof(bridge::RingHeader));
constexpr uint32_t kToHostBytes = (32u << 20) + kRingHeader;   // frame records; larger ones go in chunks
constexpr uint32_t kToClientBytes = (8u << 20) + kRingHeader;  // log lines and the frame read back, in chunks
constexpr DWORD kAnswerMs = 10000;            // a renderer that does not answer for this long is taken as hung

}  // namespace

Remote::~Remote() {
    if (process_.hProcess) {
        if (!lost_ && channel_.send(Msg::Quit, nullptr, 0, 2000) &&
            WaitForSingleObject(process_.hProcess, 3000) == WAIT_OBJECT_0) {
            // finished by itself
        } else if (WaitForSingleObject(process_.hProcess, 0) != WAIT_OBJECT_0) {
            TerminateProcess(process_.hProcess, 1);
        }
        CloseHandle(process_.hProcess);
        CloseHandle(process_.hThread);
    }
    composition_.release();
    channel_.close();
}

bool Remote::start(const BridgeOptions& o, std::string& error) {
    options_ = o;
    nextTexture_ = bridge::kFirstClientTexture;
    const std::string name =
        "Local\\uaro-render-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64());
    if (!channel_.create(name, kToHostBytes, kToClientBytes, error)) return false;
    channel_.setWhileWaiting([](void* self) { static_cast<Remote*>(self)->drain(); }, this);
    std::string cmd = "\"" + o.hostExe + "\" --channel " + name + " --parent " +
                      std::to_string(GetCurrentProcessId()) + " --dir \"" + o.gameDir + "\" --log " +
                      std::to_string(o.logLevel) + " --present " + o.present + " --dlss " + o.dlss +
                      " --dlss-jitter " + (o.dlssJitterInverted ? "inverted" : "normal") + " --dlss-motion " +
                      (o.dlssMotionInverted ? "inverted" : "normal") + " --fg " + std::to_string(o.frameGeneration) +
                      " --ai " + aiArgument(o.ai) + " --gpu \"" + (o.gpu.empty() ? "auto" : o.gpu) + "\"";
    STARTUPINFOA si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_FORCEOFFFEEDBACK;  // it never shows a window: no "app starting" cursor over the game
    std::vector<char> line(cmd.begin(), cmd.end());
    line.push_back('\0');
    if (!CreateProcessA(o.hostExe.c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr, o.gameDir.c_str(), &si,
                        &process_)) {
        error = "cannot start " + o.hostExe + " (error " + std::to_string(GetLastError()) + ")";
        process_ = PROCESS_INFORMATION{};
        return false;
    }
    channel_.setPeer(process_.hProcess);
    std::vector<uint8_t> hello;
    if (!await(Msg::Hello, hello, 20000)) {
        error = lost_ ? "the 64-bit renderer exited at its start" : "the 64-bit renderer did not answer";
        return false;
    }
    WireReader rd(hello.data(), hello.size());
    const bool ok = rd.u8() != 0;
    const std::string text = rd.str();
    if (!ok || !rd.ok()) {
        error = "the 64-bit renderer: " + text;
        return false;
    }
    adapter_ = text;
    upscales_ = rd.u8() != 0 && rd.ok();
    return true;
}

bool Remote::send(Msg type, const std::vector<uint8_t>& payload) {
    if (lost_) return false;
    if (!channel_.send(type, payload)) {
        setLost("the 64-bit renderer is gone");
        return false;
    }
    return true;
}

void Remote::setLost(const std::string& why) {
    if (lost_) return;
    lost_ = true;
    DWORD code = 0;
    if (process_.hProcess && WaitForSingleObject(process_.hProcess, 0) == WAIT_OBJECT_0)
        GetExitCodeProcess(process_.hProcess, &code);
    LOG_ERROR("%s (exit code 0x%08lx); the picture stops here, restart the game", why.c_str(),
              static_cast<unsigned long>(code));
}

void Remote::handle(Msg type, const uint8_t* data, uint32_t size) {
    WireReader rd(data, size);
    switch (type) {
        case Msg::Log: {
            const uint8_t level = rd.u8();
            const std::string text = rd.str();
            if (rd.ok()) wlog::line(LogLevel(std::min<uint8_t>(level, 2)), "render64: %s", text.c_str());
            break;
        }
        case Msg::NeedSurface:
            needSurface_ = true;
            break;
        case Msg::UpscalerReady:  // the next present switches to it (Context::present asks every frame)
            upscales_ = true;
            break;
        case Msg::Lost: setLost("render64: " + rd.str()); break;
        case Msg::IconDone: {  // AI icons (0.14): one made larger
            const uint64_t key = rd.u64();
            const int w = rd.i32(), h = rd.i32();
            if (rd.ok() && w == 0 && h == 0 && iconSink_) {  // 0.47.1: there is none - crisp text makes it by xBR
                iconSink_(iconContext_, key, 0, 0, nullptr);
                break;
            }
            if (!rd.ok() || w <= 0 || h <= 0 || w > 1024 || h > 1024 || !iconSink_) break;
            const uint8_t* bytes = rd.take(size_t(w) * size_t(h) * 4);
            if (!bytes) break;
            std::vector<uint32_t> argb(size_t(w) * size_t(h));
            std::memcpy(argb.data(), bytes, argb.size() * 4);
            iconSink_(iconContext_, key, w, h, argb.data());
            break;
        }
        case Msg::StartProgress: {  // 0.40: shown over the game's window while it waits for the Hello
            start::Progress p;
            const uint8_t stage = rd.u8();
            p.done = rd.i32();
            p.total = rd.i32();
            if (!rd.ok() || stage > uint8_t(start::Stage::AiTextures) || !startSink_) break;
            p.stage = start::Stage(stage);
            startSink_(startContext_, p);
            break;
        }
        default: break;  // an answer nobody waits for any more
    }
}

bool Remote::await(Msg want, std::vector<uint8_t>& reply, DWORD ms) {
    const ULONGLONG start = GetTickCount64();
    for (;;) {
        Msg type;
        const uint8_t* data = nullptr;
        uint32_t size = 0;
        const ULONGLONG spent = GetTickCount64() - start;
        if (spent >= ms || !channel_.receive(type, data, size, DWORD(ms - spent))) {
            if (channel_.peerGone()) setLost("the 64-bit renderer exited");
            return false;
        }
        if (type == want) {
            reply.assign(data, data + size);
            return true;
        }
        handle(type, data, size);
    }
}

void Remote::drain() {
    Msg type;
    const uint8_t* data = nullptr;
    uint32_t size = 0;
    while (!lost_ && channel_.receive(type, data, size, 0)) handle(type, data, size);
    if (!lost_ && channel_.peerGone()) setLost("the 64-bit renderer exited");
}

void Remote::setUiSharp(bool sharp) { send(Msg::SetUiSharp, {uint8_t(sharp ? 1 : 0)}); }

void Remote::preparePipelines(const std::string& file) {
    buffer_.clear();
    WireWriter(buffer_).str(file);
    send(Msg::PreparePipelines, buffer_);
}

bool Remote::setTarget(int width, int height) {
    buffer_.clear();
    WireWriter w(buffer_);
    w.i32(width);
    w.i32(height);
    std::vector<uint8_t> reply;
    if (!send(Msg::SetTarget, buffer_) || !await(Msg::TargetSet, reply, kAnswerMs)) return false;
    samples_ = 1;
    return !reply.empty() && reply[0] != 0;
}

bool Remote::setScale(double k, int width, int height, int samples, bool upscale) {
    buffer_.clear();
    WireWriter w(buffer_);
    w.f64(k);
    w.i32(width);
    w.i32(height);
    w.i32(samples);
    w.u8(upscale ? 1 : 0);
    std::vector<uint8_t> reply;
    if (!send(Msg::SetScale, buffer_) || !await(Msg::ScaleSet, reply, kAnswerMs)) return false;
    WireReader rd(reply.data(), reply.size());
    const bool ok = rd.u8() != 0;
    const int made = rd.i32();
    if (rd.ok()) samples_ = made;
    return ok && rd.ok();
}

void Remote::setCamera(bool valid, const float view[12]) {
    buffer_.clear();
    WireWriter w(buffer_);
    w.u8(valid ? 1 : 0);
    for (int i = 0; i < 12; ++i) w.f32(valid ? view[i] : 0.0f);
    send(Msg::Camera, buffer_);
}

void Remote::sendIcon(uint64_t key, int width, int height, const uint32_t* argb) {
    buffer_.clear();
    WireWriter w(buffer_);
    w.u64(key);
    w.i32(width);
    w.i32(height);
    w.bytes(argb, size_t(width) * size_t(height) * 4);
    send(Msg::IconWanted, buffer_);
}

void Remote::setMap(const std::string& map) {
    buffer_.clear();
    WireWriter w(buffer_);
    w.str(map);
    send(Msg::Map, buffer_);
}

void Remote::setDrawQuality(int anisotropy, bool alphaToCoverage, bool aiShown) {
    buffer_.clear();
    WireWriter w(buffer_);
    w.i32(anisotropy);
    w.u8(alphaToCoverage ? 1 : 0);
    w.u8(aiShown ? 1 : 0);
    send(Msg::SetDrawQuality, buffer_);
}

void Remote::setEffects(const EffectSettings& e) {
    if (effectsSent_ && e == effects_) return;
    buffer_.clear();
    WireWriter w(buffer_);
    w.u8(e.glow ? 1 : 0);
    w.i32(e.glowStrength);
    w.i32(e.glowSize);
    w.i32(e.glowLight);
    w.u8(e.ao ? 1 : 0);
    w.i32(e.aoStrength);
    w.i32(e.aoSize);
    w.u8(e.water ? 1 : 0);
    w.i32(e.waterWaves);
    w.i32(e.waterReflections);
    w.i32(e.waterClarity);
    w.u8(e.grade ? 1 : 0);
    w.i32(e.gradeSaturation);
    w.i32(e.gradeContrast);
    w.i32(e.gradeBrightness);
    w.i32(e.gradeWarmth);
    w.i32(e.gradeVignette);
    w.u8(e.dof ? 1 : 0);
    w.i32(e.dofStrength);
    w.i32(e.dofRange);
    w.u8(e.smoothLight ? 1 : 0);
    w.u8(e.torch ? 1 : 0);
    w.i32(e.torchGlow);
    w.i32(e.torchLight);
    w.i32(e.torchFlicker);
    w.u8(e.torchFlame ? 1 : 0);
    w.i32(e.torchBrightness);
    w.u8(e.smoke ? 1 : 0);
    w.i32(e.smokeDensity);
    w.u8(e.lamps ? 1 : 0);
    w.i32(e.lampsLight);
    w.u8(e.fireLight ? 1 : 0);
    w.i32(e.fireLightStrength);
    w.u8(e.fireflies ? 1 : 0);
    w.i32(e.firefliesBrightness);
    w.u8(e.mapGlows ? 1 : 0);
    w.i32(e.mapGlowsBrightness);
    w.u8(e.rays ? 1 : 0);
    w.i32(e.raysBrightness);
    w.u8(e.lava ? 1 : 0);
    w.i32(e.lavaBrightness);
    w.u8(e.actorShadows ? 1 : 0);
    w.i32(e.actorShadowsStrength);
    w.u8(e.clouds ? 1 : 0);
    w.i32(e.cloudsStrength);
    w.u8(e.haze ? 1 : 0);
    w.i32(e.hazeStrength);
    w.u8(e.heat ? 1 : 0);
    w.i32(e.heatStrength);
    w.u8(e.skillColours ? 1 : 0);
    w.u8(e.portals ? 1 : 0);
    w.i32(e.portalsStrength);
    w.u8(e.weather ? 1 : 0);
    w.i32(e.weatherAmount);
    w.u8(e.night ? 1 : 0);  // 0.63
    w.i32(e.nightStrength);
    w.u8(e.nightClock ? 1 : 0);
    w.i32(e.nightNow);
    w.u8(e.foliage ? 1 : 0);  // 0.64
    w.i32(e.foliageStrength);
    w.u8(e.softParticles ? 1 : 0);  // 0.67
    w.u8(e.rimLight ? 1 : 0);  // 0.68
    w.i32(e.rimStrength);
    w.u8(e.sky ? 1 : 0);  // 0.69
    w.i32(e.skyClouds);
    w.i32(e.skyStrength);
    w.u8(e.motes ? 1 : 0);  // 0.70
    w.i32(e.motesAmount);
    w.u8(e.relief ? 1 : 0);  // 0.71
    w.i32(e.reliefStrength);
    w.u8(e.floorReflections ? 1 : 0);  // 0.72
    w.i32(e.floorReflectStrength);
    w.u8(e.shafts ? 1 : 0);  // 0.73
    w.i32(e.shaftsStrength);
    w.u8(e.dayCycle ? 1 : 0);  // 0.74
    w.i32(e.dayNow);
    w.u8(e.sunShadows ? 1 : 0);  // 0.77
    w.i32(e.sunShadowsStrength);
    w.u8(e.bloom ? 1 : 0);  // 0.78
    w.i32(e.bloomStrength);
    w.u8(e.filmic ? 1 : 0);
    w.u8(e.regionLooks ? 1 : 0);  // 0.79
    w.i32(e.regionLooksStrength);
    w.u8(e.footprints ? 1 : 0);  // 0.80
    w.u8(e.storms ? 1 : 0);
    w.u8(e.grass ? 1 : 0);  // 0.81
    w.i32(e.grassAmount);
    w.u8(e.skillSparks ? 1 : 0);  // 0.82
    w.i32(e.skillSparksAmount);
    w.u8(e.filmGrain ? 1 : 0);  // 0.83
    w.i32(e.filmGrainAmount);
    w.u8(e.mapFade ? 1 : 0);
    w.u8(e.roundLight ? 1 : 0);  // 0.84
    w.u8(e.appear ? 1 : 0);  // 0.93
    w.u8(e.ripples ? 1 : 0);  // 0.94
    w.u8(e.glint ? 1 : 0);  // 0.95
    w.u8(e.hitFlash ? 1 : 0);  // 0.96
    if (send(Msg::SetEffects, buffer_)) {
        effects_ = e;
        effectsSent_ = true;
    }
}

uint32_t Remote::createTexture(int width, int height, int levels) {
    if (lost_) return 0;
    const uint32_t id = nextTexture_++;
    buffer_.clear();
    WireWriter w(buffer_);
    w.u32(id);
    w.i32(width);
    w.i32(height);
    w.i32(levels);
    return send(Msg::CreateTexture, buffer_) ? id : 0;
}

void Remote::releaseTexture(uint32_t id) {
    buffer_.clear();
    WireWriter(buffer_).u32(id);
    send(Msg::ReleaseTexture, buffer_);
}

void Remote::noteObjectTexture(uint32_t id, uint8_t kind) {
    buffer_.clear();
    WireWriter w(buffer_);
    w.u32(id);
    w.u8(kind);
    send(Msg::ObjectTexture, buffer_);
}

void Remote::execute(const FrameRecord& record) {
    if (lost_ || record.empty()) return;
    buffer_.clear();
    WireWriter w(buffer_);
    bridge::writeRecord(w, record);
    send(Msg::Execute, buffer_);
}

bool Remote::readTarget(std::vector<uint32_t>& argb) {
    std::vector<uint8_t> reply;
    if (!send(Msg::ReadTarget, {}) || !await(Msg::TargetPixels, reply, kAnswerMs)) return false;
    WireReader rd(reply.data(), reply.size());
    const bool ok = rd.u8() != 0;
    const int w = rd.i32();
    const int h = rd.i32();
    if (!ok || !rd.ok() || w <= 0 || h <= 0) return false;
    const size_t count = size_t(w) * size_t(h);
    const uint8_t* pixels = rd.take(count * 4);
    if (!pixels) return false;
    argb.resize(count);
    std::memcpy(argb.data(), pixels, count * 4);
    return true;
}

void Remote::showOn(HWND window, int outputWidth) {
    float scale = 1.0f;
    RECT logical{};
    if (options_.dpiScale && GetClientRect(window, &logical) && logical.right > 0 && outputWidth > 0)
        scale = float(logical.right) / float(outputWidth);  // a DPI-unaware window: its points per real pixel
    if (composition_.window() == window) {
        composition_.setScale(scale);
        return;
    }
    uint64_t handle = 0;
    std::string error;
    if (!composition_.attach(window, process_.hProcess, scale, handle, error)) {
        setLost("the game's window cannot show the 64-bit renderer's picture: " + error);
        return;
    }
    buffer_.clear();
    WireWriter(buffer_).u64(handle);
    if (send(Msg::SetSurface, buffer_))
        LOG_INFO("bridge: the picture comes through a composition surface (scale %.4f)", double(scale));
}

void Remote::present(HWND window, int outputWidth, int outputHeight, const Rect& source, const PresentOptions& o) {
    if (lost_) {
        Sleep(15);
        return;
    }
    drain();
    if (lost_) return;
    if (window && (needSurface_ || options_.present == "composition")) showOn(window, outputWidth);
    // At most two pictures wait to be shown: the game's loop keeps the pace of the renderer's presents (vsync).
    if (presentsSent_ - channel_.presented() >= 2 && !channel_.waitPresented(presentsSent_ - 1, 1000) &&
        channel_.peerGone()) {
        setLost("the 64-bit renderer exited");
        return;
    }
    buffer_.clear();
    WireWriter w(buffer_);
    w.u64(uint64_t(uintptr_t(window)));
    w.i32(outputWidth);
    w.i32(outputHeight);
    w.rect(source);
    w.u8(o.vsync ? 1 : 0);
    w.u8(o.allowTearing ? 1 : 0);
    w.i32(o.fpsLimit);
    w.f32(o.sharpen);
    w.u8(o.foreground ? 1 : 0);
    w.str(o.screenshot);  // 0.54
    w.u8(o.hdr ? 1 : 0);  // 0.65
    if (send(Msg::Present, buffer_)) ++presentsSent_;
}

}  // namespace uagfx::gpu
