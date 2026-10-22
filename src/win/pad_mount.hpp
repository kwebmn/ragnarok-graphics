#pragma once

namespace uagfx::win {

// 2.9: uaRO Pad inside the driver (src/pad) mounted from DllMain when [pad] enabled (on by default); the line for the
// log once it is open.
void mountPad();
void unmountPad();
const char* padMountedText();

}  // namespace uagfx::win
