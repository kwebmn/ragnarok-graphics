#include "studio/places.hpp"

#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>
#undef interface  // objbase.h's: pack::Choice has a member of that name

#include <cstdio>
#include <utility>
#include <vector>

#include "core/studio_link.hpp"
#include "core/studio_state.hpp"
#include "core/version.hpp"

namespace uagfx::studio {
namespace {

std::wstring folderOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

bool sameText(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), int(a.size()), b.c_str(), int(b.size()), TRUE) == CSTR_EQUAL;
}

int64_t unixOf(const FILETIME& t) {
    return int64_t((uint64_t(t.dwHighDateTime) << 32 | t.dwLowDateTime) / 10000000ull) - 11644473600ll;
}

std::wstring wideOf(const char* s) {
    std::wstring w;
    for (; *s; ++s) w += wchar_t(static_cast<unsigned char>(*s));
    return w;
}

}  // namespace

std::wstring exePath() {
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), DWORD(buf.size()));
        if (n < buf.size()) return std::wstring(buf.data(), n);
        buf.resize(buf.size() * 2);
    }
}

std::wstring studioRoot() {
    wchar_t local[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    std::wstring root = n && n < MAX_PATH ? std::wstring(local, n) : folderOf(exePath());
    return root + L"\\" + wideOf(kFolder);
}

std::wstring buildName() {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    uint32_t h = 2166136261u;  // FNV-1a over the exe's size and time: another build, another folder
    if (GetFileAttributesExW(exePath().c_str(), GetFileExInfoStandard, &d)) {
        const uint32_t parts[4] = {d.nFileSizeHigh, d.nFileSizeLow, d.ftLastWriteTime.dwHighDateTime,
                                   d.ftLastWriteTime.dwLowDateTime};
        for (uint32_t v : parts)
            for (int i = 0; i < 4; ++i) h = (h ^ ((v >> (8 * i)) & 0xFF)) * 16777619u;
    }
    char hex[16];
    std::snprintf(hex, sizeof hex, "%08x", h);
    return wideOf(kVersion) + L"-" + wideOf(hex);
}

std::wstring buildFolder() { return studioRoot() + L"\\" + buildName(); }

std::wstring linkFolder() { return studioRoot() + L"\\link"; }

bool makeFolder(const std::wstring& path) {
    const int r = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS || r == ERROR_FILE_EXISTS;
}

bool runningFromCopy() { return sameText(folderOf(exePath()), buildFolder()); }

std::wstring gameDirOf(const std::wstring& exe) {
    const std::wstring folder = folderOf(exe);
    const size_t slash = folder.find_last_of(L"\\/");
    if (slash == std::wstring::npos || !sameText(folder.substr(slash + 1), L"uaro-graphics")) return L"";
    return folder.substr(0, slash);
}

bool startCopy(const std::wstring& gameDir, std::wstring& error) {
    const std::wstring folder = buildFolder(), copy = folder + L"\\uaro-studio.exe";
    if (!makeFolder(folder)) {
        error = L"cannot make " + folder;
        return false;
    }
    // A copy of this build may run already (its file is held): it is this file, started again.
    if (GetFileAttributesW(copy.c_str()) == INVALID_FILE_ATTRIBUTES &&
        !CopyFileW(exePath().c_str(), copy.c_str(), TRUE)) {
        error = L"cannot copy the studio to " + copy;
        return false;
    }
    std::wstring line = L"\"" + copy + L"\" --game \"" + gameDir + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(copy.c_str(), &line[0], nullptr, nullptr, FALSE, 0, nullptr, folder.c_str(), &si, &pi)) {
        error = L"cannot start " + copy;
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void dropOldBuilds() {
    const std::wstring root = studioRoot(), running = buildName();
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW((root + L"\\*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::pair<std::string, int64_t>> builds;
    do {
        const std::wstring name = f.cFileName;
        // a build's folder: "<version>-<hex>", the version's digit first
        if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || name.empty() || name[0] < L'0' || name[0] > L'9' ||
            name.find(L'-') == std::wstring::npos)
            continue;
        std::string ascii;
        for (wchar_t c : name) ascii += char(c < 128 ? c : '?');
        builds.emplace_back(ascii, unixOf(f.ftLastWriteTime));
    } while (FindNextFileW(h, &f));
    FindClose(h);
    std::string runningAscii;
    for (wchar_t c : running) runningAscii += char(c < 128 ? c : '?');
    for (const std::string& gone : buildsToDrop(builds, runningAscii)) {
        std::wstring from = root + L"\\" + wideOf(gone.c_str()) + L'\0';  // double-nul ended
        SHFILEOPSTRUCTW op{};
        op.wFunc = FO_DELETE;
        op.pFrom = from.c_str();
        op.fFlags = FOF_NO_UI;
        SHFileOperationW(&op);  // a copy still running stays (its file is held)
    }
}

}  // namespace uagfx::studio
