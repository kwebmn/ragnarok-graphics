#include "app/files.hpp"

#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cstring>
#include <cwctype>

#include "app/text.hpp"

namespace uagfx::app {

std::wstring exeFolder() {
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path, n < MAX_PATH ? n : 0);
    const size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

bool readFile(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart < (LONGLONG(1) << 30);
    if (ok) {
        out.assign(size_t(size.QuadPart), '\0');
        DWORD got = 0;
        ok = out.empty() || (ReadFile(f, &out[0], DWORD(out.size()), &got, nullptr) && got == out.size());
    }
    CloseHandle(f);
    return ok;
}

bool writeFileAtomic(const std::wstring& path, const std::string& bytes) {
    const std::wstring temp = path + L".new";
    HANDLE f = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    const bool ok = (bytes.empty() || WriteFile(f, bytes.data(), DWORD(bytes.size()), &put, nullptr)) &&
                    put == bytes.size() && FlushFileBuffers(f);
    CloseHandle(f);
    if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str());
        return false;
    }
    return true;
}

bool fileExists(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dirExists(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<std::wstring> listFolder(const std::wstring& dir, bool folders) {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW d;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        const bool folder = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const std::wstring name = d.cFileName;
        if (folder == folders && name != L"." && name != L"..") out.push_back(name);
    } while (FindNextFileW(h, &d));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

bool makeFolder(const std::wstring& path) { return CreateDirectoryW(path.c_str(), nullptr) || dirExists(path); }

bool removeTree(const std::wstring& path) {
    for (const std::wstring& f : listFolder(path, false)) DeleteFileW((path + L"\\" + f).c_str());
    for (const std::wstring& d : listFolder(path, true)) removeTree(path + L"\\" + d);
    return RemoveDirectoryW(path.c_str()) != 0;
}

bool folderWritable(const std::wstring& dir) {
    const std::wstring probe = dir + L"\\uaro-graphics-write-test.tmp";
    HANDLE f = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    CloseHandle(f);
    return true;
}

bool underProgramFiles(const std::wstring& dir) {
    auto lower = [](std::wstring s) {
        for (wchar_t& ch : s) ch = wchar_t(std::towlower(ch));
        return s;
    };
    const std::wstring d = lower(dir) + L"\\";
    for (int csidl : {CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86}) {
        wchar_t path[MAX_PATH];
        if (SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, path) != S_OK) continue;
        const std::wstring p = lower(path) + L"\\";
        if (d.compare(0, p.size(), p) == 0) return true;
    }
    return false;
}

std::wstring timestampName() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t buf[32];
    wsprintfW(buf, L"%04d-%02d-%02d-%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return buf;
}

std::string regFileToUtf8(const std::string& bytes) {
    if (bytes.size() >= 2 && uint8_t(bytes[0]) == 0xFF && uint8_t(bytes[1]) == 0xFE) {
        std::wstring w((bytes.size() - 2) / 2, L'\0');
        if (!w.empty()) std::memcpy(&w[0], bytes.data() + 2, w.size() * 2);
        return utf8(w);
    }
    return bytes;
}

std::string utf8ToRegFile(const std::string& text) {
    const std::wstring w = wide(text);
    std::string out = "\xFF\xFE";
    out.append(reinterpret_cast<const char*>(w.data()), w.size() * 2);
    return out;
}

unsigned long launch(const std::wstring& file, const std::wstring& folder) {
    SHELLEXECUTEINFOW e{};
    e.cbSize = sizeof e;
    e.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    e.lpVerb = L"open";
    e.lpFile = file.c_str();
    e.lpDirectory = folder.c_str();
    e.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&e) ? 0 : GetLastError();
}

void openInShell(const std::wstring& path) {
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::string defaultIni() {
    HRSRC r = FindResourceW(nullptr, L"DEFAULT_INI", MAKEINTRESOURCEW(10));  // RT_RCDATA
    HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
    const void* p = g ? LockResource(g) : nullptr;
    return p ? std::string(static_cast<const char*>(p), SizeofResource(nullptr, r)) : std::string();
}

}  // namespace uagfx::app
