#pragma once
#include <windows.h>

#include <cstdint>
#include <vector>

#include "pad/core/logic/pad.hpp"
#include "pad/core/logic/sony.hpp"

namespace uapad {

// 0.6: a Sony controller (DualShock 4, DualSense) read as a HID device - over USB or Bluetooth - with its rumble and
// light bar (core/logic/sony.hpp). The same calls as XInputPad.
class SonyPad {
public:
    ~SonyPad();
    void configure(uint32_t lightbar, uint16_t touchpad) {
        lightbar_ = lightbar;
        touchpad_ = touchpad;
    }
    PadState poll();  // looks for one at most once a second while none is open
    void rumble(Rumble pattern, DWORD nowMs);
    void update(DWORD nowMs);
    void stopRumble();
    bool open() const { return handle_ != INVALID_HANDLE_VALUE; }
    const char* name() const;  // "DualShock 4 (USB)" and the like, for the log

private:
    struct Segment {
        DWORD ms;
        uint8_t left;
        uint8_t right;
    };
    bool find();
    void close();
    void startRead();
    void send(uint8_t left, uint8_t right);

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    HANDLE event_ = nullptr;
    OVERLAPPED overlapped_{};
    bool reading_ = false;
    std::vector<uint8_t> buffer_;
    sony::Model model_ = sony::Model::None;
    bool bluetooth_ = false;
    uint8_t sequence_ = 0;
    PadState last_;
    DWORD lastScanMs_ = 0;
    bool scanned_ = false;
    uint32_t lightbar_ = 0x0040FF;
    uint16_t touchpad_ = 0;
    std::vector<Segment> pattern_;
    size_t seg_ = 0;
    DWORD segStart_ = 0;
};

}  // namespace uapad
