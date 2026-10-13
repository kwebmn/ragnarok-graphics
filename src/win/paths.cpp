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

}  // namespace uagfx::win
