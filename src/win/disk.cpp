#include "win/disk.hpp"

#include <windows.h>

#include <algorithm>

namespace uagfx::win {

bool WinDisk::isFile(const std::string& path) {
    const DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool WinDisk::isFolder(const std::string& path) {
    const DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool WinDisk::makeFolder(const std::string& path) { return CreateDirectoryA(path.c_str(), nullptr) || isFolder(path); }

bool WinDisk::move(const std::string& from, const std::string& to, bool replace) {
    return MoveFileExA(from.c_str(), to.c_str(), replace ? MOVEFILE_REPLACE_EXISTING : 0) != 0;
}

std::vector<std::string> WinDisk::names(const std::string& folder) {
    std::vector<std::string> out;
    WIN32_FIND_DATAA f;
    HANDLE find = FindFirstFileA((folder + "\\*").c_str(), &f);
    if (find == INVALID_HANDLE_VALUE) return out;
    do {
        const std::string name = f.cFileName;
        if (name != "." && name != "..") out.push_back(name);
    } while (FindNextFileA(find, &f));
    FindClose(find);
    std::sort(out.begin(), out.end());
    return out;
}

bool WinDisk::removeFile(const std::string& path) { return DeleteFileA(path.c_str()) != 0; }

bool WinDisk::removeEmptyFolder(const std::string& path) { return RemoveDirectoryA(path.c_str()) != 0; }

bool WinDisk::readStart(const std::string& path, size_t most, std::string& out) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    out.assign(most, '\0');
    DWORD got = 0;
    const bool ok = most == 0 || ReadFile(f, &out[0], DWORD(most), &got, nullptr);
    CloseHandle(f);
    out.resize(ok ? got : 0);
    return ok;
}

}  // namespace uagfx::win
