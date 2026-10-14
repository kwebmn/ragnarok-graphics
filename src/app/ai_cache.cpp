#include "app/ai_cache.hpp"

#include <windows.h>

#include <cstdio>
#include <unordered_set>

#include "core/grf/grf.hpp"

namespace uagfx::app {

AiCacheFile readAiCache(const std::wstring& path) {
    AiCacheFile f;
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return f;
    f.exists = true;
    f.bytes = uint64_t(a.nFileSizeHigh) << 32 | a.nFileSizeLow;
    grf::Archive archive;
    if (!archive.open(_wfopen(path.c_str(), L"rb"), false, f.error)) {
        if (f.error == "no file") f.error = "cannot open it (error " + std::to_string(GetLastError()) + ")";
        return f;
    }
    for (const std::string& name : archive.names()) f.entries.push_back({name, archive.find(name)->aligned});
    return f;
}

std::string rewriteAiCache(const std::wstring& path, const std::vector<std::string>& keep) {
    if (keep.empty()) return removeAiCache(path);
    const std::unordered_set<std::string> names(keep.begin(), keep.end());
    const std::wstring temp = path + L".new";
    std::string error;
    grf::Archive archive;
    if (!archive.open(_wfopen(path.c_str(), L"rb"), false, error)) return error;
    std::FILE* out = _wfopen(temp.c_str(), L"wb");
    if (!out) return "cannot create upscaled.grf.new (error " + std::to_string(GetLastError()) + ")";
    bool ok = archive.copyTo(out, [&](const std::string& n) { return names.count(n) > 0; }, error);
    ok = std::fclose(out) == 0 && ok;
    archive.close();
    if (ok && !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "cannot replace it (error " + std::to_string(GetLastError()) + ")";
        ok = false;
    }
    if (!ok) {
        DeleteFileW(temp.c_str());
        return error.empty() ? "cannot write upscaled.grf.new" : error;
    }
    return "";
}

std::string removeAiCache(const std::wstring& path) {
    if (DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return "";
    return "cannot delete it (error " + std::to_string(GetLastError()) + ")";
}

bool aiCacheInUse() {
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\uaro-graphics-upscaled-grf");
    if (!h) return false;
    CloseHandle(h);
    return true;
}

}  // namespace uagfx::app
