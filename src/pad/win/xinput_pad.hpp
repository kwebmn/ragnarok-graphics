#pragma once
#include <windows.h>

#include <xinput.h>

#include <vector>

#include "pad/core/logic/pad.hpp"

namespace uapad {

class XInputPad {
public:
    bool load();
    const char* dllName() const { return name_; }
    PadState poll();  // first connected controller
    void rumble(Rumble pattern, DWORD nowMs);
    void update(DWORD nowMs);  // advances the current rumble pattern
    void stopRumble();

private:
    struct Segment {
        DWORD ms;
        WORD left;
        WORD right;
    };
    using GetState_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
    using SetState_t = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);

    void setMotors(WORD left, WORD right);

    GetState_t getState_ = nullptr;
    SetState_t setState_ = nullptr;
    const char* name_ = "";
    int index_ = -1;
    bool scanned_ = false;
    DWORD lastScanMs_ = 0;
    std::vector<Segment> pattern_;
    size_t seg_ = 0;
    DWORD segStart_ = 0;
};

}  // namespace uapad
