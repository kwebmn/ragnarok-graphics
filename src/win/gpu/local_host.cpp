#include "win/gpu/local_host.hpp"

#include <memory>
#include <vector>

#include "core/bridge_protocol.hpp"
#include "core/bridge_wire.hpp"
#include "core/start_screen.hpp"
#include "win/bridge/channel.hpp"
#include "win/bridge/serve.hpp"
#include "win/log.hpp"
#include "win/thread_time.hpp"

namespace uagfx::gpu {
namespace {

using uagfx::bridge::Msg;
using uagfx::bridge::WireWriter;

struct Start {
    std::string name;
    BridgeOptions options;
};

// What the start does (the shaders, the AI textures' index), to the game's end, which shows it over its window.
void startProgress(void* context, const start::Progress& p) {
    std::vector<uint8_t> out;
    WireWriter w(out);
    w.u8(uint8_t(p.stage));
    w.i32(p.done);
    w.i32(p.total);
    static_cast<win::bridge::Channel*>(context)->send(Msg::StartProgress, out);
}

// An AI icon made larger (from the AI textures' own thread too: Channel::send takes one message at a time).
void iconDone(void* context, uint64_t key, int width, int height, const uint32_t* argb) {
    std::vector<uint8_t> out;
    WireWriter w(out);
    w.u64(key);
    w.i32(width);
    w.i32(height);
    if (width > 0 && height > 0 && argb) w.bytes(argb, size_t(width) * size_t(height) * 4);  // 0x0: there is none
    static_cast<win::bridge::Channel*>(context)->send(Msg::IconDone, out);
}

DWORD WINAPI run(void* context) {
    const std::unique_ptr<Start> s(static_cast<Start*>(context));
    win::bridge::Channel channel;
    std::string error;
    if (!channel.open(s->name, error)) {
        LOG_ERROR("Direct3D 11's thread: %s", error.c_str());
        return 1;
    }
    std::vector<uint8_t> hello;
    WireWriter w(hello);
    {
        Renderer renderer;  // goes before the channel
        renderer.setStartSink(&startProgress, &channel);
        if (!renderer.createD3D11(s->options.shaderCache, error)) {
            w.u8(0);
            w.str(error);
            channel.send(Msg::Hello, hello);
            return 2;
        }
        renderer.setStateCache(s->options.stateCache);
        renderer.setMergeDraws(s->options.mergeDraws);
        renderer.setIconSink(&iconDone, &channel);
        renderer.setGameDir(s->options.gameDir);
        if (s->options.ai.enabled) renderer.startAiTextures(s->options.gameDir, s->options.ai);  // its own log line
        w.u8(1);
        w.str(renderer.adapterName());
        w.u8(0);  // no DLSS
        channel.send(Msg::Hello, hello);
        win::bridge::Server server(renderer, channel);
        // 2.5.15: how much of a core this thread has - its work, not its waits for the card (HD 6570: which limits?)
        double cpuSince = win::threadCpuMs();
        ULONGLONG wallSince = GetTickCount64();
        unsigned frames = 0;
        for (;;) {
            Msg type;
            const uint8_t* data = nullptr;
            uint32_t size = 0;
            if (!channel.receive(type, data, size, INFINITE) || type == Msg::Quit) break;
            if (type == Msg::Present) {
                win::bridge::Server::Present p;
                if (win::bridge::Server::readPresent(data, size, p)) {
                    renderer.present(p.window, p.width, p.height, p.source, p.options);
                    server.sendTiming();
                }
                channel.notifyPresented();
                ++frames;
                const ULONGLONG now = GetTickCount64();
                if (now - wallSince >= 30000) {
                    const double cpu = win::threadCpuMs();
                    LOG_INFO("Direct3D 11's thread: busy %.0f %% of a core (%.1f ms of the processor a frame, %u "
                             "frames)", (cpu - cpuSince) * 100.0 / double(now - wallSince), (cpu - cpuSince) / frames,
                             frames);
                    cpuSince = cpu;
                    wallSince = now;
                    frames = 0;
                }
            } else {
                server.dispatch(type, data, size);
            }
            server.sendLost();
        }
        renderer.finish();
    }
    return 0;
}

}  // namespace

HANDLE startLocalHost(const std::string& name, const BridgeOptions& options) {
    Start* s = new Start{name, options};
    const HANDLE thread = CreateThread(nullptr, 0, &run, s, 0, nullptr);
    if (!thread) delete s;
    return thread;
}

}  // namespace uagfx::gpu
