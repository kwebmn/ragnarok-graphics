#pragma once
#include <windows.h>

#include <cstdint>
#include <vector>

#include "core/bridge_protocol.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "win/bridge/channel.hpp"
#include "win/gpu/renderer.hpp"

namespace uagfx::win::bridge {

// 2.5.12: the renderer's end of the bridge - each of the driver's messages turned into the renderer's call, its answer
// sent back. uaro-render64.exe's (Direct3D 12, DLSS), and in the game's own process Direct3D 11's thread of its own
// (gpu/local_host.cpp). Present, SetSurface and Quit stay the caller's.
class Server {
public:
    Server(gpu::Renderer& renderer, Channel& channel) : renderer_(renderer), channel_(channel) {}
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool dispatch(Msg type, const uint8_t* data, uint32_t size);  // false: not one of these
    struct Present {  // a Present message's fields
        HWND window = nullptr;
        int width = 0, height = 0;
        Rect source;
        gpu::PresentOptions options;
    };
    static bool readPresent(const uint8_t* data, uint32_t size, Present& out);
    void sendTiming();  // after a present: the card's work a frame, each new half second's (Msg::Timing, 2.2.1)
    void sendLost();    // once the renderer has lost its device (Msg::Lost)

private:
    gpu::Renderer& renderer_;
    Channel& channel_;
    int gameWidth_ = 0;
    int gameHeight_ = 0;
    FrameRecord record_;
    std::vector<uint32_t> pixels_;
    std::vector<uint8_t> out_;
    std::vector<uint8_t> timingOut_;
    uint32_t timingSent_ = 0;
    bool lostSent_ = false;
};

}  // namespace uagfx::win::bridge
