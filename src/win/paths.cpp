#include "win/paths.hpp"

#include <fstream>
#include <sstream>

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
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream s;
    s << f.rdbuf();
    out = s.str();
    return true;
}

}  // namespace uagfx::win
