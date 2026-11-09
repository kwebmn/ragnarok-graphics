#include "win/code_page.hpp"

#include <windows.h>

namespace uagfx::win {

// 3.0.17-beta.25 (R-VDZY6F: «в «Совете дня» русский текст отображается кракозябрами» on an English Windows, its code
// page 1252): uaRO's words are Windows-1251 - the tips' file, the players' chat, the names. [roext] code_page set:
// that one; else Windows' own when it is 1251, 1251 otherwise
unsigned gameCodePage(const Config& config) {
    if (config.clientRoext && config.roext.codePage > 0) return unsigned(config.roext.codePage);
    return GetACP() == 1251 ? CP_ACP : 1251u;
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

std::string narrowed(const std::wstring& text, unsigned codePage) {
    const int n = WideCharToMultiByte(codePage, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(codePage, 0, text.data(), int(text.size()), &s[0], n, nullptr, nullptr);
    return s;
}

}  // namespace uagfx::win
