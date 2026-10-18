#pragma once
#include <windows.h>

#include <string>

#include "win/gpu/renderer.hpp"

namespace uagfx::gpu {

// 2.5.12: Direct3D 11 on a thread of its own in the game's process. The game's thread writes each frame down and goes
// on, as it does for uaro-render64.exe; this thread draws it - the same messages through a block of this process
// (win/bridge/serve.hpp), the same Remote at the game's end. The channel `name` is made already (Channel::create);
// the thread answers its Hello. Returns the thread's handle (it ends after Msg::Quit), or nullptr.
HANDLE startLocalHost(const std::string& name, const BridgeOptions& options);

}  // namespace uagfx::gpu
