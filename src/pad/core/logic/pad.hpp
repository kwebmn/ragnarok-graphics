#pragma once
#include <cstdint>

namespace uapad {

enum PadButton : uint16_t {
    kPadA = 1 << 0,
    kPadB = 1 << 1,
    kPadX = 1 << 2,
    kPadY = 1 << 3,
    kPadLB = 1 << 4,
    kPadRB = 1 << 5,
    kPadBack = 1 << 6,
    kPadStart = 1 << 7,
    kPadUp = 1 << 8,
    kPadDown = 1 << 9,
    kPadLeft = 1 << 10,
    kPadRight = 1 << 11,
    kPadL3 = 1 << 12,
    kPadR3 = 1 << 13,
    kPadLT = 1 << 14,  // the triggers as buttons: the controller sets these from lt / rt
    kPadRT = 1 << 15,
};

struct PadState {
    bool connected = false;
    uint16_t buttons = 0;  // PadButton bits
    float lx = 0, ly = 0;  // -1..1, +y = stick pushed up
    float rx = 0, ry = 0;
    float lt = 0, rt = 0;  // 0..1
};

enum class Rumble { None, TargetSwitch, TargetCleared, PadOn, PadOff, Denied, CameraOn, CameraOff };

}  // namespace uapad
