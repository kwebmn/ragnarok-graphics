#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"
#include "win/bridge/channel.hpp"
#include "win/bridge/composition.hpp"
#include "win/gpu/renderer.hpp"

namespace uagfx::gpu {

// Part 3: the renderer in uaro-render64.exe, driven from the game's process. Every call goes through the shared block;
// only setTarget, setScale and readTarget wait for an answer. Texture ids are given out here. 2.5.12: or Direct3D 11
// on a thread of its own in this process (BridgeOptions::thread, gpu/local_host.cpp), through a block of its own.
class Remote {
public:
    Remote() = default;
    ~Remote();  // asks the renderer process to finish, and ends it if it does not
    Remote(const Remote&) = delete;
    Remote& operator=(const Remote&) = delete;

    bool start(const BridgeOptions& o, std::string& error);
    std::string adapterName() const { return adapter_; }
    bool upscales() const { return upscales_; }  // DLSS is ready in the renderer
    // 2.2.1: the renderer's work a frame, as it last sent it (Msg::Timing, twice a second)
    bool timing(GpuTiming& out) const {
        out = timing_;
        return timing_.gpuMs >= 0.0;
    }
    void setUiSharp(bool sharp);
    void setWorldScale(double share);  // 2.5.16: sent when it changes
    void preparePipelines(const std::string& file);
    bool setTarget(int width, int height);
    bool setScale(double k, int width, int height, int samples, bool upscale);
    void setCamera(bool valid, const float view[12], float targetZ = 0);
    void setMap(const std::string& map);
    void sendIcon(uint64_t key, int width, int height, const uint32_t* argb);
    void setIconSink(Renderer::IconSink sink, void* context) {
        iconSink_ = sink;
        iconContext_ = context;
    }
    void setStartSink(Renderer::StartSink sink, void* context) {  // 0.40
        startSink_ = sink;
        startContext_ = context;
    }
    int samples() const { return samples_; }
    void setDrawQuality(int anisotropy, bool alphaToCoverage, uint8_t aiKinds, uint8_t aiSprites);
    void setStudio(uint8_t flags, int x, int y);  // 2.3
    void setEffects(const EffectSettings& e);  // sent when they change
    uint32_t createTexture(int width, int height, int levels);
    void releaseTexture(uint32_t id);
    void noteObjectTexture(uint32_t id, uint8_t kind);  // 0.23.1
    void execute(const FrameRecord& record);
    bool readTarget(std::vector<uint32_t>& argb);
    void present(HWND window, int outputWidth, int outputHeight, const Rect& source, const PresentOptions& options);

private:
    bool send(bridge::Msg type, const std::vector<uint8_t>& payload);
    bool startThread(const std::string& name, std::string& error);  // 2.5.12
    // Waits for the answer `want`, handling what else arrives meanwhile; false when the renderer is gone or slow.
    bool await(bridge::Msg want, std::vector<uint8_t>& reply, DWORD ms);
    void handle(bridge::Msg type, const uint8_t* data, uint32_t size);
    void drain();  // what has arrived, without waiting
    void setLost(const std::string& why);
    void showOn(HWND window, int outputWidth);  // the composition surface, when the renderer asked for one

    win::bridge::Channel channel_;
    win::bridge::Composition composition_;
    PROCESS_INFORMATION process_{};
    HANDLE thread_ = nullptr;  // 2.5.12: Direct3D 11's own thread in this process instead (gpu/local_host.cpp)
    std::string who_ = "the 64-bit renderer";  // in the log
    BridgeOptions options_;
    std::string adapter_;
    uint32_t nextTexture_ = 0;
    double worldScale_ = 1.0;  // 2.5.16: the last sent
    uint32_t presentsSent_ = 0;
    // 2.1.1: the game's thread's time here - its draws written and sent, its wait for the renderer's presents -
    // logged every 30 s
    LONGLONG sendTicks_ = 0, waitTicks_ = 0, since_ = 0;
    uint32_t frames_ = 0;
    void timeLog();
    int samples_ = 1;
    bool upscales_ = false;
    GpuTiming timing_;
    bool lost_ = false;
    bool needSurface_ = false;  // the renderer could not make a swap chain for the game's window
    EffectSettings effects_;  // as last sent
    bool effectsSent_ = false;
    Renderer::IconSink iconSink_ = nullptr;  // AI icons that came back (Msg::IconDone)
    void* iconContext_ = nullptr;
    Renderer::StartSink startSink_ = nullptr;  // 0.40: what the renderer's start does (Msg::StartProgress)
    void* startContext_ = nullptr;
    std::vector<uint8_t> buffer_;  // a message being written; kept to keep its memory
};

}  // namespace uagfx::gpu
