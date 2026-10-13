#include "app/text.hpp"

#include <windows.h>

#include <cstdio>

namespace uagfx::app {

std::wstring wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring out(size_t(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), &out[0], n);
    return out;
}

std::string utf8(const std::wstring& w) {
    if (w.empty()) return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), &out[0], n, nullptr, nullptr);
    return out;
}

std::wstring wtr(settings::Lang l, settings::Text t) { return wide(settings::tr(l, t)); }

std::wstring wtrf(settings::Lang l, settings::Text t, const std::vector<std::string>& args) {
    return wide(settings::fmt(settings::tr(l, t), args));
}

std::string num(long long v) { return std::to_string(v); }

std::string scaleText(double s) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", s);
    std::string out = buf;
    while (out.size() > 1 && out.back() == '0') out.pop_back();
    if (!out.empty() && out.back() == '.') out.pop_back();
    return out;
}

}  // namespace uagfx::app
