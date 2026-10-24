#include "win/code_page.hpp"

#include <windows.h>

namespace uagfx::win {

unsigned gameCodePage(const Config& config) {
    return config.clientRoext && config.roext.codePage > 0 ? unsigned(config.roext.codePage) : CP_ACP;
}

std::wstring widened(const std::string& text, unsigned codePage) {
    const int n = MultiByteToWideChar(codePage, 0, text.data(), int(text.size()), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(codePage, 0, text.data(), int(text.size()), &w[0], n);
    return w;
}

std::string utf8Of(const std::wstring& text) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), &s[0], n, nullptr, nullptr);
    return s;
}

}  // namespace uagfx::win
