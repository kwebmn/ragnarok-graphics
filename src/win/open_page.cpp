#include "win/open_page.hpp"

#include <windows.h>

namespace uagfx::win {

bool openPage(const std::wstring& url) {
    using ShellExecuteFn = HINSTANCE(WINAPI*)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
    static const HMODULE shell = LoadLibraryA("shell32.dll");
    const auto open = reinterpret_cast<ShellExecuteFn>(
        reinterpret_cast<void*>(shell ? GetProcAddress(shell, "ShellExecuteW") : nullptr));
    if (open) open(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return open != nullptr;
}

}  // namespace uagfx::win
