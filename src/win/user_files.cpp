#include "win/user_files.hpp"

#include <windows.h>

#include "win/dpapi.hpp"

namespace uagfx::win::userfiles {
namespace {

std::wstring folder() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    return std::wstring(buf) + L"\\uaroGraphics";
}

std::wstring wide(const char* s) {
    std::wstring w;
    for (; *s; ++s) w += wchar_t(static_cast<unsigned char>(*s));  // our names: ASCII
    return w;
}

bool readAll(const std::wstring& file, std::string& out) {
    const HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(h, &size) && size.QuadPart >= 0 && size.QuadPart < (16 << 20);
    if (ok) {
        out.resize(size_t(size.QuadPart));
        DWORD got = 0;
        ok = out.empty() || (ReadFile(h, out.data(), DWORD(out.size()), &got, nullptr) && got == out.size());
    }
    CloseHandle(h);
    return ok;
}

bool writeAll(const std::wstring& file, const std::string& bytes) {
    const std::wstring tmp = file + L".tmp";
    const HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    const bool ok = WriteFile(h, bytes.data(), DWORD(bytes.size()), &put, nullptr) && put == bytes.size();
    CloseHandle(h);
    if (!ok || !MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

}  // namespace

std::wstring path(const char* name) {
    const std::wstring dir = folder();
    return dir.empty() ? dir : dir + L"\\" + wide(name);
}

unsigned long long writeTime(const char* name) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    const std::wstring p = path(name);
    if (p.empty() || !GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &a)) return 0;
    return (unsigned long long)a.ftLastWriteTime.dwHighDateTime << 32 | a.ftLastWriteTime.dwLowDateTime;
}

bool readSealed(const char* name, std::string& plain) {
    std::string blob;
    const std::wstring p = path(name);
    return !p.empty() && readAll(p, blob) && dpapi::unprotect(blob, plain);
}

bool writeSealed(const char* name, const std::string& plain) {
    const std::wstring dir = folder();
    if (dir.empty()) return false;
    CreateDirectoryW(dir.c_str(), nullptr);
    std::string blob;
    return dpapi::protect(plain, blob) && writeAll(path(name), blob);
}

bool remove(const char* name) {
    const std::wstring p = path(name);
    return !p.empty() && (DeleteFileW(p.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND);
}

bool keepAside(const char* name) {
    const std::wstring p = path(name);
    return !p.empty() && MoveFileExW(p.c_str(), (p + L".bad").c_str(), MOVEFILE_REPLACE_EXISTING);
}

}  // namespace uagfx::win::userfiles
