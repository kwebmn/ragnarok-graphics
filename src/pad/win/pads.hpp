#pragma once
#include <windows.h>

#include "pad/core/config/config.hpp"
#include "pad/core/logic/pad.hpp"
#include "pad/win/sony_pad.hpp"
#include "pad/win/xinput_pad.hpp"

namespace uapad {

// 0.6: the controller the pad reads - an Xbox one (XInput) or a Sony one (HID), by [pad] type: auto takes an Xbox one
// while there is one, else a Sony one. The rumble goes to the one read last.
class Pads {
public:
    void configure(const Config& c) {
        type_ = c.padType;
        sony_.configure(c.lightbar, c.touchpad);
    }
    bool load() { return xbox_.load(); }
    const char* dllName() const { return xbox_.dllName(); }
    PadState poll() {
        PadState p;
        if (type_ != Config::PadType::Sony) {
            p = xbox_.poll();
            if (p.connected || type_ == Config::PadType::Xbox) {
                setSony(false);
                return p;
            }
        }
        p = sony_.poll();
        setSony(p.connected);
        return p;
    }
    void rumble(Rumble pattern, DWORD nowMs) {
        if (sony_.open() && sonyNow_)
            sony_.rumble(pattern, nowMs);
        else
            xbox_.rumble(pattern, nowMs);
    }
    void update(DWORD nowMs) {
        xbox_.update(nowMs);
        sony_.update(nowMs);
    }
    void stopRumble() {
        xbox_.stopRumble();
        sony_.stopRumble();
    }
    bool sony() const { return sonyNow_; }  // a Sony controller is the one read: its buttons' names in the menu
    // 3.0.11-beta.89: the one read, for the app's page «Контролер» («DualSense (Bluetooth)», «Xbox (XInput)»)
    const char* name() const { return sonyNow_ ? sony_.name() : "Xbox (XInput)"; }

private:
    void setSony(bool on);
    XInputPad xbox_;
    SonyPad sony_;
    Config::PadType type_ = Config::PadType::Auto;
    bool sonyNow_ = false;
};

}  // namespace uapad
