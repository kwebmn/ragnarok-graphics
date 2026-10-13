#include "win/dpi.hpp"

#include <algorithm>

namespace uagfx::win {
namespace {

using SetContextFn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);

// Windows 10 1607 and newer; without it the game's size is all there is.
SetContextFn setContext() {
    static const SetContextFn fn = reinterpret_cast<SetContextFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("user32.dll"), "SetThreadDpiAwarenessContext")));
    return fn;
}

}  // namespace

SIZE clientPixels(HWND hwnd, bool physical) {
    const SetContextFn set = physical ? setContext() : nullptr;
    const DPI_AWARENESS_CONTEXT old = set ? set(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) : nullptr;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (old) set(old);
    return SIZE{std::max<LONG>(1, rc.right - rc.left), std::max<LONG>(1, rc.bottom - rc.top)};
}

}  // namespace uagfx::win
