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
// only setTarget, setScale and readTarget wait for an answer. Texture ids are given out here.
class Remote {
public:
    Remote() = default;
    ~Remote();  // asks the renderer process to finish, and ends it if it does not
    Remote(const Remote&) = delete;
    Remote& operator=(const Remote&) = delete;

    bool start(const BridgeOptions& o, std::string& error);
    std::string adapterName() const { return adapter_; }
    bool upscales() const { return upscales_; }  // DLSS is ready in the renderer
    void setUiSharp(bool sharp);
    void preparePipelines(const std::string& file);
    bool setTarget(int width, int height);
    bool setScale(double k, int width, int height, int samples, bool upscale);
    void setCamera(bool valid, const float view[12]);
    int samples() const { return samples_; }
    void setDrawQuality(int anisotropy, bool alphaToCoverage, bool aiShown);
    uint32_t createTexture(int width, int height, int levels);
    void releaseTexture(uint32_t id);
    void execute(const FrameRecord& record);
    bool readTarget(std::vector<uint32_t>& argb);
    void present(HWND window, int outputWidth, int outputHeight, const Rect& source, const PresentOptions& options);

private:
    bool send(bridge::Msg type, const std::vector<uint8_t>& payload);
    // Waits for the answer `want`, handling what else arrives meanwhile; false when the renderer is gone or slow.
    bool await(bridge::Msg want, std::vector<uint8_t>& reply, DWORD ms);
    void handle(bridge::Msg type, const uint8_t* data, uint32_t size);
    void drain();  // what has arrived, without waiting
    void setLost(const std::string& why);
    void showOn(HWND window, int outputWidth);  // the composition surface, when the renderer asked for one

    win::bridge::Channel channel_;
    win::bridge::Composition composition_;
    PROCESS_INFORMATION process_{};
    BridgeOptions options_;
    std::string adapter_;
    uint32_t nextTexture_ = 0;
    uint32_t presentsSent_ = 0;
    int samples_ = 1;
    bool upscales_ = false;
    bool lost_ = false;
    bool needSurface_ = false;  // the renderer could not make a swap chain for the game's window
    std::vector<uint8_t> buffer_;  // a message being written; kept to keep its memory
};

}  // namespace uagfx::gpu
