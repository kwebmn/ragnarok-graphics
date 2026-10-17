#include "win/paths.hpp"

namespace uagfx::win {

std::string modulePath(HMODULE m) {
    if (!m) return "";
    char path[MAX_PATH];
    const DWORD n = GetModuleFileNameA(m, path, MAX_PATH);
    return n && n < MAX_PATH ? std::string(path, n) : "";
}

std::string gameDir() {
    const std::string exe = modulePath(GetModuleHandleA(nullptr));
    const size_t slash = exe.find_last_of("\\/");
    return slash == std::string::npos ? "." : exe.substr(0, slash);
}

std::string systemDir() {
    char dir[MAX_PATH];
    const UINT n = GetSystemDirectoryA(dir, MAX_PATH);
    return n && n < MAX_PATH ? std::string(dir, n) : "C:\\Windows\\System32";
}

bool fileExists(const std::string& path) {
    const DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool readTextFile(const std::string& path, std::string& out) {
    // Shared for deleting too: the settings app replaces uaro-graphics.ini in one step (MoveFileEx), also while it is
    // read here (0.6.0 review M1).
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
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
    if (!ok) out.clear();  // not half a file: a caller that does not look at the answer reads nothing
    return ok;
}

bool writeFileAtomic(const std::string& path, const void* data, size_t size) {
    const std::string part = path + "." + std::to_string(GetCurrentProcessId()) + ".tmp";
    HANDLE f = CreateFileA(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    const bool ok = !size || (WriteFile(f, data, DWORD(size), &put, nullptr) && put == size);
    CloseHandle(f);
    if (ok && MoveFileExA(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
    DeleteFileA(part.c_str());
    return false;
}

bool writeFileAtomic(const std::string& path, const std::string& text) {
    return writeFileAtomic(path, text.data(), text.size());
}

bool editIniFile(const std::string& path, const std::vector<settings::IniEdit>& edits) {
    const auto stamp = [&](WIN32_FILE_ATTRIBUTE_DATA& a) {
        return GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a) != 0;
    };
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt) Sleep(20);
        WIN32_FILE_ATTRIBUTE_DATA before{}, after{};
        const bool existed = stamp(before);
        std::string text;
        if (existed && !readTextFile(path, text)) continue;  // being replaced this moment
        const std::string next = settings::applyIniEdits(text, edits);
        const bool exists = stamp(after);
        const bool same = exists == existed &&
                          (!exists || (CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime) == 0 &&
                                       before.nFileSizeLow == after.nFileSizeLow &&
                                       before.nFileSizeHigh == after.nFileSizeHigh));
        if (same) return writeFileAtomic(path, next);
    }
    return false;
}

}  // namespace uagfx::win
