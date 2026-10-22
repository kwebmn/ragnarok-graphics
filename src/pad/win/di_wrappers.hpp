#pragma once
#include <windows.h>

#include <dinput.h>

namespace uapad {

// Wraps the system IDirectInputA so the mouse device can be observed and fed virtual input.
IDirectInputA* wrapDirectInput(IDirectInputA* real);

}  // namespace uapad
