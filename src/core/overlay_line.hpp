#pragma once
#include <string>

namespace uagfx::overlayline {

// 3.0.22-beta.28 (the owner's video session 20.11: Pause's «uaRO Graphics вимкнено…» stood in the photo mode's
// recordings): what the line in the game window's top left corner says - the frame counter (Ctrl+Shift+F11) when
// `counter`, after the note (Pause's line, the graphics test's, the frame keeper's, the card's) when there is one;
// nothing at all in the photo mode (Ctrl+Alt+F12, a big shot): a clean picture for a shot or a recording.
// "" - the line's window hidden.
std::wstring line(bool counter, bool photo, const std::wstring& note, const std::wstring& counterText);

}  // namespace uagfx::overlayline
