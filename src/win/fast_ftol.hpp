#pragma once

namespace uagfx::win::fastftolwin {

// 3.0.19-beta.48 (core/fast_ftol): uaRO.exe's __ftol sent to one SSE3 fisttp - when the processor has SSE3 and the
// function's bytes are this build's (else the game's own, as before); once a process
bool install();

}  // namespace uagfx::win::fastftolwin
