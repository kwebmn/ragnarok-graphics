#include "win/accounts_store.hpp"

#include <windows.h>

#include <string>

#include "win/dpapi.hpp"
#include "win/log.hpp"

namespace uagfx::win::accountsstore {
namespace {

constexpr const wchar_t* kFolder = L"\\uaroGraphics";  // under %LOCALAPPDATA%: this Windows user's, any game folder
constexpr const wchar_t* kFile = L"\\accounts.dat";
constexpr ULONGLONG kLookEveryMs = 2000;

struct State {
    accounts::Shelf shelf;
    bool read = false;
    unsigned long long stamp = 0;  // the file's write time as last read or written, 0 none
    ULONGLONG lookedAt = 0;
};

State& state() {
    static State s;
    return s;
}

std::wstring folder() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    return std::wstring(buf) + kFolder;
}

std::wstring filePath() {
    const std::wstring f = folder();
    return f.empty() ? f : f + kFile;
}

unsigned long long writeTime(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (path.empty() || !GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return 0;
    return (unsigned long long)a.ftLastWriteTime.dwHighDateTime << 32 | a.ftLastWriteTime.dwLowDateTime;
}

bool readAll(const std::wstring& path, std::string& out) {
    const HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
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

bool writeAll(const std::wstring& path, const std::string& bytes) {
    const std::wstring tmp = path + L".tmp";
    const HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    const bool ok = WriteFile(h, bytes.data(), DWORD(bytes.size()), &put, nullptr) && put == bytes.size();
    CloseHandle(h);
    if (!ok || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

void load(State& s) {
    const std::wstring path = filePath();
    s.shelf = accounts::Shelf{};
    s.stamp = writeTime(path);
    if (!s.stamp) return;  // none yet: an empty shelf
    std::string blob, words;
    if (readAll(path, blob) && dpapi::unprotect(blob, words) && accounts::fromJson(words, s.shelf)) {
        SecureZeroMemory(words.data(), words.size());
        return;
    }
    SecureZeroMemory(words.data(), words.size());
    s.shelf = accounts::Shelf{};
    MoveFileExW(path.c_str(), (path + L".bad").c_str(), MOVEFILE_REPLACE_EXISTING);
    s.stamp = 0;
    LOG_INFO("login: the saved accounts could not be read - kept as accounts.dat.bad");
}

}  // namespace

accounts::Shelf& shelf() {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (!s.read || now - s.lookedAt >= kLookEveryMs) {
        s.lookedAt = now;
        if (!s.read || writeTime(filePath()) != s.stamp) load(s);  // another game window changed it, or forgot it
        s.read = true;
    }
    return s.shelf;
}

bool save() {
    State& s = state();
    const std::wstring dir = folder();
    if (dir.empty()) return false;
    CreateDirectoryW(dir.c_str(), nullptr);
    std::string words = accounts::toJson(s.shelf), blob;
    const bool sealed = dpapi::protect(words, blob);
    SecureZeroMemory(words.data(), words.size());
    if (!sealed || !writeAll(filePath(), blob)) {
        LOG_ERROR("login: the accounts could not be saved (%s)",
                  sealed ? "not written" : "Windows did not protect them");
        return false;
    }
    s.stamp = writeTime(filePath());
    s.read = true;
    return true;
}

bool forget() {
    State& s = state();
    s.shelf = accounts::Shelf{};
    s.stamp = 0;
    s.read = true;
    const std::wstring path = filePath();
    return !path.empty() && (DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND);
}

}  // namespace uagfx::win::accountsstore
