#include "pad/win/xinput_pad.hpp"

namespace uapad {
namespace {

float axis(SHORT v) { return v < 0 ? float(v) / 32768.0f : float(v) / 32767.0f; }

struct ButtonMap {
    WORD xinput;
    uint16_t ours;
};

const ButtonMap kButtons[] = {
    {XINPUT_GAMEPAD_A, kPadA},
    {XINPUT_GAMEPAD_B, kPadB},
    {XINPUT_GAMEPAD_X, kPadX},
    {XINPUT_GAMEPAD_Y, kPadY},
    {XINPUT_GAMEPAD_LEFT_SHOULDER, kPadLB},
    {XINPUT_GAMEPAD_RIGHT_SHOULDER, kPadRB},
    {XINPUT_GAMEPAD_BACK, kPadBack},
    {XINPUT_GAMEPAD_START, kPadStart},
    {XINPUT_GAMEPAD_DPAD_UP, kPadUp},
    {XINPUT_GAMEPAD_DPAD_DOWN, kPadDown},
    {XINPUT_GAMEPAD_DPAD_LEFT, kPadLeft},
    {XINPUT_GAMEPAD_DPAD_RIGHT, kPadRight},
    {XINPUT_GAMEPAD_LEFT_THUMB, kPadL3},
    {XINPUT_GAMEPAD_RIGHT_THUMB, kPadR3},
};

}  // namespace

bool XInputPad::load() {
    static const char* const kDlls[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    for (const char* dll : kDlls) {
        HMODULE m = LoadLibraryA(dll);
        if (!m) continue;
        getState_ = reinterpret_cast<GetState_t>(reinterpret_cast<void*>(GetProcAddress(m, "XInputGetState")));
        setState_ = reinterpret_cast<SetState_t>(reinterpret_cast<void*>(GetProcAddress(m, "XInputSetState")));
        if (getState_) {
            name_ = dll;
            return true;
        }
    }
    return false;
}

PadState XInputPad::poll() {
    PadState p;
    if (!getState_) return p;
    XINPUT_STATE st{};
    if (index_ >= 0 && getState_(DWORD(index_), &st) != ERROR_SUCCESS) index_ = -1;
    if (index_ < 0) {
        // Polling empty XInput slots is slow: look for a controller at most once a second.
        const DWORD now = GetTickCount();
        if (scanned_ && now - lastScanMs_ < 1000) return p;
        scanned_ = true;
        lastScanMs_ = now;
        for (DWORD i = 0; i < 4 && index_ < 0; ++i)
            if (getState_(i, &st) == ERROR_SUCCESS) index_ = int(i);
        if (index_ < 0) return p;
    }
    const XINPUT_GAMEPAD& g = st.Gamepad;
    p.connected = true;
    for (const ButtonMap& b : kButtons)
        if (g.wButtons & b.xinput) p.buttons = uint16_t(p.buttons | b.ours);
    p.lx = axis(g.sThumbLX);
    p.ly = axis(g.sThumbLY);
    p.rx = axis(g.sThumbRX);
    p.ry = axis(g.sThumbRY);
    p.lt = float(g.bLeftTrigger) / 255.0f;
    p.rt = float(g.bRightTrigger) / 255.0f;
    return p;
}

void XInputPad::rumble(Rumble pattern, DWORD nowMs) {
    pattern_.clear();
    switch (pattern) {
        case Rumble::TargetSwitch: pattern_ = {{80, 0, 0x6000}}; break;
        case Rumble::TargetCleared: pattern_ = {{60, 0x3000, 0}}; break;
        case Rumble::PadOn: pattern_ = {{60, 0x5000, 0x5000}, {60, 0, 0}, {60, 0x5000, 0x5000}}; break;
        case Rumble::PadOff: pattern_ = {{250, 0x8000, 0}}; break;
        case Rumble::Denied: pattern_ = {{40, 0, 0x3000}}; break;
        case Rumble::CameraOn: pattern_ = {{40, 0x2000, 0x6000}, {50, 0, 0}, {40, 0x2000, 0x6000}}; break;
        case Rumble::CameraOff: pattern_ = {{90, 0x6000, 0x2000}}; break;
        case Rumble::None: break;
    }
    seg_ = 0;
    segStart_ = nowMs;
    if (!pattern_.empty()) setMotors(pattern_[0].left, pattern_[0].right);
}

void XInputPad::update(DWORD nowMs) {
    if (pattern_.empty()) return;
    while (seg_ < pattern_.size() && nowMs - segStart_ >= pattern_[seg_].ms) {
        segStart_ += pattern_[seg_].ms;
        ++seg_;
        if (seg_ < pattern_.size()) setMotors(pattern_[seg_].left, pattern_[seg_].right);
    }
    if (seg_ >= pattern_.size()) stopRumble();
}

void XInputPad::stopRumble() {
    pattern_.clear();
    setMotors(0, 0);
}

void XInputPad::setMotors(WORD left, WORD right) {
    if (!setState_ || index_ < 0) return;
    XINPUT_VIBRATION v{left, right};
    setState_(DWORD(index_), &v);
}

}  // namespace uapad
