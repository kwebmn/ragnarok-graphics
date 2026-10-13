#include "app/page.hpp"

namespace uagfx::app {

std::string controlText(HWND ctl) {
    wchar_t buf[256];
    const int n = GetWindowTextW(ctl, buf, 256);
    return utf8(std::wstring(buf, size_t(n > 0 ? n : 0)));
}

}  // namespace uagfx::app
