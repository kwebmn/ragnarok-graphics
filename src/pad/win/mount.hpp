#pragma once

namespace uapad {

// 2.9: how the game's start went for the pad - the driver writes it into its log once that is open.
enum class Mounted { Yes, Off, OldPad, NoSlot };

// uaRO Pad inside uaRO Graphics: the game's import slot DINPUT.dll!DirectInputCreateA - its one DirectInput import -
// is taken by the pad's proxy, which calls what the slot held (the system's DirectInput, or ROExt's when that is the
// game's dinput.dll) and wraps what it makes, as the pad's own dinput.dll did. Called from the driver's DllMain, the
// game's imports bound and its code not run yet. Off: [pad] enabled = 0. OldPad: a 2.x uaRO Pad dinput.dll is the
// game's - that one runs (the settings app removes it). NoSlot: no such import (another client).
Mounted mount(bool enabled);
void unmount();                      // the process ends: the cursor let out of a window that is gone
const char* mountedText(Mounted m);  // for the driver's log

}  // namespace uapad
