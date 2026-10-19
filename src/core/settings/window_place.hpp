#pragma once
#include <optional>
#include <string>

namespace uagfx::settings {

// 2.6: the settings app's window as it was closed - its rectangle when not maximized (screen pixels, a monitor left
// of the main one below 0) and whether it was maximized; [utility] window = "left top right bottom 0|1".
struct WindowPlace {
    int left, top, right, bottom;
    bool maximized;
};
std::string placeToken(const WindowPlace& p);
std::optional<WindowPlace> parsePlace(const std::string& token);  // five numbers, 200-20000 wide and tall

}  // namespace uagfx::settings
